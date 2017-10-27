/*
 * Copyright 2011 Hans Leidekker for CodeWeavers
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

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "winsvc.h"
#include "winuser.h"
#include "wine/debug.h"
#include "dbt_append.h"
#include "dbt.h"
#include "wine/list.h"

#include <stdio.h>

WINE_DEFAULT_DEBUG_CHANNEL(plugplay);

static WCHAR plugplayW[] = {'P','l','u','g','P','l','a','y',0};

static SERVICE_STATUS_HANDLE service_handle;
static HANDLE stop_event;

#define DEV_MSG_NEW    0
#define DEV_MSG_SEND   1

typedef struct _device_event
{
    struct list entry;
    DWORD  event_state;
    int    flag;
	PDEV_BROADCAST_WINE dbcw;
}device_event;

static struct list device_event_list = LIST_INIT(device_event_list);

typedef struct _registed_handle
{
    struct list entry;
    LPWSTR service_name;
    HANDLE wnd;
    DWORD  registed_handle;
}registed_handle;

static struct list registed_window_list = LIST_INIT(registed_window_list);
static struct list registed_service_list = LIST_INIT(registed_service_list);

static void deviceEventRegHandlerA(DWORD dwEventType, DEV_BROADCAST_WINE *device_info)
{
	const char *Enum = "System\\CurrentControlSet\\Enum";
	const char *formatA = "USB\\VID_%04x&PID_%04x\\%s";
	char *instanceId = NULL;
	int len,exists;
	long ret;
	HKEY enumKey, key = INVALID_HANDLE_VALUE;

	TRACE("dwEventType=%04x device_info=%p\n",dwEventType, device_info);
	
	if(device_info == NULL)
		return ;

	TRACE("Register Device Event Info: vid:%04x  pid:%04x  interfaceNum:%d  devpath:%s  sysname:%s interface:%s serial:%s syspath:%s\n",device_info->dbcw_vid,device_info->dbcw_pid,device_info->dbcw_interfacenum,device_info->dbcw_devpath,device_info->dbcw_sysname,device_info->dbcw_interfacename,device_info->dbcw_serialnum,device_info->dbcw_syspath);
	len = lstrlenA("USB\\VID_&PID_\\") + 8 + lstrlenA(device_info->dbcw_sysname) + 1;
	if((instanceId = HeapAlloc(GetProcessHeap(), 0, len*sizeof(char))))
	{
		sprintf(instanceId,formatA,device_info->dbcw_vid,device_info->dbcw_pid,device_info->dbcw_sysname);
	}

	if(dwEventType == DBT_DEVICEARRIVAL)
		exists = 1;
	else
		exists = 0;

	ret = RegCreateKeyExA(HKEY_LOCAL_MACHINE,Enum,0,NULL,0,KEY_ALL_ACCESS, NULL, &enumKey, NULL);
	if(!ret)
	{
		ret = RegCreateKeyExA(enumKey,instanceId,0,NULL,0,KEY_READ|KEY_WRITE, NULL, &key, NULL);
		if(!ret)
		{
			if(device_info->dbcw_vid != -1)
				RegSetValueExA(key,"VID",0,REG_DWORD,(BYTE *)&(device_info->dbcw_vid),sizeof(device_info->dbcw_vid));
			if(device_info->dbcw_pid != -1)
				RegSetValueExA(key,"PID",0,REG_DWORD,(BYTE *)&(device_info->dbcw_pid),sizeof(device_info->dbcw_pid));
			if(device_info->dbcw_sysname != NULL)
				RegSetValueExA(key,"SysName",0,REG_SZ,(BYTE *)(device_info->dbcw_sysname),sizeof(char)*(lstrlenA(device_info->dbcw_sysname)+1));
			if(device_info->dbcw_interfacenum != -1)
				RegSetValueExA(key,"InterfaceNum",0,REG_DWORD,(BYTE *)&(device_info->dbcw_interfacenum),sizeof(device_info->dbcw_interfacenum));
			if(device_info->dbcw_interfacename != NULL)
				RegSetValueExA(key,"InterfaceName",0,REG_SZ,(BYTE *)(device_info->dbcw_interfacename),sizeof(char)*(lstrlenA(device_info->dbcw_interfacename)+1));
			if(device_info->dbcw_serialnum != NULL)
				RegSetValueExA(key,"SerialNum",0,REG_SZ,(BYTE *)(device_info->dbcw_serialnum),sizeof(char)*(lstrlenA(device_info->dbcw_serialnum)+1));
			if(device_info->dbcw_devpath != NULL)
				RegSetValueExA(key,"DevPath",0,REG_SZ,(BYTE *)(device_info->dbcw_devpath),sizeof(char)*(lstrlenA(device_info->dbcw_devpath)+1));
			if(device_info->dbcw_syspath != NULL)
				RegSetValueExA(key,"SysPath",0,REG_SZ,(BYTE *)(device_info->dbcw_syspath),sizeof(char)*(lstrlenA(device_info->dbcw_syspath)+1));
			RegSetValueExA(key,"Exists",0,REG_DWORD,(BYTE *)&exists,sizeof(exists));
		}
	}
}

static device_event* addToCache(DWORD dwEventType, DEV_BROADCAST_WINE *dbcw)
{
    device_event *de_entry = NULL;
    device_event *de_new = NULL;
    if(dbcw == NULL || dbcw->dbcw_syspath == NULL)
    {
        return NULL;
    }

    LIST_FOR_EACH_ENTRY(de_entry, &device_event_list, device_event, entry)
    {
        if(lstrcmpA(de_entry->dbcw->dbcw_syspath, dbcw->dbcw_syspath) == 0)
        {
            if(de_entry->event_state == dwEventType)
            {
                de_entry->flag = DEV_MSG_NEW;
                de_new = de_entry;
                break;
            }
        }
    }

    if(!de_new)
    {
        de_new = HeapAlloc(GetProcessHeap(),0,sizeof(device_event));
        de_new->event_state = dwEventType;
        de_new->flag = DEV_MSG_NEW;
        de_new->dbcw = dbcw;
        list_add_tail(&device_event_list, &(de_new->entry));
    }
    return de_new;
}

static void sendEventToService(DWORD dwEventType, LPWSTR service_name, PDEV_BROADCAST_WINE dbcw)
{
	SC_HANDLE scm, service;
	SERVICE_STATUS_PROCESS ssStatus;
	DWORD dwBytesNeeded;
	TRACE("dwEventType=%04x device_info=%p\n",dwEventType, dbcw);
	if(dbcw == NULL)
	{
		ERR("device_info is NULL\n");
		return ;
	}
    if(service_name == NULL)
    {
        ERR("service_name is NULL\n");
        return ;
    }

	scm = OpenSCManagerW(NULL,NULL,0);
	if(!scm)
	{
        ERR("failed to open SCM (%u)\n", GetLastError());
        return;
	}
    service = OpenServiceW(scm, service_name, SERVICE_ALL_ACCESS);
	if(!service)
	{
        ERR("failed to open %s (%u)\n", debugstr_w(service_name), GetLastError());
		CloseServiceHandle(scm);
        return;
	}
	if(!QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,
				(LPBYTE) &ssStatus, sizeof(SERVICE_STATUS_PROCESS),
				&dwBytesNeeded))
	{
		ERR("failed to query service %s status (%u)\n", debugstr_w(service_name), GetLastError());
		CloseServiceHandle(service);
		CloseServiceHandle(scm);
		return;
	}
	if(ssStatus.dwCurrentState != SERVICE_RUNNING)
	{
		ERR("service %s status is not running\n", debugstr_w(service_name));
		CloseServiceHandle(service);
		CloseServiceHandle(scm);
		return;
	}

	TRACE("Send Device Event Info to serivce %s: vid:%04x  pid:%04x  interfaceNum:%d  devpath:%s  sysname:%s interface:%s serial:%s syspath:%s\n",debugstr_w(service_name), dbcw->dbcw_vid, dbcw->dbcw_pid, dbcw->dbcw_interfacenum, dbcw->dbcw_devpath, dbcw->dbcw_sysname, dbcw->dbcw_interfacename, dbcw->dbcw_serialnum, dbcw->dbcw_syspath);

    if(!ControlDeviceEventA(service,dwEventType,dbcw->dbcw_vid,dbcw->dbcw_pid,dbcw->dbcw_interfacenum,dbcw->dbcw_devpath,dbcw->dbcw_sysname,dbcw->dbcw_interfacename,dbcw->dbcw_serialnum,dbcw->dbcw_syspath))
	{
		WARN("control device event failed(%u)\n", GetLastError());
	}
	CloseServiceHandle(service);
	CloseServiceHandle(scm);
}

static void sendEventToWindow(DWORD dwEventType, HANDLE hwnd, PDEV_BROADCAST_WINE dbcw)
{
    GUID interfaceClassGuid = { 0x25dbce51, 0x6c8f, 0x4a72, 
                                               0x8a,0x6d,0xb5,0x4c,0x2b,0x4f,0xc8,0x35 };
    DEV_BROADCAST_DEVICEINTERFACE_W NotificationFilter;

    ZeroMemory( &NotificationFilter, sizeof(NotificationFilter) );
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE_W);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_classguid = interfaceClassGuid;

    if(!SendMessageW(hwnd,WM_DEVICECHANGE, dwEventType,(LPARAM)(&NotificationFilter)))
        ERR("SendEventToWindow error code:%u\n", GetLastError());
}

static void sendEventToWindowAndServices(DWORD event_type, PDEV_BROADCAST_WINE dbcw)
{
    registed_handle *rh = NULL;
    //window
    LIST_FOR_EACH_ENTRY(rh, &registed_window_list, registed_handle, entry)
    {
        fprintf(stderr,"send_event_to_window =%d\n",(DWORD)rh->wnd);
        sendEventToWindow(event_type,rh->wnd,dbcw);
    }

    rh = NULL;
    //service
    LIST_FOR_EACH_ENTRY(rh, &registed_service_list, registed_handle, entry)
    {
        fprintf(stderr,"send_event_to_service:%s\n",debugstr_w(rh->service_name));
        sendEventToService(event_type,rh->service_name,dbcw);
    }
}

static void freeRegistedHandle(DWORD handle)
{
    registed_handle *rh = NULL;
    registed_handle *rh_find = NULL;
    //window
    LIST_FOR_EACH_ENTRY(rh, &registed_window_list, registed_handle, entry)
    {
        if(rh->registed_handle == handle)
        {
            rh_find = rh;
            break;
        }
    }
    if(rh_find != NULL)
    {
        fprintf(stderr,"unreg window_handle=%d in window_list\n",(DWORD)rh_find->wnd);
        list_remove(&rh_find->entry);
    }else
    {//service
        LIST_FOR_EACH_ENTRY(rh, &registed_service_list, registed_handle, entry)
        {
            if(rh->registed_handle == handle)
            {
               rh_find = rh;
               break;
            }
        }
        if(rh_find != NULL)
        {
            fprintf(stderr,"unreg service_name=%s  in service_list\n",debugstr_w(rh_find->service_name));
            list_remove(&rh_find->entry);
        }
    }
}

static DWORD WINAPI service_handler( DWORD ctrl, DWORD event_type, LPVOID event_data, LPVOID context )
{
    SERVICE_STATUS status;
    TRACE("control:%#x  event_type:%04x  event_data:%p\n", ctrl, event_type, event_data);

    status.dwServiceType             = SERVICE_WIN32;
    status.dwControlsAccepted        = SERVICE_ACCEPT_STOP;
    status.dwWin32ExitCode           = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint              = 0;
    status.dwWaitHint                = 0;

    switch(ctrl)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        WINE_TRACE( "shutting down\n" );
        status.dwCurrentState     = SERVICE_STOP_PENDING;
        status.dwControlsAccepted = 0;
        SetServiceStatus( service_handle, &status );
        SetEvent( stop_event );
        return NO_ERROR;
    case SERVICE_CONTROL_DEVICEEVENT:
        if(event_data != NULL)
        {
	        PDEV_BROADCAST_HDR dbh = NULL;
	        PDEV_BROADCAST_WINE dbcw = NULL;
            DEV_NOTIFICATION_INFO *dni = NULL;
            registed_handle * rh = NULL;
            switch(event_type)
            {
                case DBT_DEVICEARRIVAL:
                case DBT_DEVICEREMOVECOMPLETE:
	                dbh = (DEV_BROADCAST_HDR*)event_data;
                    if(dbh->dbch_devicetype == DBT_DEVTYP_WINE)
                    {
                        dbcw = (DEV_BROADCAST_WINE *)dbh;
                        
                        if(event_type == DBT_DEVICEARRIVAL)
                            fprintf(stderr,"pnp-device-arrival\n");
                        else if(event_type == DBT_DEVICEREMOVECOMPLETE)
                            fprintf(stderr,"pnp-device-remove\n");

                        addToCache(event_type,dbcw);
	                    deviceEventRegHandlerA(event_type,dbcw);
                        sendEventToWindowAndServices(event_type,dbcw);
                    }
                    break;
                case DBT_UNREGDEVNOTIF:
                    dni = (DEV_NOTIFICATION_INFO *)event_data;
                    freeRegistedHandle(dni->registed_handle);
                    break;
                case DBT_REGDEVNOTIF:
                    dni = (DEV_NOTIFICATION_INFO *)event_data;
                    rh = HeapAlloc(GetProcessHeap(),0,sizeof(registed_handle));
                    rh->registed_handle = dni->registed_handle;
                    if((dni->flags & DEVICE_NOTIFY_SERVICE_HANDLE) == DEVICE_NOTIFY_SERVICE_HANDLE)
                    {
                        rh->service_name = HeapAlloc(GetProcessHeap(),0,(lstrlenW(dni->service_name)+1)*sizeof(WCHAR));
                        lstrcpyW(rh->service_name,dni->service_name);
                        fprintf(stderr,"reg service_name=%s  in service_list\n",debugstr_w(rh->service_name));
                        list_add_tail(&registed_service_list, &(rh->entry));
                    }else if((dni->flags & DEVICE_NOTIFY_WINDOW_HANDLE) == DEVICE_NOTIFY_WINDOW_HANDLE)
                    {
                        rh->wnd = (HANDLE)dni->wnd;
                        fprintf(stderr,"reg window_handle=%d in window_list\n",dni->wnd);
                        list_add_tail(&registed_window_list, &(rh->entry));
                    }
                    break;                    
                default:
                    break;
            }
            return NO_ERROR;
        }
    default:
        WINE_FIXME( "got service ctrl %x\n", ctrl );
        status.dwCurrentState = SERVICE_RUNNING;
        SetServiceStatus( service_handle, &status );
        return NO_ERROR;
    }
}

static void WINAPI ServiceMain( DWORD argc, LPWSTR *argv )
{
    SERVICE_STATUS status;

    WINE_TRACE( "starting service\n" );

    stop_event = CreateEventW( NULL, TRUE, FALSE, NULL );

    service_handle = RegisterServiceCtrlHandlerExW( plugplayW, service_handler, NULL );
    if (!service_handle)
        return;

    status.dwServiceType             = SERVICE_WIN32;
    status.dwCurrentState            = SERVICE_RUNNING;
    status.dwControlsAccepted        = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    status.dwWin32ExitCode           = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint              = 0;
    status.dwWaitHint                = 10000;
    SetServiceStatus( service_handle, &status );

    WaitForSingleObject( stop_event, INFINITE );

    status.dwCurrentState     = SERVICE_STOPPED;
    status.dwControlsAccepted = 0;
    SetServiceStatus( service_handle, &status );
    WINE_TRACE( "service stopped\n" );
}

int wmain( int argc, WCHAR *argv[] )
{
    static const SERVICE_TABLE_ENTRYW service_table[] =
    {
        { plugplayW, ServiceMain },
        { NULL, NULL }
    };

    StartServiceCtrlDispatcherW( service_table );
    return 0;
}
