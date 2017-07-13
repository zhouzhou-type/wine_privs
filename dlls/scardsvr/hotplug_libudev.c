/*
 * MUSCLE SmartCard Development ( http://pcsclite.alioth.debian.org/pcsclite.html )
 *
 * Copyright (C) 2011
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 * Copyright (C) 2014
 *  Stefani Seibold <stefani@seibold.net>
 *
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief This provides a search API for hot plugable devices using libudev
 */

#include "config.h"
#define USE_UDEV 1
#define HAVE_LIBUDEV 1
#if defined(HAVE_LIBUDEV)&defined(USE_UDEV)

#define _GNU_SOURCE		/* for asprintf(3) */
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <pthread.h>
#include <libudev.h>
#include <poll.h>

#include "debuglog.h"
#include "parser.h"
#include "readerfactory.h"
#include "sys_generic.h"
#include "hotplug.h"
#include "utils.h"

#include "windef.h"
#include "winbase.h"
#include "winreg.h"

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
  (__extension__							      \
    ({ long int __result;						      \
       do __result = (long int) (expression);				      \
       while (__result == -1L && errno == EINTR);			      \
       __result; }))
#endif

#undef DEBUG_HOTPLUG

#define FALSE			0
#define TRUE			1

extern char Add_Interface_In_Name;
extern char Add_Serial_In_Name;

static pthread_t usbNotifyThread;
static int driverSize = -1;
static struct udev *Udev;


/**
 * keep track of drivers in a dynamically allocated array
 */
static struct _driverTracker
{
	unsigned int manuID;
	unsigned int productID;

	char *bundleName;
	char *libraryPath;
	char *readerName;
	char *CFBundleName;
} *driverTracker = NULL;
#define DRIVER_TRACKER_SIZE_STEP 10

/* The CCID driver already supports 176 readers.
 * We start with a big array size to avoid reallocation. */
#define DRIVER_TRACKER_INITIAL_SIZE 200

/**
 * keep track of PCSCLITE_MAX_READERS_CONTEXTS simultaneous readers
 */
static struct _readerTracker
{
	char *devpath;	/**< device name seen by udev */
	char *fullName;	/**< full reader name (including serial number) */
	char *sysname;	/**< sysfs path */
} readerTracker[PCSCLITE_MAX_READERS_CONTEXTS];


static LONG HPReadBundleValues(void)
{
	LONG rv;
	DIR *hpDir;
	struct dirent *currFP = NULL;
	char fullPath[FILENAME_MAX];
	char fullLibPath[FILENAME_MAX];
	int listCount = 0;

	hpDir = opendir(PCSCLITE_HP_DROPDIR);

	if (NULL == hpDir)
	{
		Log1(PCSC_LOG_ERROR, "Cannot open PC/SC drivers directory: " PCSCLITE_HP_DROPDIR);
		Log1(PCSC_LOG_ERROR, "Disabling USB support for pcscd.");
		return -1;
	}

	/* allocate a first array */
	driverSize = DRIVER_TRACKER_INITIAL_SIZE;
	driverTracker = calloc(driverSize, sizeof(*driverTracker));
	if (NULL == driverTracker)
	{
		Log1(PCSC_LOG_CRITICAL, "Not enough memory");
		(void)closedir(hpDir);
		return -1;
	}

#define GET_KEY(key, values) \
	rv = LTPBundleFindValueWithKey(&plist, key, values); \
	if (rv) \
	{ \
		Log2(PCSC_LOG_ERROR, "Value/Key not defined for " key " in %s", \
			fullPath); \
		continue; \
	}

	while ((currFP = readdir(hpDir)) != 0)
	{
		if (strstr(currFP->d_name, ".bundle") != 0)
		{
			unsigned int alias;
			list_t plist, *values;
			list_t *manuIDs, *productIDs, *readerNames;
			char *CFBundleName;
			char *libraryPath;

			/*
			 * The bundle exists - let's form a full path name and get the
			 * vendor and product ID's for this particular bundle
			 */
			(void)snprintf(fullPath, sizeof(fullPath), "%s/%s/Contents/Info.plist",
				PCSCLITE_HP_DROPDIR, currFP->d_name);
			fullPath[sizeof(fullPath) - 1] = '\0';

			rv = bundleParse(fullPath, &plist);
			if (rv)
				continue;

			/* get CFBundleExecutable */
			GET_KEY(PCSCLITE_HP_LIBRKEY_NAME, &values)
			libraryPath = list_get_at(values, 0);
			(void)snprintf(fullLibPath, sizeof(fullLibPath),
				"%s/%s/Contents/%s/%s",
				PCSCLITE_HP_DROPDIR, currFP->d_name, PCSC_ARCH,
				libraryPath);
			fullLibPath[sizeof(fullLibPath) - 1] = '\0';

			GET_KEY(PCSCLITE_HP_MANUKEY_NAME, &manuIDs)
			GET_KEY(PCSCLITE_HP_PRODKEY_NAME, &productIDs)
			GET_KEY(PCSCLITE_HP_NAMEKEY_NAME, &readerNames)

			if  ((list_size(manuIDs) != list_size(productIDs))
				|| (list_size(manuIDs) != list_size(readerNames)))
			{
				Log2(PCSC_LOG_CRITICAL, "Error parsing %s", fullPath);
				(void)closedir(hpDir);
				return -1;
			}

			/* Get CFBundleName */
			rv = LTPBundleFindValueWithKey(&plist, PCSCLITE_HP_CFBUNDLE_NAME,
				&values);
			if (rv)
				CFBundleName = NULL;
			else
				CFBundleName = strdup(list_get_at(values, 0));

			/* while we find a nth ifdVendorID in Info.plist */
			for (alias=0; alias<list_size(manuIDs); alias++)
			{
				char *value;

				/* variables entries */
				value = list_get_at(manuIDs, alias);
				driverTracker[listCount].manuID = strtol(value, NULL, 16);

				value = list_get_at(productIDs, alias);
				driverTracker[listCount].productID = strtol(value, NULL, 16);

				driverTracker[listCount].readerName = strdup(list_get_at(readerNames, alias));

				/* constant entries for a same driver */
				driverTracker[listCount].bundleName = strdup(currFP->d_name);
				driverTracker[listCount].libraryPath = strdup(fullLibPath);
				driverTracker[listCount].CFBundleName = CFBundleName;

#ifdef DEBUG_HOTPLUG
				Log2(PCSC_LOG_INFO, "Found driver for: %s",
					driverTracker[listCount].readerName);
#endif
				listCount++;
				if (listCount >= driverSize)
				{
					int i;

					/* increase the array size */
					driverSize += DRIVER_TRACKER_SIZE_STEP;
#ifdef DEBUG_HOTPLUG
					Log2(PCSC_LOG_INFO,
						"Increase driverTracker to %d entries", driverSize);
#endif
					driverTracker = realloc(driverTracker,
						driverSize * sizeof(*driverTracker));
					if (NULL == driverTracker)
					{
						Log1(PCSC_LOG_CRITICAL, "Not enough memory");
						driverSize = -1;
						(void)closedir(hpDir);
						return -1;
					}

					/* clean the newly allocated entries */
					for (i=driverSize-DRIVER_TRACKER_SIZE_STEP; i<driverSize; i++)
					{
						driverTracker[i].manuID = 0;
						driverTracker[i].productID = 0;
						driverTracker[i].bundleName = NULL;
						driverTracker[i].libraryPath = NULL;
						driverTracker[i].readerName = NULL;
						driverTracker[i].CFBundleName = NULL;
					}
				}
			}
			bundleRelease(&plist);
		}
	}

	driverSize = listCount;
	(void)closedir(hpDir);

#ifdef DEBUG_HOTPLUG
	Log2(PCSC_LOG_INFO, "Found drivers for %d readers", listCount);
#endif

	return 0;
} /* HPReadBundleValues */


/*@null@*/ static struct _driverTracker *get_driver(struct udev_device *dev,
	const char *devpath, struct _driverTracker **classdriver)
{
	int i;
	unsigned int idVendor, idProduct;
	static struct _driverTracker *driver;
	const char *str;

	str = udev_device_get_sysattr_value(dev, "idVendor");
	if (!str)
	{
		Log1(PCSC_LOG_ERROR, "udev_device_get_sysattr_value() failed");
		return NULL;
	}
	idVendor = strtol(str, NULL, 16);

	str = udev_device_get_sysattr_value(dev, "idProduct");
	if (!str)
	{
		Log1(PCSC_LOG_ERROR, "udev_device_get_sysattr_value() failed");
		return NULL;
	}
	idProduct = strtol(str, NULL, 16);

#ifdef NO_LOG
	(void)devpath;
#endif
	Log4(PCSC_LOG_DEBUG,
		"Looking for a driver for VID: 0x%04X, PID: 0x%04X, path: %s",
		idVendor, idProduct, devpath);

	*classdriver = NULL;
	driver = NULL;
	/* check if the device is supported by one driver */
	for (i=0; i<driverSize; i++)
	{
		if (driverTracker[i].libraryPath != NULL &&
			idVendor == driverTracker[i].manuID &&
			idProduct == driverTracker[i].productID)
		{
			if ((driverTracker[i].CFBundleName != NULL)
				&& (0 == strcmp(driverTracker[i].CFBundleName, "CCIDCLASSDRIVER")))
				*classdriver = &driverTracker[i];
			else
				/* it is not a CCID Class driver */
				driver = &driverTracker[i];
		}
	}

	/* if we found a specific driver */
	if (driver)
		return driver;

	/* else return the Class driver (if any) */
	return *classdriver;
}


static void HPRemoveDevice(struct udev_device *dev)
{
	int i;
	const char *devpath;
	struct udev_device *parent;
	const char *sysname;

	/* The device pointed to by dev contains information about
	   the interface. In order to get information about the USB
	   device, get the parent device with the subsystem/devtype pair
	   of "usb"/"usb_device". This will be several levels up the
	   tree, but the function will find it.*/
	parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb",
		"usb_device");
	if (!parent)
		return;

	devpath = udev_device_get_devnode(parent);
	if (!devpath)
	{
		/* the device disapeared? */
		Log1(PCSC_LOG_ERROR, "udev_device_get_devnode() failed");
		return;
	}

	sysname = udev_device_get_sysname(dev);
	if (!sysname)
	{
		Log1(PCSC_LOG_ERROR, "udev_device_get_sysname() failed");
		return;
	}

	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (readerTracker[i].fullName && !strcmp(sysname, readerTracker[i].sysname))
		{
			Log4(PCSC_LOG_INFO, "Removing USB device[%d]: %s at %s", i,
				readerTracker[i].fullName, readerTracker[i].devpath);

			RFRemoveReader(readerTracker[i].fullName, PCSCLITE_HP_BASE_PORT + i);

			free(readerTracker[i].devpath);
			readerTracker[i].devpath = NULL;
			free(readerTracker[i].fullName);
			readerTracker[i].fullName = NULL;
			free(readerTracker[i].sysname);
			readerTracker[i].sysname = NULL;
			break;
		}
	}
}


static void HPAddDevice(struct udev_device *dev)
{
	int index, a;
	char *deviceName = NULL;
	char *fullname = NULL;
	struct _driverTracker *driver, *classdriver;
	const char *sSerialNumber = NULL, *sInterfaceName = NULL;
	const char *sInterfaceNumber;
	LONG ret;
	int bInterfaceNumber;
	const char *devpath;
	struct udev_device *parent;
	const char *sysname;

	/* The device pointed to by dev contains information about
	   the interface. In order to get information about the USB
	   device, get the parent device with the subsystem/devtype pair
	   of "usb"/"usb_device". This will be several levels up the
	   tree, but the function will find it.*/
	parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb",
		"usb_device");
	if (!parent)
		return;

	devpath = udev_device_get_devnode(parent);
	if (!devpath)
	{
		/* the device disapeared? */
		Log1(PCSC_LOG_ERROR, "udev_device_get_devnode() failed");
		return;
	}

	driver = get_driver(parent, devpath, &classdriver);
	if (NULL == driver)
	{
		/* not a smart card reader */
#ifdef DEBUG_HOTPLUG
		Log2(PCSC_LOG_DEBUG, "%s is not a supported smart card reader",
			devpath);
#endif
		return;
	}

	sysname = udev_device_get_sysname(dev);
	if (!sysname)
	{
		Log1(PCSC_LOG_ERROR, "udev_device_get_sysname() failed");
		return;
	}

	/* check for duplicated add */
	for (index=0; index<PCSCLITE_MAX_READERS_CONTEXTS; index++)
	{
		if (readerTracker[index].fullName && !strcmp(sysname, readerTracker[index].sysname))
			return;
	}

	Log2(PCSC_LOG_INFO, "Adding USB device: %s", driver->readerName);

	sInterfaceNumber = udev_device_get_sysattr_value(dev, "bInterfaceNumber");
	if (sInterfaceNumber)
		bInterfaceNumber = atoi(sInterfaceNumber);
	else
		bInterfaceNumber = 0;

	a = asprintf(&deviceName, "usb:%04x/%04x:libudev:%d:%s",
		driver->manuID, driver->productID, bInterfaceNumber, devpath);
	if (-1 ==  a)
	{
		Log1(PCSC_LOG_ERROR, "asprintf() failed");
		return;
	}

	/* find a free entry */
	for (index=0; index<PCSCLITE_MAX_READERS_CONTEXTS; index++)
	{
		if (NULL == readerTracker[index].fullName)
			break;
	}

	if (PCSCLITE_MAX_READERS_CONTEXTS == index)
	{
		Log2(PCSC_LOG_ERROR,
			"Not enough reader entries. Already found %d readers", index);
		return;
	}

	if (Add_Interface_In_Name)
		sInterfaceName = udev_device_get_sysattr_value(dev, "interface");

	if (Add_Serial_In_Name)
		sSerialNumber = udev_device_get_sysattr_value(parent, "serial");

	/* name from the Info.plist file */
	fullname = strdup(driver->readerName);

	/* interface name from the device (if any) */
	if (sInterfaceName)
	{
		char *result;

		/* create a new name */
		a = asprintf(&result, "%s [%s]", fullname, sInterfaceName);
		if (-1 ==  a)
		{
			Log1(PCSC_LOG_ERROR, "asprintf() failed");
			goto exit;
		}

		free(fullname);
		fullname = result;
	}

	/* serial number from the device (if any) */
	if (sSerialNumber)
	{
		/* only add the serial number if it is not already present in the
		 * interface name */
		if (!sInterfaceName || NULL == strstr(sInterfaceName, sSerialNumber))
		{
			char *result;

			/* create a new name */
			a = asprintf(&result, "%s (%s)", fullname, sSerialNumber);
			if (-1 ==  a)
			{
				Log1(PCSC_LOG_ERROR, "asprintf() failed");
				goto exit;
			}

			free(fullname);
			fullname = result;
		}
	}

	readerTracker[index].fullName = strdup(fullname);
	readerTracker[index].devpath = strdup(devpath);
	readerTracker[index].sysname = strdup(sysname);

	ret = RFAddReader(fullname, PCSCLITE_HP_BASE_PORT + index,
		driver->libraryPath, deviceName);
	if ((SCARD_S_SUCCESS != ret) && (SCARD_E_UNKNOWN_READER != ret))
	{
		Log2(PCSC_LOG_ERROR, "Failed adding USB device: %s",
			driver->readerName);

		if (classdriver && driver != classdriver)
		{
			/* the reader can also be used by the a class driver */
			ret = RFAddReader(fullname, PCSCLITE_HP_BASE_PORT + index,
				classdriver->libraryPath, deviceName);
			if ((SCARD_S_SUCCESS != ret) && (SCARD_E_UNKNOWN_READER != ret))
			{
				Log2(PCSC_LOG_ERROR, "Failed adding USB device: %s",
						driver->readerName);
				(void)CheckForOpenCT();
			}
		}
		else
		{
			(void)CheckForOpenCT();
		}
	}

	if (SCARD_S_SUCCESS != ret)
	{
		/* adding the reader failed */
		free(readerTracker[index].devpath);
		readerTracker[index].devpath = NULL;
		free(readerTracker[index].fullName);
		readerTracker[index].fullName = NULL;
		free(readerTracker[index].sysname);
		readerTracker[index].sysname = NULL;
	}

exit:
	free(fullname);
	free(deviceName);
} /* HPAddDevice */


static void HPScanUSB(struct udev *udev)
{
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;

	/* Create a list of the devices in the 'usb' subsystem. */
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "usb");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	/* For each item enumerated */
	udev_list_entry_foreach(dev_list_entry, devices)
	{
		struct udev_device *dev;
		const char *devpath;

		/* Get the filename of the /sys entry for the device
		   and create a udev_device object (dev) representing it */
		devpath = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, devpath);

#ifdef DEBUG_HOTPLUG
		Log2(PCSC_LOG_DEBUG, "Found matching USB device: %s", devpath);
#endif
		HPAddDevice(dev);

		/* free device */
		udev_device_unref(dev);
	}

	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);
}


static void HPEstablishUSBNotifications(void *arg)
{
	struct udev_monitor *udev_monitor = arg;
	int r;
	int fd;
	struct pollfd pfd;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	/* udev monitor file descriptor */
	fd = udev_monitor_get_fd(udev_monitor);
	if (fd < 0)
	{
		Log2(PCSC_LOG_ERROR, "udev_monitor_get_fd() error: %d", fd);
		pthread_exit(NULL);
	}

	pfd.fd = fd;
	pfd.events = POLLIN;

	for (;;)
	{
		struct udev_device *dev;

#ifdef DEBUG_HOTPLUG
		Log0(PCSC_LOG_INFO);
#endif
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		/* wait for a udev event */
		r = TEMP_FAILURE_RETRY(poll(&pfd, 1, -1));
		if (r < 0)
		{
			Log2(PCSC_LOG_ERROR, "select(): %s", strerror(errno));
			pthread_exit(NULL);
		}

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		dev = udev_monitor_receive_device(udev_monitor);
		if (dev)
		{
			const char *action = udev_device_get_action(dev);

			if (action)
			{
				if (!strcmp("remove", action))
				{
					Log1(PCSC_LOG_INFO, "USB Device removed");
					HPRemoveDevice(dev);
				}
				else
				if (!strcmp("add", action))
				{
					Log1(PCSC_LOG_INFO, "USB Device add");
					HPAddDevice(dev);
				}
			}

			/* free device */
			udev_device_unref(dev);
		}
	}

	pthread_exit(NULL);
} /* HPEstablishUSBNotifications */


/***
 * Start a thread waiting for hotplug events
 */
LONG HPSearchHotPluggables(void)
{
	int i;

	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		readerTracker[i].devpath = NULL;
		readerTracker[i].fullName = NULL;
		readerTracker[i].sysname = NULL;
	}

	return HPReadBundleValues();
} /* HPSearchHotPluggables */


/**
 * Stop the hotplug thread
 */
LONG HPStopHotPluggables(void)
{
	int i;

	if (driverSize <= 0)
		return 0;

	if (!Udev)
		return 0;

	pthread_cancel(usbNotifyThread);
	pthread_join(usbNotifyThread, NULL);

	for (i=0; i<driverSize; i++)
	{
		/* free strings allocated by strdup() */
		free(driverTracker[i].bundleName);
		free(driverTracker[i].libraryPath);
		free(driverTracker[i].readerName);
	}
	free(driverTracker);

	udev_unref(Udev);

	Udev = NULL;
	driverSize = -1;

	Log1(PCSC_LOG_INFO, "Hotplug stopped");
	return 0;
} /* HPStopHotPluggables */

/**
 * Sets up callbacks for device hotplug events.
 */
ULONG HPRegisterForHotplugEvents_Origin(void)
{
	struct udev_monitor *udev_monitor;
	int r;

	if (driverSize <= 0)
	{
		Log1(PCSC_LOG_INFO, "No bundle files in pcsc drivers directory: "
			PCSCLITE_HP_DROPDIR);
		Log1(PCSC_LOG_INFO, "Disabling USB support for pcscd");
		return 0;
	}

	/* Create the udev object */
	Udev = udev_new();
	if (!Udev)
	{
		Log1(PCSC_LOG_ERROR, "udev_new() failed");
		return SCARD_F_INTERNAL_ERROR;
	}

	udev_monitor = udev_monitor_new_from_netlink(Udev, "udev");
	if (NULL == udev_monitor)
	{
		Log1(PCSC_LOG_ERROR, "udev_monitor_new_from_netlink() error");
		pthread_exit(NULL);
	}

	/* filter only the interfaces */
	r = udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "usb",
		"usb_interface");
	if (r)
	{
		Log2(PCSC_LOG_ERROR, "udev_monitor_filter_add_match_subsystem_devtype() error: %d\n", r);
		pthread_exit(NULL);
	}

	r = udev_monitor_enable_receiving(udev_monitor);
	if (r)
	{
		Log2(PCSC_LOG_ERROR, "udev_monitor_enable_receiving() error: %d\n", r);
		pthread_exit(NULL);
	}

	/* scan the USB bus at least once before accepting client connections */
	HPScanUSB(Udev);
/*
	if (ThreadCreate(&usbNotifyThread, 0,
		(PCSCLITE_THREAD_FUNCTION( )) HPEstablishUSBNotifications, udev_monitor))
	{
		Log1(PCSC_LOG_ERROR, "ThreadCreate() failed");
		return SCARD_F_INTERNAL_ERROR;
	}
*/
	return 0;
} /* HPRegisterForHotplugEvents */

static void ScanUSBFromReg(void);
ULONG HPRegisterForHotplugEvents(void)
{
	if (driverSize <= 0)
	{
		Log1(PCSC_LOG_INFO, "No bundle files in pcsc drivers directory: "
			PCSCLITE_HP_DROPDIR);
		Log1(PCSC_LOG_INFO, "Disabling USB support for pcscd");
		return 0;
	}

	/* scan the USB bus at least once before accepting client connections */
	ScanUSBFromReg();
	return 0;
} /* HPRegisterForHotplugEvents */


void HPReCheckSerialReaders(void)
{
	/* nothing to do here */
#ifdef DEBUG_HOTPLUG
	Log0(PCSC_LOG_ERROR);
#endif
} /* HPReCheckSerialReaders */

static struct _driverTracker *libudev_get_driver(unsigned int idVendor, unsigned int idProduct,
		struct _driverTracker **classdriver)
{
	int i;
	static struct _driverTracker *driver;
	*classdriver = NULL;
	driver = NULL;

	/* check if the device is supported by one driver */
	for (i=0; i<driverSize; i++)
	{
		if (driverTracker[i].libraryPath != NULL &&
			idVendor == driverTracker[i].manuID &&
			idProduct == driverTracker[i].productID)
		{
			if ((driverTracker[i].CFBundleName != NULL)
				&& (0 == strcmp(driverTracker[i].CFBundleName, "CCIDCLASSDRIVER")))
				*classdriver = &driverTracker[i];
			else
				/* it is not a CCID Class driver */
				driver = &driverTracker[i];
		}
	}

	/* if we found a specific driver */
	if (driver)
		return driver;

	/* else return the Class driver (if any) */
	return *classdriver;
}

void libudev_add_device(PDEV_BROADCAST_WINE dbcw)
{
	int index, a;
	struct _driverTracker *driver, *classdriver;
	char *devpath = NULL, *sysname = NULL, *sSerialNumber = NULL, *sInterfaceName = NULL, *deviceName = NULL, *fullname = NULL;
	int bInterfaceNumber, vid, pid;
	LONG ret;

	if(dbcw->dbcw_devpath == NULL)
	{
		Log1(PCSC_LOG_ERROR, "devpath param NULL");
		return ;
	}else
		devpath = dbcw->dbcw_devpath;
	if(dbcw->dbcw_sysname == NULL)
	{
		Log1(PCSC_LOG_ERROR,"sysname param NULL");
		return ;
	}else
		sysname = dbcw->dbcw_sysname;

	bInterfaceNumber = dbcw->dbcw_interfacenum;
	vid = dbcw->dbcw_vid;
	pid = dbcw->dbcw_pid;

	if(Add_Interface_In_Name)
		sInterfaceName = dbcw->dbcw_interfacename;
	if(Add_Serial_In_Name)
		sSerialNumber = dbcw->dbcw_serialnum;

	Log4(PCSC_LOG_INFO,"SCardSvr add device vid = %04x pid = %04x interfacenumber= %d", vid,pid,bInterfaceNumber);
	Log3(PCSC_LOG_INFO,"SCardSvr add_device devpath = %s sysname = %s", devpath,sysname);
	Log3(PCSC_LOG_INFO,"SCardSvr add_device interfacename = %s serial = %s", sInterfaceName,sSerialNumber);

	driver = libudev_get_driver(vid,pid,&classdriver);
	if(NULL == driver)
	{
		Log2(PCSC_LOG_DEBUG, "%s is not a supported smart card reader", devpath);
		return;
	}
	/* check for duplicated add */
	for (index=0; index<PCSCLITE_MAX_READERS_CONTEXTS; index++)
	{
		if (readerTracker[index].fullName && !strcmp(sysname, readerTracker[index].sysname))
			return;
	}

	Log2(PCSC_LOG_INFO, "Adding USB device: %s", driver->readerName);
    
	a = asprintf(&deviceName, "usb:%04x/%04x:libudev:%d:%s",
		driver->manuID, driver->productID, bInterfaceNumber, devpath);
	if (-1 ==  a)
	{
		Log1(PCSC_LOG_ERROR, "asprintf() failed");
		return;
	}

	/* find a free entry */
	for (index=0; index<PCSCLITE_MAX_READERS_CONTEXTS; index++)
	{
		if (NULL == readerTracker[index].fullName)
			break;
	}

	if (PCSCLITE_MAX_READERS_CONTEXTS == index)
	{
		Log2(PCSC_LOG_ERROR,
			"Not enough reader entries. Already found %d readers", index);
		return;
	}

	fullname = strdup(driver->readerName);

	/* interface name from the device (if any) */
	if (sInterfaceName)
	{
		char *result;

		/* create a new name */
		a = asprintf(&result, "%s [%s]", fullname, sInterfaceName);
		if (-1 ==  a)
		{
			Log1(PCSC_LOG_ERROR, "asprintf() failed");
			goto exit;
		}

		free(fullname);
		fullname = result;
	}

	/* serial number from the device (if any) */
	if (sSerialNumber)
	{
		/* only add the serial number if it is not already present in the
		 * interface name */
		if (!sInterfaceName || NULL == strstr(sInterfaceName, sSerialNumber))
		{
			char *result;

			/* create a new name */
			a = asprintf(&result, "%s (%s)", fullname, sSerialNumber);
			if (-1 ==  a)
			{
				Log1(PCSC_LOG_ERROR, "asprintf() failed");
				goto exit;
			}

			free(fullname);
			fullname = result;
		}
	}

	readerTracker[index].fullName = strdup(fullname);
	readerTracker[index].devpath = strdup(devpath);
	readerTracker[index].sysname = strdup(sysname);

	ret = RFAddReader(fullname, PCSCLITE_HP_BASE_PORT + index,
		driver->libraryPath, deviceName);
	if ((SCARD_S_SUCCESS != ret) && (SCARD_E_UNKNOWN_READER != ret))
	{
		Log2(PCSC_LOG_ERROR, "Failed adding USB device: %s",
			driver->readerName);

		if (classdriver && driver != classdriver)
		{
			/* the reader can also be used by the a class driver */
			ret = RFAddReader(fullname, PCSCLITE_HP_BASE_PORT + index,
				classdriver->libraryPath, deviceName);
			if ((SCARD_S_SUCCESS != ret) && (SCARD_E_UNKNOWN_READER != ret))
			{
				Log2(PCSC_LOG_ERROR, "Failed adding USB device: %s",
						driver->readerName);
				(void)CheckForOpenCT();
			}
		}
		else
		{
			(void)CheckForOpenCT();
		}
	}

	if (SCARD_S_SUCCESS != ret)
	{
		/* adding the reader failed */
		free(readerTracker[index].devpath);
		readerTracker[index].devpath = NULL;
		free(readerTracker[index].fullName);
		readerTracker[index].fullName = NULL;
		free(readerTracker[index].sysname);
		readerTracker[index].sysname = NULL;
	}

exit:
	free(fullname);
	free(deviceName);
} /* libudev_add_device */

void libudev_remove_device(PDEV_BROADCAST_WINE dbcw)
{
	int i;
	char *devpath = NULL, *sysname = NULL;
	if(dbcw->dbcw_devpath == NULL)
	{
		Log1(PCSC_LOG_ERROR, "devpath param NULL");
		return ;
	}else
	{
		devpath = dbcw->dbcw_devpath;
	}
	if(dbcw->dbcw_sysname == NULL)
	{
		Log1(PCSC_LOG_ERROR,"sysname param NULL");
		return ;
	}else
	{
		sysname = dbcw->dbcw_sysname;
	}

	Log3(PCSC_LOG_INFO,"SCardSvr remove_device devpath = %s sysname = %s", devpath,sysname);

	for (i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (readerTracker[i].fullName && !strcmp(sysname, readerTracker[i].sysname))
		{
			Log4(PCSC_LOG_INFO, "Removing USB device[%d]: %s at %s", i,
				readerTracker[i].fullName, readerTracker[i].devpath);

			RFRemoveReader(readerTracker[i].fullName, PCSCLITE_HP_BASE_PORT + i);

			free(readerTracker[i].devpath);
			readerTracker[i].devpath = NULL;
			free(readerTracker[i].fullName);
			readerTracker[i].fullName = NULL;
			free(readerTracker[i].sysname);
			readerTracker[i].sysname = NULL;
			break;
		}
	}
}

static char *strdupA( const char *str )
{
	char *ret = NULL;
	if (!str) return NULL;
	if ((ret = HeapAlloc( GetProcessHeap(), 0, strlen(str) + 1 )))
		strcpy(ret,str);
	return ret;
}
static void ScanUSBFromReg(void)
{
	const char *Enum = "System\\CurrentControlSet\\Enum\\USB";
	HKEY usbKey, vpKey, sysnameKey;
	char *vpKeyName, *sysnameKeyName; 
	DWORD vpKeyNameMaxLen = -1, sysnameKeyNameMaxLen = -1;
	DWORD vpKeyNameLen = -1, sysnameKeyNameLen = -1;
	int vpIndex = 0, sysIndex = 0;
	DWORD maxValue = -1, maxCount = -1;
	DWORD type;

	char *devpath, *sysname, *interface, *serial;
	int vid = -1,pid = -1,interfaceNum = -1,exists;
	DEV_BROADCAST_WINE dbcw;

	Log1(PCSC_LOG_INFO,"Start");

	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,Enum,0,KEY_READ,&usbKey) != ERROR_SUCCESS)
	{
		Log2(PCSC_LOG_INFO,"NO reg key:HLM\\%s",Enum);
		return ;
	}
	if(RegQueryInfoKeyA(usbKey,NULL,NULL,NULL,NULL,&vpKeyNameMaxLen,NULL,NULL,NULL,NULL,NULL,NULL) != ERROR_SUCCESS)
		return;
	vpKeyNameMaxLen++;
	vpKeyNameLen = vpKeyNameMaxLen;
	vpKeyName = HeapAlloc(GetProcessHeap(),0,vpKeyNameMaxLen);
	//Key: VID_xxxx&PID_xxxx
	for(vpIndex = 0; RegEnumKeyExA(usbKey, vpIndex, vpKeyName, &vpKeyNameLen,NULL,NULL,NULL,NULL) != ERROR_NO_MORE_ITEMS; ++vpIndex, vpKeyNameLen = vpKeyNameMaxLen)
	{
		if(RegOpenKeyExA(usbKey, vpKeyName, 0, KEY_READ, &vpKey) != ERROR_SUCCESS)
			continue;
		if(RegQueryInfoKeyA(vpKey,NULL,NULL,NULL,NULL,&sysnameKeyNameMaxLen,NULL,NULL,NULL,NULL,NULL,NULL) != ERROR_SUCCESS)
			continue;
		sysnameKeyNameMaxLen++;
		sysnameKeyNameLen = sysnameKeyNameMaxLen;
		sysnameKeyName = HeapAlloc(GetProcessHeap(),0,sysnameKeyNameMaxLen);
		//Key: sysname
		for(sysIndex = 0; RegEnumKeyExA(vpKey, sysIndex, sysnameKeyName, &sysnameKeyNameLen,NULL,NULL,NULL,NULL) != ERROR_NO_MORE_ITEMS; ++sysIndex, sysnameKeyNameLen = sysnameKeyNameMaxLen)
		{
			if(RegOpenKeyExA(vpKey,sysnameKeyName,0,KEY_READ, &sysnameKey) != ERROR_SUCCESS)
				continue;
			maxCount = sizeof(exists);
			if((RegQueryValueExA(sysnameKey,"Exists",NULL,&type,(BYTE *)(&exists), &maxCount) != ERROR_SUCCESS) || exists != 1)
				continue;
			if(RegQueryInfoKeyA(sysnameKey,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,&maxValue,NULL,NULL) != ERROR_SUCCESS)
				continue;
			maxValue++;
			maxCount = sizeof(vid);
			if(RegQueryValueExA(sysnameKey,"VID",NULL,&type,(BYTE *)(&vid), &maxCount) != ERROR_SUCCESS)
				continue;
			maxCount = sizeof(pid);
			if(RegQueryValueExA(sysnameKey,"PID",NULL,&type,(BYTE *)(&pid), &maxCount) != ERROR_SUCCESS)
				continue;
			maxCount = sizeof(interfaceNum);
			RegQueryValueExA(sysnameKey,"InterfaceNum",NULL,&type,(BYTE *)(&interfaceNum), &maxCount);
			maxCount = maxValue;
			devpath = HeapAlloc(GetProcessHeap(),0,maxCount);
			if(RegQueryValueExA(sysnameKey,"DevPath",NULL,&type,(BYTE *)devpath, &maxCount) != ERROR_SUCCESS)
			{
				HeapFree(GetProcessHeap(),0,devpath);
				continue;
			}
			maxCount = maxValue;
			sysname = HeapAlloc(GetProcessHeap(),0,maxCount);
			if(RegQueryValueExA(sysnameKey,"SysName",NULL,&type,(BYTE *)sysname, &maxCount) != ERROR_SUCCESS)
			{
				HeapFree(GetProcessHeap(),0,devpath);
				HeapFree(GetProcessHeap(),0,sysname);
				continue;
			}
			maxCount = maxValue;
			interface = HeapAlloc(GetProcessHeap(),0,maxCount);
			if(RegQueryValueExA(sysnameKey,"InterfaceName",NULL,&type,(BYTE *)interface, &maxCount) != ERROR_SUCCESS)
			{
				HeapFree(GetProcessHeap(),0,interface);
				interface = NULL;
			}
			maxCount = maxValue;
			serial = HeapAlloc(GetProcessHeap(),0,maxCount);
			if(RegQueryValueExA(sysnameKey,"SerialNum",NULL,&type,(BYTE *)serial, &maxCount) != ERROR_SUCCESS)
			{
				HeapFree(GetProcessHeap(),0,serial);
				serial = NULL;
			}
			Log4(PCSC_LOG_INFO, "GetFromRegInfo:vid:%04x pid:%04x interfacenum:%d", vid,pid,interfaceNum);
			Log3(PCSC_LOG_INFO, "GetFromRegInfo:devpath:%s sysname:%s", devpath,sysname);
			Log3(PCSC_LOG_INFO, "GetFromRegInfo:interfacename:%s serialnum:%s", interface,serial);
			//construct dbcw and call add device
			dbcw.dbcw_size = sizeof(DEV_BROADCAST_WINE);
			dbcw.dbcw_vid = vid;
			dbcw.dbcw_pid = pid;
			dbcw.dbcw_interfacenum = interfaceNum;
			dbcw.dbcw_devpath = strdupA(devpath);
			dbcw.dbcw_sysname = strdupA(sysname);
			dbcw.dbcw_interfacename = strdupA(interface);
			dbcw.dbcw_serialnum = strdupA(serial);
			libudev_add_device(&dbcw);
			if(devpath != NULL)
				HeapFree(GetProcessHeap(),0,devpath);
			if(sysname != NULL)
				HeapFree(GetProcessHeap(),0,sysname);
			if(interface != NULL)
				HeapFree(GetProcessHeap(),0,interface);
			if(serial != NULL)
				HeapFree(GetProcessHeap(),0,serial);
		}//Key: sysname
		if(sysnameKeyName != NULL)
			HeapFree(GetProcessHeap(),0,sysnameKeyName);
	}//Key: VID_xxxx&PID_xxxx
	if(vpKeyName != NULL)
		HeapFree(GetProcessHeap(),0,vpKeyName);
	Log1(PCSC_LOG_INFO,"Over");
}

#endif
