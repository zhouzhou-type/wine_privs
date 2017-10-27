/*
 * Task SCard Service
 *
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winsvc.h"
#include "wine/debug.h"
#include "svr_main.h"
#include "winuser.h"
#include "dbt.h"
#include "dbt_append.h"
#include "hotplug.h"

WINE_DEFAULT_DEBUG_CHANNEL(scardsvr);

static const WCHAR scardsvrW[] = {'S','C','a','r','d','S','v','r',0};
static SERVICE_STATUS_HANDLE scardsvr_handle;
static HANDLE done_event;
static HDEVNOTIFY dev_notify;

static void scardsvr_update_status(DWORD state)
{
    SERVICE_STATUS status;

    status.dwServiceType = SERVICE_WIN32;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    status.dwWin32ExitCode = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint = 0;
    status.dwWaitHint = 0;
    status.dwControlsAccepted = 0;
    status.dwCurrentState = state;

    SetServiceStatus(scardsvr_handle, &status);
}

static DWORD WINAPI scardsvr_handler( DWORD control, DWORD event_type, LPVOID event_data, LPVOID context )
{
    TRACE("control:%#x  event_type:%04x  event_data:%p\n", control, event_type, event_data);

    switch (control)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        scardsvr_update_status(SERVICE_STOP_PENDING);
        if(UnregisterDeviceNotification(dev_notify) == FALSE)
            ERR("UnRegisterDeviceNotification error %d\n", GetLastError());
        SetEvent(done_event);
        break;
    case SERVICE_CONTROL_DEVICEEVENT:
		if(event_data != NULL)
		{
	        PDEV_BROADCAST_HDR dbh = (DEV_BROADCAST_HDR*)event_data;
	        PDEV_BROADCAST_WINE dbcw = NULL;
			if(event_type == DBT_DEVICEARRIVAL)
			{
                switch(dbh->dbch_devicetype)
                {
                    case DBT_DEVTYP_WINE:
                        dbcw = (DEV_BROADCAST_WINE *)dbh;
				        libudev_add_device(dbcw);
                        break;
                    case DBT_DEVTYP_DEVICEINTERFACE:
                        FIXME("control:DBT_DEVTYP_DEVICEINITERFACE\n");
                        break;
                    default:
                        break;
                }
			}else if(event_type == DBT_DEVICEREMOVECOMPLETE)
			{
                switch(dbh->dbch_devicetype)
                {
                    case DBT_DEVTYP_WINE:
                        dbcw = (DEV_BROADCAST_WINE *)dbh;
				        libudev_remove_device(dbcw);
                        break;
                    case DBT_DEVTYP_DEVICEINTERFACE:
                        FIXME("control:DBT_DEVTYP_DEVICEINITERFACE\n");
                        break;
                    default:
                        break;
                }
			}
		}
    default:
        scardsvr_update_status(SERVICE_RUNNING);
        break;
    }
	return NO_ERROR;
}
int svr_init()
{
    return S_OK;
}


void WINAPI ServiceMain(DWORD argc, LPWSTR *argv)
{
    #define ARGC 2
    char *pcscd_argv[ARGC] = {"pcscd","-adf"};
    TRACE("starting Task Scard Service!\n");

    scardsvr_handle = RegisterServiceCtrlHandlerExW(scardsvrW, scardsvr_handler,NULL);
    if (!scardsvr_handle)
    {
        ERR("RegisterServiceCtrlHandler error %d\n", GetLastError());
        return;
    }
    else
    {
        dev_notify = RegisterDeviceNotificationW(scardsvr_handle,NULL,DEVICE_NOTIFY_SERVICE_HANDLE);
        if(dev_notify == NULL)
            ERR("RegisterDeviceNotification error %d\n", GetLastError());
    }

    done_event = CreateEventW(NULL, TRUE, FALSE, NULL);

    scardsvr_update_status(SERVICE_START_PENDING);

    if (svr_init() == S_OK)
    {
        scardsvr_update_status(SERVICE_RUNNING);
		pcscd_main(ARGC,pcscd_argv);
        WaitForSingleObject(done_event, INFINITE);
    }

    scardsvr_update_status(SERVICE_STOPPED);

    TRACE("exiting Task Scardsvr Service\n");
}


