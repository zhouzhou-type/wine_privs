/*
 * Plug and Play support for hid devices found through udev
 *
 * Copyright 2016 CodeWeavers, Aric Stewart
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif
#ifdef HAVE_LIBUDEV_H
# include <libudev.h>
#endif
#ifdef HAVE_LINUX_HIDRAW_H
# include <linux/hidraw.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#define NONAMELESSUNION

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include "ddk/wdm.h"
#include "ddk/hidtypes.h"
#include "wine/debug.h"
#include "wine/unicode.h"

#include "bus.h"
#include "winsvc.h"
#include "winuser.h"
#include "dbt.h"
#include "winreg.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(plugplay);

#ifdef HAVE_UDEV

WINE_DECLARE_DEBUG_CHANNEL(hid_report);

static struct udev *udev_context = NULL;
static DRIVER_OBJECT *udev_driver_obj = NULL;

static const WCHAR hidraw_busidW[] = {'H','I','D','R','A','W',0};

typedef struct _device_info_
{
	struct list entry;
	DWORD vid;
	DWORD pid;
	DWORD interfacenum;
	char *devpath;
	char *sysname;
	char *interfacename;
	char *serialnum;
	char *syspath;
}device_info_;

static struct list device_info_list = LIST_INIT(device_info_list);

#include "initguid.h"
DEFINE_GUID(GUID_DEVCLASS_HIDRAW, 0x3def44ad,0x242e,0x46e5,0x82,0x6d,0x70,0x72,0x13,0xf3,0xaa,0x81);

struct platform_private
{
    struct udev_device *udev_device;
    int device_fd;

    HANDLE report_thread;
    int control_pipe[2];
};

static inline struct platform_private *impl_from_DEVICE_OBJECT(DEVICE_OBJECT *device)
{
    return (struct platform_private *)get_platform_private(device);
}

static inline WCHAR *strdupAtoW(const char *src)
{
    WCHAR *dst;
    DWORD len;
    if (!src) return NULL;
    len = MultiByteToWideChar(CP_UNIXCP, 0, src, -1, NULL, 0);
    if ((dst = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR))))
        MultiByteToWideChar(CP_UNIXCP, 0, src, -1, dst, len);
    return dst;
}

static DWORD get_sysattr_dword(struct udev_device *dev, const char *sysattr, int base)
{
    const char *attr = udev_device_get_sysattr_value(dev, sysattr);
    if (!attr)
    {
        WARN("Could not get %s from device\n", sysattr);
        return 0;
    }
    return strtol(attr, NULL, base);
}

static WCHAR *get_sysattr_string(struct udev_device *dev, const char *sysattr)
{
    const char *attr = udev_device_get_sysattr_value(dev, sysattr);
    if (!attr)
    {
        WARN("Could not get %s from device\n", sysattr);
        return NULL;
    }
    return strdupAtoW(attr);
}

static int compare_platform_device(DEVICE_OBJECT *device, void *platform_dev)
{
    struct udev_device *dev1 = impl_from_DEVICE_OBJECT(device)->udev_device;
    struct udev_device *dev2 = platform_dev;
    return strcmp(udev_device_get_syspath(dev1), udev_device_get_syspath(dev2));
}

static NTSTATUS hidraw_get_reportdescriptor(DEVICE_OBJECT *device, BYTE *buffer, DWORD length, DWORD *out_length)
{
#ifdef HAVE_LINUX_HIDRAW_H
    struct hidraw_report_descriptor descriptor;
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);

    if (ioctl(private->device_fd, HIDIOCGRDESCSIZE, &descriptor.size) == -1)
    {
        WARN("ioctl(HIDIOCGRDESCSIZE) failed: %d %s\n", errno, strerror(errno));
        return STATUS_UNSUCCESSFUL;
    }

    *out_length = descriptor.size;

    if (length < descriptor.size)
        return STATUS_BUFFER_TOO_SMALL;
    if (!descriptor.size)
        return STATUS_SUCCESS;

    if (ioctl(private->device_fd, HIDIOCGRDESC, &descriptor) == -1)
    {
        WARN("ioctl(HIDIOCGRDESC) failed: %d %s\n", errno, strerror(errno));
        return STATUS_UNSUCCESSFUL;
    }

    memcpy(buffer, descriptor.value, descriptor.size);
    return STATUS_SUCCESS;
#else
    return STATUS_NOT_IMPLEMENTED;
#endif
}

static NTSTATUS hidraw_get_string(DEVICE_OBJECT *device, DWORD index, WCHAR *buffer, DWORD length)
{
    struct udev_device *usbdev;
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);
    WCHAR *str = NULL;

    usbdev = udev_device_get_parent_with_subsystem_devtype(private->udev_device, "usb", "usb_device");
    if (usbdev)
    {
        switch (index)
        {
            case HID_STRING_ID_IPRODUCT:
                str = get_sysattr_string(usbdev, "product");
                break;
            case HID_STRING_ID_IMANUFACTURER:
                str = get_sysattr_string(usbdev, "manufacturer");
                break;
            case HID_STRING_ID_ISERIALNUMBER:
                str = get_sysattr_string(usbdev, "serial");
                break;
            default:
                ERR("Unhandled string index %08x\n", index);
                return STATUS_NOT_IMPLEMENTED;
        }
    }
    else
    {
#ifdef HAVE_LINUX_HIDRAW_H
        switch (index)
        {
            case HID_STRING_ID_IPRODUCT:
            {
                char buf[MAX_PATH];
                if (ioctl(private->device_fd, HIDIOCGRAWNAME(MAX_PATH), buf) == -1)
                    WARN("ioctl(HIDIOCGRAWNAME) failed: %d %s\n", errno, strerror(errno));
                else
                    str = strdupAtoW(buf);
                break;
            }
            case HID_STRING_ID_IMANUFACTURER:
                break;
            case HID_STRING_ID_ISERIALNUMBER:
                break;
            default:
                ERR("Unhandled string index %08x\n", index);
                return STATUS_NOT_IMPLEMENTED;
        }
#else
        return STATUS_NOT_IMPLEMENTED;
#endif
    }

    if (!str)
    {
        if (!length) return STATUS_BUFFER_TOO_SMALL;
        buffer[0] = 0;
        return STATUS_SUCCESS;
    }

    if (length <= strlenW(str))
    {
        HeapFree(GetProcessHeap(), 0, str);
        return STATUS_BUFFER_TOO_SMALL;
    }

    strcpyW(buffer, str);
    HeapFree(GetProcessHeap(), 0, str);
    return STATUS_SUCCESS;
}

static DWORD CALLBACK device_report_thread(void *args)
{
    DEVICE_OBJECT *device = (DEVICE_OBJECT*)args;
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);
    struct pollfd plfds[2];

    plfds[0].fd = private->device_fd;
    plfds[0].events = POLLIN;
    plfds[0].revents = 0;
    plfds[1].fd = private->control_pipe[0];
    plfds[1].events = POLLIN;
    plfds[1].revents = 0;

    while (1)
    {
        int size;
        BYTE report_buffer[1024];

        if (poll(plfds, 2, -1) <= 0) continue;
        if (plfds[1].revents)
            break;
        size = read(plfds[0].fd, report_buffer, sizeof(report_buffer));
        if (size == -1)
            TRACE_(hid_report)("Read failed. Likely an unplugged device\n");
        else if (size == 0)
            TRACE_(hid_report)("Failed to read report\n");
        else
            process_hid_report(device, report_buffer, size);
    }
    return 0;
}

static NTSTATUS begin_report_processing(DEVICE_OBJECT *device)
{
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);

    if (private->report_thread)
        return STATUS_SUCCESS;

    if (pipe(private->control_pipe) != 0)
    {
        ERR("Control pipe creation failed\n");
        return STATUS_UNSUCCESSFUL;
    }

    private->report_thread = CreateThread(NULL, 0, device_report_thread, device, 0, NULL);
    if (!private->report_thread)
    {
        ERR("Unable to create device report thread\n");
        close(private->control_pipe[0]);
        close(private->control_pipe[1]);
        return STATUS_UNSUCCESSFUL;
    }
    else
        return STATUS_SUCCESS;
}

static NTSTATUS hidraw_set_output_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
    struct platform_private* ext = impl_from_DEVICE_OBJECT(device);
    ssize_t rc;

    if (id != 0)
        rc = write(ext->device_fd, report, length);
    else
    {
        BYTE report_buffer[1024];

        if (length + 1 > sizeof(report_buffer))
        {
            ERR("Output report buffer too small\n");
            return STATUS_UNSUCCESSFUL;
        }

        report_buffer[0] = 0;
        memcpy(&report_buffer[1], report, length);
        rc = write(ext->device_fd, report_buffer, length + 1);
    }
    if (rc > 0)
    {
        *written = rc;
        return STATUS_SUCCESS;
    }
    else
    {
        *written = 0;
        return STATUS_UNSUCCESSFUL;
    }
}

static NTSTATUS hidraw_get_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *read)
{
#if defined(HAVE_LINUX_HIDRAW_H) && defined(HIDIOCGFEATURE)
    int rc;
    struct platform_private* ext = impl_from_DEVICE_OBJECT(device);
    report[0] = id;
    length = min(length, 0x1fff);
    rc = ioctl(ext->device_fd, HIDIOCGFEATURE(length), report);
    if (rc >= 0)
    {
        *read = rc;
        return STATUS_SUCCESS;
    }
    else
    {
        *read = 0;
        return STATUS_UNSUCCESSFUL;
    }
#else
    *read = 0;
    return STATUS_NOT_IMPLEMENTED;
#endif
}

static NTSTATUS hidraw_set_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
#if defined(HAVE_LINUX_HIDRAW_H) && defined(HIDIOCSFEATURE)
    int rc;
    struct platform_private* ext = impl_from_DEVICE_OBJECT(device);
    BYTE *feature_buffer;
    BYTE buffer[1024];

    if (id == 0)
    {
        if (length + 1 > sizeof(buffer))
        {
            ERR("Output feature buffer too small\n");
            return STATUS_UNSUCCESSFUL;
        }
        buffer[0] = 0;
        memcpy(&buffer[1], report, length);
        feature_buffer = buffer;
        length = length + 1;
    }
    else
        feature_buffer = report;
    length = min(length, 0x1fff);
    rc = ioctl(ext->device_fd, HIDIOCSFEATURE(length), feature_buffer);
    if (rc >= 0)
    {
        *written = rc;
        return STATUS_SUCCESS;
    }
    else
    {
        *written = 0;
        return STATUS_UNSUCCESSFUL;
    }
#else
    *written = 0;
    return STATUS_NOT_IMPLEMENTED;
#endif
}

static const platform_vtbl hidraw_vtbl =
{
    compare_platform_device,
    hidraw_get_reportdescriptor,
    hidraw_get_string,
    begin_report_processing,
    hidraw_set_output_report,
    hidraw_get_feature_report,
    hidraw_set_feature_report,
};

static device_info_* getDeviceInfoFromCache(struct udev_device *dev)
{
	const char *syspath;
	device_info_ *device_info = NULL;
	device_info_ *ret = NULL;

	if(dev == NULL)
	{
		ERR("param wrong dev or usbdev is NULL\n");
		return NULL;
	}

	TRACE("dev=%p\n", dev);

	syspath = udev_device_get_syspath(dev);

	if(syspath == NULL)
		return NULL;

	LIST_FOR_EACH_ENTRY(device_info, &device_info_list, device_info_, entry)
	{
		if(lstrcmpA(device_info->syspath, syspath) == 0)
		{
			ret = device_info;
			break;
		}
	}

	if(ret!=NULL)
	{
		TRACE("Get Device Event Info From Cache: vid:%04x  pid:%04x  interfaceNum:%d  devpath:%s  sysname:%s interface:%s serial:%s syspath:%s\n",ret->vid,ret->pid,ret->interfacenum,ret->devpath,ret->sysname,ret->interfacename,ret->serialnum,ret->syspath);
	}

	return ret;
}
static char *strdupA( const char *str )
{
	char *ret = NULL;
	if (!str) return NULL;
	if ((ret = HeapAlloc( GetProcessHeap(), 0, strlen(str) + 1 )))
		strcpy(ret,str);
	return ret;
}
static device_info_* cacheDeviceInfo(struct udev_device *dev, struct udev_device *usbdev)
{
	char *devpath, *sysname, *interface, *serial, *syspath;
	int vid = -1,pid = -1,interfaceNum = -1;
	device_info_ *device_info = NULL;

	TRACE("dev=%p  usbdev=%p\n", dev, usbdev);

	if(dev == NULL || usbdev == NULL)
	{
		ERR("param wrong dev or usbdev is NULL\n");
		return NULL;
	}

	device_info = getDeviceInfoFromCache(dev);

	devpath = udev_device_get_devnode(usbdev);
	sysname = udev_device_get_sysname(dev);
	interface = udev_device_get_sysattr_value(dev, "interface");
	serial = udev_device_get_sysattr_value(usbdev,"serial");
	vid = get_sysattr_dword(usbdev,"idVendor",16);
	pid = get_sysattr_dword(usbdev,"idProduct",16);
	interfaceNum = get_sysattr_dword(dev, "bInterfaceNumber",16);
	syspath = udev_device_get_syspath(dev);

	if(syspath == NULL || devpath == NULL || sysname == NULL || vid == -1 || pid == -1)
	{
		ERR("udev get information not support cache: syspath == NULL || devpath == NULL || sysname == NULL || vid == 0 || pid == 0\n");
		return NULL;
	}

	TRACE("Cache Device Event Info: vid:%04x  pid:%04x  interfaceNum:%d  devpath:%s  sysname:%s interface:%s serial:%s syspath:%s\n",vid,pid,interfaceNum,devpath,sysname,interface,serial,syspath);

	if(device_info == NULL)
	{
		if(!(device_info = HeapAlloc(GetProcessHeap(),0,sizeof(*device_info))))
		{
			ERR("Alloc Heap Error\n");
			return NULL;
		}
	}

	device_info->vid = vid;
	device_info->pid = pid;
	device_info->interfacenum = interfaceNum;
	device_info->devpath = strdupA(devpath);
	device_info->sysname = strdupA(sysname);
	device_info->interfacename = strdupA(interface);
	device_info->serialnum = strdupA(serial);
	device_info->syspath = strdupA(syspath);

	list_add_tail(&device_info_list, &(device_info->entry));

	return device_info;
}

static void sendDeviceEventToPNP(DWORD dwEventType, device_info_ *dev_info)
{
    WCHAR plugplayW[] = {'P','l','u','g','P','l','a','y',0};
	SC_HANDLE scm, service;
	SERVICE_STATUS_PROCESS ssStatus;
	DWORD dwBytesNeeded;
	const char *devpath, *sysname, *interface, *serial, *syspath;
	int vid,pid,interfaceNum;
	TRACE("dwEventType=%04x device_info=%p\n",dwEventType, dev_info);
	if(dev_info == NULL)
	{
		ERR("device_info is NULL\n");
		return ;
	}

	scm = OpenSCManagerW(NULL,NULL,0);
	if(!scm)
	{
        WARN("failed to open SCM (%u)\n", GetLastError());
        return;
	}
    service = OpenServiceW(scm, plugplayW, SERVICE_ALL_ACCESS);
	if(!service)
	{
        WARN("failed to open PNP (%u)\n", GetLastError());
		CloseServiceHandle(scm);
        return;
	}
	if(!QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,
				(LPBYTE) &ssStatus, sizeof(SERVICE_STATUS_PROCESS),
				&dwBytesNeeded))
	{
		WARN("failed to query service status (%u)\n", GetLastError());
		CloseServiceHandle(service);
		CloseServiceHandle(scm);
		return;
	}
	if(ssStatus.dwCurrentState != SERVICE_RUNNING)
	{
		CloseServiceHandle(service);
		CloseServiceHandle(scm);
		return;
	}
    
	devpath = dev_info->devpath;
	sysname = dev_info->sysname;
	interface = dev_info->interfacename;
	serial = dev_info->serialnum;
	vid = dev_info->vid;
	pid = dev_info->pid;
	interfaceNum = dev_info->interfacenum;
    syspath = dev_info->syspath;

	TRACE("Send Device Event Info: vid:%04x  pid:%04x  interfaceNum:%d  devpath:%s  sysname:%s interface:%s serial:%s syspath:%s\n",vid,pid,interfaceNum,devpath,sysname,interface,serial,syspath);

	if(!ControlDeviceEventA(service,dwEventType,vid,pid,interfaceNum,devpath,sysname,interface,serial,syspath))
	{
		WARN("control device event failed(%u)\n", GetLastError());
	}
	CloseServiceHandle(service);
	CloseServiceHandle(scm);
}

static void try_add_device(struct udev_device *dev)
{
    DWORD vid = 0, pid = 0, version = 0,interfaceclass = 0;
    struct udev_device *usbdev;
    DEVICE_OBJECT *device = NULL;
    const char *devnode;
    WCHAR *serial = NULL;
	device_info_* dev_info = NULL;
    int fd;

    usbdev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
    if (usbdev)
    {
        vid     = get_sysattr_dword(usbdev, "idVendor", 16);
        pid     = get_sysattr_dword(usbdev, "idProduct", 16);
        version = get_sysattr_dword(usbdev, "version", 10);
        serial  = get_sysattr_string(usbdev, "serial");
    }

    if (!(devnode = udev_device_get_devnode(usbdev)))
        return;

    if ((fd = open(devnode, O_RDWR)) == -1)
    {
        WARN("Unable to open udev device %s: %s\n", debugstr_a(devnode), strerror(errno));
        return;
    }

    TRACE("Found udev device %s (vid %04x, pid %04x, version %u, serial %s)\n",
          debugstr_a(devnode), vid, pid, version, debugstr_w(serial));
	//Cache Device Event Info
	dev_info = cacheDeviceInfo(dev,usbdev);
    //Send Device Event to  PNP
	sendDeviceEventToPNP(DBT_DEVICEARRIVAL,dev_info);
	interfaceclass = get_sysattr_dword(dev,"bInterfaceClass",10);
	if(interfaceclass == 3)
	{
        device = bus_create_hid_device(udev_driver_obj, hidraw_busidW, vid, pid, version, 0, serial, FALSE,
                                       &GUID_DEVCLASS_HIDRAW, &hidraw_vtbl, sizeof(struct platform_private));
    }

    if (device)
    {
        struct platform_private *private = impl_from_DEVICE_OBJECT(device);
        private->udev_device = udev_device_ref(dev);
        private->device_fd = fd;
        IoInvalidateDeviceRelations(device, BusRelations);
    }
    else
    {
        WARN("Ignoring device %s with interfaceclass %d (hid = 3)\n", debugstr_a(devnode), interfaceclass);
        close(fd);
    }

    HeapFree(GetProcessHeap(), 0, serial);
}

static void try_remove_device(struct udev_device *dev)
{
    DEVICE_OBJECT *device = bus_find_hid_device(&hidraw_vtbl, dev);
    struct platform_private *private;
	device_info_ *dev_info = NULL;
	//Get Cached Device Info
	dev_info = getDeviceInfoFromCache(dev);
	//Send Device Event to PNP
	sendDeviceEventToPNP(DBT_DEVICEREMOVECOMPLETE, dev_info);
    if (!device) return;

    IoInvalidateDeviceRelations(device, RemovalRelations);

    private = impl_from_DEVICE_OBJECT(device);

    if (private->report_thread)
    {
        write(private->control_pipe[1], "q", 1);
        WaitForSingleObject(private->report_thread, INFINITE);
        close(private->control_pipe[0]);
        close(private->control_pipe[1]);
        CloseHandle(private->report_thread);
    }

    dev = private->udev_device;
    close(private->device_fd);
    bus_remove_hid_device(device);
    udev_device_unref(dev);
}

static void build_initial_deviceset(void)
{
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;

    enumerate = udev_enumerate_new(udev_context);
    if (!enumerate)
    {
        WARN("Unable to create udev enumeration object\n");
        return;
    }

    if (udev_enumerate_add_match_subsystem(enumerate, "usb") < 0)
        WARN("Failed to add subsystem 'hidraw' to enumeration\n");

    if (udev_enumerate_scan_devices(enumerate) < 0)
        WARN("Enumeration scan failed\n");

    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(dev_list_entry, devices)
    {
        struct udev_device *dev;
        const char *path;

        path = udev_list_entry_get_name(dev_list_entry);
        if ((dev = udev_device_new_from_syspath(udev_context, path)))
        {
            try_add_device(dev);
            udev_device_unref(dev);
        }
    }

    udev_enumerate_unref(enumerate);
}

static struct udev_monitor *create_monitor(struct pollfd *pfd)
{
    struct udev_monitor *monitor;

    monitor = udev_monitor_new_from_netlink(udev_context, "udev");
    if (!monitor)
    {
        WARN("Unable to get udev monitor object\n");
        return NULL;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(monitor, "usb", "usb_interface") < 0)
        WARN("Failed to add subsystem 'usb' to monitor\n");

    if (udev_monitor_enable_receiving(monitor) < 0)
        goto error;

    if ((pfd->fd = udev_monitor_get_fd(monitor)) >= 0)
    {
        pfd->events = POLLIN;
        return monitor;
    }

error:
    WARN("Failed to start monitoring\n");
    udev_monitor_unref(monitor);
    return NULL;
}

static void process_monitor_event(struct udev_monitor *monitor)
{
    struct udev_device *dev;
    const char *action;

    dev = udev_monitor_receive_device(monitor);
    if (!dev)
    {
        FIXME("Failed to get device that has changed\n");
        return;
    }

    action = udev_device_get_action(dev);
    TRACE("Received action %s for udev device %s\n", debugstr_a(action),
          debugstr_a(udev_device_get_devnode(dev)));

    if (!action)
        WARN("No action received\n");
    else if (strcmp(action, "add") == 0)
        try_add_device(dev);
    else if (strcmp(action, "remove") == 0)
        try_remove_device(dev);
    else
        WARN("Unhandled action %s\n", debugstr_a(action));

    udev_device_unref(dev);
}

static DWORD CALLBACK deviceloop_thread(void *args)
{
    struct udev_monitor *monitor;
    HANDLE init_done = args;
    struct pollfd pfd;

    monitor = create_monitor(&pfd);
    build_initial_deviceset();
    SetEvent(init_done);

    while (monitor)
    {
        if (poll(&pfd, 1, -1) <= 0) continue;
        process_monitor_event(monitor);
    }

    TRACE("Monitor thread exiting\n");
    if (monitor)
        udev_monitor_unref(monitor);
    return 0;
}

NTSTATUS WINAPI udev_driver_init(DRIVER_OBJECT *driver, UNICODE_STRING *registry_path)
{
    HANDLE events[2];
    DWORD result;

    TRACE("(%p, %s)\n", driver, debugstr_w(registry_path->Buffer));

    if (!(udev_context = udev_new()))
    {
        ERR("Can't create udev object\n");
        return STATUS_UNSUCCESSFUL;
    }

    udev_driver_obj = driver;
    driver->MajorFunction[IRP_MJ_PNP] = common_pnp_dispatch;
    driver->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = hid_internal_dispatch;

    if (!(events[0] = CreateEventW(NULL, TRUE, FALSE, NULL)))
        goto error;
    if (!(events[1] = CreateThread(NULL, 0, deviceloop_thread, events[0], 0, NULL)))
    {
        CloseHandle(events[0]);
        goto error;
    }

    result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
    CloseHandle(events[0]);
    CloseHandle(events[1]);
    if (result == WAIT_OBJECT_0)
    {
        TRACE("Initialization successful\n");
        return STATUS_SUCCESS;
    }

error:
    ERR("Failed to initialize udev device thread\n");
    udev_unref(udev_context);
    udev_context = NULL;
    udev_driver_obj = NULL;
    return STATUS_UNSUCCESSFUL;
}

#else

NTSTATUS WINAPI udev_driver_init(DRIVER_OBJECT *driver, UNICODE_STRING *registry_path)
{
    WARN("Wine was compiled without UDEV support\n");
    return STATUS_NOT_IMPLEMENTED;
}

#endif /* HAVE_UDEV */
