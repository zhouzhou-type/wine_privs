/* Copyright 2005 Ulrich Czekalla
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pwd.h>
#include <unistd.h>
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "wtsapi32.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wtsapi);

static inline BOOL WTS_UnicodeToANSI(LPSTR str, LPWSTR wstr, int strlen)
{
	if (!wstr )
	{
		str = NULL;
		return TRUE;
	}
	int strsize = strlen+1;
	int len=0;
	CHAR wide[strsize*2];

	memcpy(wide, wstr, strsize*2);
	if (str)
	{
		while(len < strsize)
		{
				str[len] = wide[len*2];
				len++;
		}
		return TRUE;
	}

	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return FALSE;

}

static inline BOOL WTS_ANSIToUnicode(LPWSTR wstr, LPSTR str, int strsize)
{
	if (!str || strsize <= 0)
	{
		wstr = NULL;
		return TRUE;
	}

	int len=0;
	int length = strsize*sizeof(WCHAR);
	CHAR buf[length];

	while(len < strsize)
	{
		buf[len*2] = str[len];
		buf[len*2+1] = 0;
		len++;
	}

	if(wstr)
	{
		memcpy(wstr, buf, length);
		return TRUE;
	}

	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return FALSE;
}

/************************************************************
 *                WTSCloseServer  (WTSAPI32.@)
 */
void WINAPI WTSCloseServer(HANDLE hServer)
{
    FIXME("Stub %p\n", hServer);
}

/************************************************************
 *                WTSConnectSessionA  (WTSAPI32.@)
 */
BOOL WINAPI WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId, PSTR pPassword, BOOL bWait)
{
   FIXME("Stub %d %d (%s) %d\n", LogonId, TargetLogonId, debugstr_a(pPassword), bWait);
   return TRUE;
}

/************************************************************
 *                WTSConnectSessionW  (WTSAPI32.@)
 */
BOOL WINAPI WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId, PWSTR pPassword, BOOL bWait)
{
   FIXME("Stub %d %d (%s) %d\n", LogonId, TargetLogonId, debugstr_w(pPassword), bWait);
   return TRUE;
}

/************************************************************
 *                WTSDisconnectSession  (WTSAPI32.@)
 */
BOOL WINAPI WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
    FIXME("Stub %p 0x%08x %d\n", hServer, SessionId, bWait);
    return TRUE;
}

/************************************************************
 *                WTSEnableChildSessions  (WTSAPI32.@)
 */
BOOL WINAPI WTSEnableChildSessions(BOOL enable)
{
    FIXME("Stub %d\n", enable);
    return TRUE;
}

/************************************************************
 *                WTSEnumerateProcessesA  (WTSAPI32.@)
 */
BOOL WINAPI WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved, DWORD Version,
    PWTS_PROCESS_INFOA* ppProcessInfo, DWORD* pCount)
{
    FIXME("Stub %p 0x%08x 0x%08x %p %p\n", hServer, Reserved, Version,
          ppProcessInfo, pCount);

    if (!ppProcessInfo || !pCount) return FALSE;

    *pCount = 0;
    *ppProcessInfo = NULL;

    return TRUE;
}

/************************************************************
 *                WTSEnumerateProcessesW  (WTSAPI32.@)
 */
BOOL WINAPI WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved, DWORD Version,
    PWTS_PROCESS_INFOW* ppProcessInfo, DWORD* pCount)
{
    FIXME("Stub %p 0x%08x 0x%08x %p %p\n", hServer, Reserved, Version,
          ppProcessInfo, pCount);

    if (!ppProcessInfo || !pCount || Reserved != 0 || Version != 1)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    *pCount = 0;
    *ppProcessInfo = NULL;

    return TRUE;
}

/************************************************************
 *                WTSEnumerateServersA  (WTSAPI32.@)
 */
BOOL WINAPI WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOA *ppServerInfo, DWORD *pCount)
{
    FIXME("Stub %s 0x%08x 0x%08x %p %p\n", debugstr_a(pDomainName), Reserved, Version, ppServerInfo, pCount);
    return FALSE;
}

/************************************************************
 *                WTSEnumerateServersW  (WTSAPI32.@)
 */
BOOL WINAPI WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOW *ppServerInfo, DWORD *pCount)
{
    FIXME("Stub %s 0x%08x 0x%08x %p %p\n", debugstr_w(pDomainName), Reserved, Version, ppServerInfo, pCount);
    return FALSE;
}


/************************************************************
 *                WTSEnumerateEnumerateSessionsA  (WTSAPI32.@)
 */
BOOL WINAPI WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved, DWORD Version,
    PWTS_SESSION_INFOA* ppSessionInfo, DWORD* pCount)
{
    BOOL ret = FALSE;
    PWTS_SESSION_INFOW pwSessionInfo;
    int i;
    int size, newsize;
    TRACE(" %p 0x%08x 0x%08x %p %p\n", hServer, Reserved, Version,
          ppSessionInfo, pCount);

    if (Reserved || (Version != 1)) return ret;
    if( hServer == WTS_CURRENT_SERVER_HANDLE )
    {
    	if(WTSEnumerateSessionsW(hServer, Reserved, Version, &pwSessionInfo, pCount))
    	{
    		size = (*pCount)*sizeof(WTS_SESSION_INFOA);
    		*ppSessionInfo = LocalAlloc(LMEM_ZEROINIT, size);
    		for(i=0; i<(*pCount); i++)
    		{
    			newsize = size + lstrlenW(pwSessionInfo[i].pWinStationName)+1;
    			if((*ppSessionInfo = LocalReAlloc(*ppSessionInfo, newsize, LMEM_ZEROINIT)))
    			{
    				if(WTS_UnicodeToANSI((*ppSessionInfo)+size, pwSessionInfo[i].pWinStationName, lstrlenW(pwSessionInfo[i].pWinStationName)))
    				{
    					ppSessionInfo[i]->pWinStationName = (*ppSessionInfo)+size;
    					size = newsize;
    				}
    				else ret = FALSE;
    			}
    			else ret = FALSE;
        		ppSessionInfo[i]->SessionId = pwSessionInfo[i].SessionId;
        		ppSessionInfo[i]->State = pwSessionInfo[i].State;
    		}
    		ret = TRUE;
    		WTSFreeMemory(pwSessionInfo);
    	}
    }
    return ret;
}

/************************************************************
 *                WTSEnumerateEnumerateSessionsW  (WTSAPI32.@)
 */
BOOL WINAPI WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version,
    PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount)
{
	static WCHAR console[]={'C','o','n','s','o','l','e',0};
	static WCHAR services[]={'S','e','r','v','i','c','e','s',0};
	BOOL ret = FALSE;
	int i;
	int size, newsize;

    TRACE(" %p 0x%08x 0x%08x %p %p\n", hServer, Reserved, Version,
          ppSessionInfo, pCount);

    if (Reserved || (Version != 1)) return ret;

    *pCount = 0;
    *ppSessionInfo = NULL;
     fprintf(stderr,"WTSEnumerateSessionsW(%d)\n",lstrlenW(console));
    if(hServer == WTS_CURRENT_SERVER_HANDLE ){
    	SERVER_START_REQ( enum_session )
    	{
    		fprintf(stderr,"enum_session: (%d)\n",GetLastError());
    		if (!wine_server_call_err( req ))
    		{
    			if((*pCount = reply->size)){
    				size = (reply->size)*sizeof(WTS_SESSION_INFOW);
    				*ppSessionInfo = LocalAlloc(LMEM_ZEROINIT,size);
    				if(*ppSessionInfo)
    				{
        				for(i = 0; i < reply->size; i++)
        				{
        					if(reply->ids[i])
        					{
        						newsize = size + sizeof(console);
        						if((*ppSessionInfo = LocalReAlloc(*ppSessionInfo, newsize, LMEM_ZEROINIT)))
        						{
        							memcpy((*ppSessionInfo)+size, console, sizeof(console));
        							ppSessionInfo[i]->pWinStationName = (*ppSessionInfo)+size;
        							size = newsize;
        						}
        					}
        					else
        					{
        						newsize = size + sizeof(services);
        						if((*ppSessionInfo = LocalReAlloc(*ppSessionInfo, newsize, LMEM_ZEROINIT)))
        						{
        							memcpy((*ppSessionInfo)+size, services, sizeof(services));
        							ppSessionInfo[i]->pWinStationName = (*ppSessionInfo)+size;
        							size = newsize;
        						}
        					}
        					ppSessionInfo[i]->SessionId = reply->ids[i];
        					if(reply->ids[i] == NtCurrentTeb()->Peb->SessionId)
        						ppSessionInfo[i]->State = WTSActive;
        					else ppSessionInfo[i]->State = WTSDisconnected;
        				}
        				ret = TRUE;
    				}
    			}

    		}

    	}
    	SERVER_END_REQ;
    }

    return ret;
}

/************************************************************
 *                WTSFreeMemory (WTSAPI32.@)
 */
void WINAPI WTSFreeMemory(PVOID pMemory)
{
    static int once;

    if (!once++) FIXME("Stub %p\n", pMemory);
    LocalFree(pMemory);
}

/************************************************************
 *                WTSLogoffSession (WTSAPI32.@)
 */
BOOL WINAPI WTSLogoffSession(HANDLE hserver, DWORD session_id, BOOL bwait)
{
    FIXME("(%p, 0x%x, %d): stub\n", hserver, session_id, bwait);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/************************************************************
 *                WTSOpenServerA (WTSAPI32.@)
 */
HANDLE WINAPI WTSOpenServerA(LPSTR pServerName)
{
    FIXME("(%s) stub\n", debugstr_a(pServerName));
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

/************************************************************
 *                WTSOpenServerW (WTSAPI32.@)
 */
HANDLE WINAPI WTSOpenServerW(LPWSTR pServerName)
{
    FIXME("(%s) stub\n", debugstr_w(pServerName));
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

/************************************************************
 *                WTSQuerySessionInformationA  (WTSAPI32.@)
 */
BOOL WINAPI WTSQuerySessionInformationA(
    HANDLE hServer,
    DWORD SessionId,
    WTS_INFO_CLASS WTSInfoClass,
    LPSTR* Buffer,
    DWORD* BytesReturned)
{
    /* FIXME: Forward request to winsta.dll::WinStationQueryInformationA */
    DWORD state;
    DWORD scount = 0;
    DWORD id = 0;
    int i;
    BOOL current_id = FALSE;
    static const CHAR console[]={'C','o','n','s','o','l','e',0};
    static const CHAR services[]={'S','e','r','v','i','c','e','s',0};
    BOOL ret = FALSE;
    /* FIXME: Forward request to winsta.dll::WinStationQueryInformationA */
//    FIXME("Stub %p 0x%08x %d %p %p\n", hServer, SessionId, WTSInfoClass,
//        Buffer, BytesReturned);
    if(hServer != WTS_CURRENT_SERVER_HANDLE) goto Error;

    if(SessionId == WTS_CURRENT_SESSION)
    {
    	id = NtCurrentTeb()->Peb->SessionId;
    	current_id = TRUE;
    }
    else if(SessionId == NtCurrentTeb()->Peb->SessionId)
    {
    	id = SessionId;
    	current_id = TRUE;
    }
    else
    {
    	id = SessionId;
    }

    switch(WTSInfoClass){
    case WTSInitialProgram:
    case WTSApplicationName:
    	if(id)
    		ret = TRUE;
    	else SetLastError(ERROR_INVALID_PARAMETER);
    	break;
    case WTSWorkingDirectory:
    case WTSOEMId:
    	ret = TRUE;
    	break;
    case WTSSessionId:
    	*Buffer = LocalAlloc(LMEM_ZEROINIT, sizeof(DWORD));
    	memcpy(*Buffer,&SessionId,sizeof(DWORD));
    	*BytesReturned = sizeof(DWORD);
    	return TRUE;
    case WTSUserName:
       if(current_id)
       {
       	struct passwd *pwd;
       	pwd = getpwuid(getuid());
       	*BytesReturned = lstrlenA(pwd->pw_name)+sizeof(CHAR);
       	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
       	lstrcpyA(*Buffer, pwd->pw_name);
       	return TRUE;
       }
    	else if(!id)
       {
     	   ret = TRUE;
     	   break;
       }
    	else
       {
    		SetLastError(ERROR_FILE_NOT_FOUND);
    		break;
       }
    case WTSWinStationName:
       if(current_id)
       {
			*BytesReturned = sizeof(console);
			*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
			lstrcpyA(*Buffer, console);
        	return TRUE;
       }
     	else if(!id)
       {
          *BytesReturned = sizeof(services);
          *Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
          lstrcpyA(*Buffer, services);
    	   return TRUE;
       }
     	else
       {
     		SetLastError(ERROR_FILE_NOT_FOUND);
     		break;
       }
    case WTSDomainName:
        if(current_id)
        {
        	CHAR host[256];
        	if((gethostname(host,sizeof(host))) == 0)
        	{
        		*BytesReturned = lstrlenA(host)+sizeof(CHAR);
    			*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    			lstrcpyA(*Buffer, host);
    			return TRUE;
        	}
        	else
        	{
        		SetLastError(ERROR_FILE_NOT_FOUND);
        		break;
        	}
        }
     	else if(!id)
        {
     		ret = TRUE;
     		break;
        }
     	else
        {
     		SetLastError(ERROR_FILE_NOT_FOUND);
     		break;
        }
    case WTSConnectState:
    	 if(current_id)
        {
         	*BytesReturned = sizeof(int);
         	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
         	state = WTSActive;
         	memcpy(*Buffer, &state, sizeof(int));
        	return TRUE;
        }
     	else if(!id)
        {
         	*BytesReturned = sizeof(int);
         	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
         	state = WTSDisconnected;
         	memcpy(*Buffer, &state, sizeof(int));
        	return TRUE;
        }
     	else
        {
     		SetLastError(ERROR_FILE_NOT_FOUND);
     		break;
        }
    case WTSClientBuildNumber:
     	*BytesReturned = sizeof(int);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientName:
    case WTSClientDirectory:
     	*BytesReturned = sizeof(CHAR);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientProductId:
     	*BytesReturned = sizeof(short);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientHardwareId:
     	*BytesReturned = sizeof(int);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientAddress:
     	*BytesReturned = 6*sizeof(int);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientDisplay:
     	*BytesReturned = 3*sizeof(int);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientProtocolType:
     	*BytesReturned = sizeof(short);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    default:
    	goto Error;
    }
Error:
   if(ret)
   {
	   (*Buffer) = LocalAlloc(LMEM_ZEROINIT, sizeof(CHAR));
	   *BytesReturned = sizeof(CHAR);
   }
   else
   {
	   (*Buffer) = NULL;
	   *BytesReturned = 0;
   }
   return ret;
}

/************************************************************
 *                WTSQuerySessionInformationW  (WTSAPI32.@)
 */
BOOL WINAPI WTSQuerySessionInformationW(
    HANDLE hServer,
    DWORD SessionId,
    WTS_INFO_CLASS WTSInfoClass,
    LPWSTR* Buffer,
    DWORD* BytesReturned)
{
    DWORD state;
    DWORD scount = 0;
    DWORD id = 0;
    int i;
    BOOL current_id = FALSE;
    static const WCHAR console[]={'C','o','n','s','o','l','e',0};
    static const WCHAR services[]={'S','e','r','v','i','c','e','s',0};
    BOOL ret = FALSE;
    /* FIXME: Forward request to winsta.dll::WinStationQueryInformationA */
//    FIXME("Stub %p 0x%08x %d %p %p\n", hServer, SessionId, WTSInfoClass,
//        Buffer, BytesReturned);
    if(hServer != WTS_CURRENT_SERVER_HANDLE) goto Error;

    if(SessionId == WTS_CURRENT_SESSION)
    {
    	id = NtCurrentTeb()->Peb->SessionId;
    	current_id = TRUE;
    }
    else if(SessionId == NtCurrentTeb()->Peb->SessionId)
    {
    	id = SessionId;
    	current_id = TRUE;
    }
    else
    {
    	id = SessionId;
    }

    switch(WTSInfoClass){
    case WTSInitialProgram:
    case WTSApplicationName:
    	if(id)
    		ret = TRUE;
    	else SetLastError(ERROR_INVALID_PARAMETER);
    	break;
    case WTSWorkingDirectory:
    case WTSOEMId:
    	ret = TRUE;
    	break;
    case WTSSessionId:
    	*Buffer = LocalAlloc(LMEM_ZEROINIT, sizeof(DWORD));
    	memcpy(*Buffer,&SessionId,sizeof(DWORD));
    	*BytesReturned = sizeof(DWORD);
    	return TRUE;
    case WTSUserName:
       if(current_id)
       {
       	struct passwd *pwd;
       	pwd = getpwuid(getuid());
       	*BytesReturned = sizeof(WCHAR)*(lstrlenA(pwd->pw_name)+sizeof(CHAR));
       	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
       	if(WTS_ANSIToUnicode(*Buffer, pwd->pw_name, *BytesReturned))
       		return TRUE;
       	else
       	{
       		LocalFree(*Buffer);
       		ret = FALSE;
       		break;
       	}

       }
    	else if(!id)
       {
     	   ret = TRUE;
     	   break;
       }
    	else
       {
    		SetLastError(ERROR_FILE_NOT_FOUND);
    		break;
       }
    case WTSWinStationName:
       if(current_id)
       {
			*BytesReturned = sizeof(console);
			*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
			lstrcpyW(*Buffer, console);
        	return TRUE;
       }
     	else if(!id)
       {
          *BytesReturned = sizeof(services);
          *Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
          lstrcpyW(*Buffer, services);
    	   return TRUE;
       }
     	else
       {
     		SetLastError(ERROR_FILE_NOT_FOUND);
     		break;
       }
    case WTSDomainName:
        if(current_id)
        {
        	CHAR host[256];
        	if((gethostname(host,sizeof(host))) == 0)
        	{
        		*BytesReturned = sizeof(WCHAR)*(lstrlenA(host)+sizeof(CHAR));
    			*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	       if(WTS_ANSIToUnicode(*Buffer, host, *BytesReturned))
    	       	return TRUE;
    	       else
    	       	{
    	       	LocalFree(*Buffer);
    	       	ret = FALSE;
    	       	break;
    	       	}
        	}
        	else
        	{
        		SetLastError(ERROR_FILE_NOT_FOUND);
        		break;
        	}
        }
     	else if(!id)
        {
     		ret = TRUE;
     		break;
        }
     	else
        {
     		SetLastError(ERROR_FILE_NOT_FOUND);
     		break;
        }
    case WTSConnectState:
    	 if(current_id)
        {
         	*BytesReturned = sizeof(int);
         	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
         	state = WTSActive;
         	memcpy(*Buffer, &state, sizeof(int));
        	return TRUE;
        }
     	else if(!id)
        {
         	*BytesReturned = sizeof(int);
         	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
         	state = WTSDisconnected;
         	memcpy(*Buffer, &state, sizeof(int));
        	return TRUE;
        }
     	else
        {
     		SetLastError(ERROR_FILE_NOT_FOUND);
     		break;
        }
    case WTSClientBuildNumber:
     	*BytesReturned = sizeof(int);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientName:
    case WTSClientDirectory:
     	*BytesReturned = sizeof(WCHAR);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientProductId:
     	*BytesReturned = sizeof(short);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientHardwareId:
     	*BytesReturned = sizeof(int);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientAddress:
     	*BytesReturned = 6*sizeof(int);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientDisplay:
     	*BytesReturned = 3*sizeof(int);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    case WTSClientProtocolType:
     	*BytesReturned = sizeof(short);
     	*Buffer = LocalAlloc(LMEM_ZEROINIT, (*BytesReturned));
    	return TRUE;
    default:
    	goto Error;
    }
Error:
   if(ret)
   {
	   (*Buffer) = LocalAlloc(LMEM_ZEROINIT, sizeof(WCHAR));
	   *BytesReturned = sizeof(WCHAR);
   }
   else
   {
	   (*Buffer) = NULL;
	   *BytesReturned = 0;
   }

   return ret;
}

/************************************************************
 *                WTSQueryUserToken (WTSAPI32.@)
 */
BOOL WINAPI WTSQueryUserToken(ULONG session_id, PHANDLE token)
{
    FIXME("%u %p\n", session_id, token);
    return FALSE;
}

/************************************************************
 *                WTSQueryUserConfigA (WTSAPI32.@)
 */
BOOL WINAPI WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR *ppBuffer, DWORD *pBytesReturned)
{
   FIXME("Stub (%s) (%s) 0x%08x %p %p\n", debugstr_a(pServerName), debugstr_a(pUserName), WTSConfigClass,
        ppBuffer, pBytesReturned);
   return FALSE;
}

/************************************************************
 *                WTSQueryUserConfigW (WTSAPI32.@)
 */
BOOL WINAPI WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR *ppBuffer, DWORD *pBytesReturned)
{
   FIXME("Stub (%s) (%s) 0x%08x %p %p\n", debugstr_w(pServerName), debugstr_w(pUserName), WTSConfigClass,
        ppBuffer, pBytesReturned);
   return FALSE;
}


/************************************************************
 *                WTSRegisterSessionNotification (WTSAPI32.@)
 */
BOOL WINAPI WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags)
{
    FIXME("Stub %p 0x%08x\n", hWnd, dwFlags);
    return TRUE;
}

/************************************************************
 *                WTSRegisterSessionNotification (WTSAPI32.@)
 */
BOOL WINAPI WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd, DWORD dwFlags)
{
    FIXME("Stub %p %p 0x%08x\n", hServer, hWnd, dwFlags);
    return FALSE;
}


/************************************************************
 *                WTSSendMessageA (WTSAPI32.@)
 */
BOOL WINAPI WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle, DWORD TitleLength, LPSTR pMessage,
   DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD *pResponse, BOOL bWait)
{
   FIXME("Stub %p 0x%08x (%s) %d (%s) %d 0x%08x %d %p %d\n", hServer, SessionId, debugstr_a(pTitle), TitleLength, debugstr_a(pMessage), MessageLength, Style, Timeout, pResponse, bWait);
   return FALSE;
}

/************************************************************
 *                WTSSendMessageW (WTSAPI32.@)
 */
BOOL WINAPI WTSSendMessageW(HANDLE hServer, DWORD SessionId, LPWSTR pTitle, DWORD TitleLength, LPWSTR pMessage,
   DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD *pResponse, BOOL bWait)
{
   FIXME("Stub %p 0x%08x (%s) %d (%s) %d 0x%08x %d %p %d\n", hServer, SessionId, debugstr_w(pTitle), TitleLength, debugstr_w(pMessage), MessageLength, Style, Timeout, pResponse, bWait);
   return FALSE;
}

/************************************************************
 *                WTSSetUserConfigA (WTSAPI32.@)
 */
BOOL WINAPI WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer, DWORD DataLength)
{
   FIXME("Stub (%s) (%s) 0x%08x %p %d\n", debugstr_a(pServerName), debugstr_a(pUserName), WTSConfigClass,pBuffer, DataLength);
   return FALSE;
}

/************************************************************
 *                WTSSetUserConfigW (WTSAPI32.@)
 */
BOOL WINAPI WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer, DWORD DataLength)
{
   FIXME("Stub (%s) (%s) 0x%08x %p %d\n", debugstr_w(pServerName), debugstr_w(pUserName), WTSConfigClass,pBuffer, DataLength);
   return FALSE;
}

/************************************************************
 *                WTSShutdownSystem (WTSAPI32.@)
 */
BOOL WINAPI WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag)
{
   FIXME("Stub %p 0x%08x\n", hServer,ShutdownFlag);
   return FALSE;
}

/************************************************************
 *                WTSStartRemoteControlSessionA (WTSAPI32.@)
 */
BOOL WINAPI WTSStartRemoteControlSessionA(LPSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
   FIXME("Stub (%s) %d %d %d\n", debugstr_a(pTargetServerName), TargetLogonId, HotkeyVk, HotkeyModifiers);
   return FALSE;
}

/************************************************************
 *                WTSStartRemoteControlSessionW (WTSAPI32.@)
 */
BOOL WINAPI WTSStartRemoteControlSessionW(LPWSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
   FIXME("Stub (%s) %d %d %d\n", debugstr_w(pTargetServerName), TargetLogonId, HotkeyVk, HotkeyModifiers);
   return FALSE;
}

/************************************************************
 *                WTSStopRemoteControlSession (WTSAPI32.@)
 */
BOOL WINAPI WTSStopRemoteControlSession(ULONG LogonId)
{
   FIXME("Stub %d\n",  LogonId);
   return FALSE;
}

/************************************************************
 *                WTSTerminateProcess (WTSAPI32.@)
 */
BOOL WINAPI WTSTerminateProcess(HANDLE hServer, DWORD ProcessId, DWORD ExitCode)
{
   FIXME("Stub %p %d %d\n", hServer, ProcessId, ExitCode);
   return FALSE;
}

/************************************************************
 *                WTSUnRegisterSessionNotification (WTSAPI32.@)
 */
BOOL WINAPI WTSUnRegisterSessionNotification(HWND hWnd)
{
    FIXME("Stub %p\n", hWnd);
    return FALSE;
}

/************************************************************
 *                WTSUnRegisterSessionNotification (WTSAPI32.@)
 */
BOOL WINAPI WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd)
{
    FIXME("Stub %p %p\n", hServer, hWnd);
    return FALSE;
}


/************************************************************
 *                WTSVirtualChannelClose (WTSAPI32.@)
 */
BOOL WINAPI WTSVirtualChannelClose(HANDLE hChannelHandle)
{
   FIXME("Stub %p\n", hChannelHandle);
   return FALSE;
}

/************************************************************
 *                WTSVirtualChannelOpen (WTSAPI32.@)
 */
HANDLE WINAPI WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName)
{
   FIXME("Stub %p %d (%s)\n", hServer, SessionId, debugstr_a(pVirtualName));
   return NULL;
}

/************************************************************
 *                WTSVirtualChannelOpen (WTSAPI32.@)
 */
HANDLE WINAPI WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{
   FIXME("Stub %d (%s) %d\n",  SessionId, debugstr_a(pVirtualName), flags);
   return NULL;
}

/************************************************************
 *                WTSVirtualChannelPurgeInput (WTSAPI32.@)
 */
BOOL WINAPI WTSVirtualChannelPurgeInput(HANDLE hChannelHandle)
{
   FIXME("Stub %p\n", hChannelHandle);
   return FALSE;
}

/************************************************************
 *                WTSVirtualChannelPurgeOutput (WTSAPI32.@)
 */
BOOL WINAPI WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle)
{
   FIXME("Stub %p\n", hChannelHandle);
   return FALSE;
}


/************************************************************
 *                WTSVirtualChannelQuery (WTSAPI32.@)
 */
BOOL WINAPI WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID *ppBuffer, DWORD *pBytesReturned)
{
   FIXME("Stub %p %d %p %p\n", hChannelHandle, WtsVirtualClass, ppBuffer, pBytesReturned);
   return FALSE;
}

/************************************************************
 *                WTSVirtualChannelRead (WTSAPI32.@)
 */
BOOL WINAPI WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead)
{
   FIXME("Stub %p %d %p %d %p\n", hChannelHandle, TimeOut, Buffer, BufferSize, pBytesRead);
   return FALSE;
}

/************************************************************
 *                WTSVirtualChannelWrite (WTSAPI32.@)
 */
BOOL WINAPI WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten)
{
   FIXME("Stub %p %p %d %p\n", hChannelHandle, Buffer, Length, pBytesWritten);
   return FALSE;
}

/************************************************************
 *                WTSWaitSystemEvent (WTSAPI32.@)
 */
BOOL WINAPI WTSWaitSystemEvent(HANDLE hServer, DWORD Mask, DWORD* Flags)
{
    /* FIXME: Forward request to winsta.dll::WinStationWaitSystemEvent */
    FIXME("Stub %p 0x%08x %p\n", hServer, Mask, Flags);
    return FALSE;
}
