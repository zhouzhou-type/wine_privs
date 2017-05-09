/**
*call pcsclite.so
*written 2017,gaofeng@iscas.ac.cn
*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


#define USR_LINUX
#include "winscard_impl.h"

LONG WINAPI wine_SCardEstablishContext(DWORD dwScope,
		/*@null@*/ LPCVOID pvReserved1, /*@null@*/ LPCVOID pvReserved2,
		/*@out@*/ LPSCARDCONTEXT phContext)
{
    LONG ret;
    if(ISDEBUG)fprintf(stderr,"wine_SCardEstablishContext Linux \n");
    ret = L_SCardEstablishContext( dwScope, pvReserved1, pvReserved2, phContext);
     if(ISDEBUG)fprintf(stderr,"SCardEstablishContext rv:%ld phContext: %ld \n", ret,*phContext);
	return ret;
}

LONG WINAPI wine_SCardReleaseContext (SCARDCONTEXT hContext)
{
     if(ISDEBUG)fprintf(stderr,"wine_SCardReleaseContext: Linux \n");
    return  L_SCardReleaseContext(hContext);
}

LONG WINAPI wine_SCardListReaders(SCARDCONTEXT hContext,LPCSTR mszGroups,
		LPSTR mszReaders,LPDWORD pcchReaders )
{
    LONG ret;
	 if(ISDEBUG)fprintf(stderr,"wine_SCardListReaders \n");
    ret =   L_SCardListReaders( hContext, mszGroups, mszReaders, pcchReaders );

	return ret;
}

LONG WINAPI wine_SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader,DWORD dwShareMode,
				DWORD dwPreferredProtocols, LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardConnect: Linux \n");


    return  L_SCardConnect( hContext, szReader, dwShareMode, dwPreferredProtocols, phCard, pdwActiveProtocol);
}

LONG WINAPI wine_SCardControl(SCARDHANDLE hCard,DWORD dwControlCode,LPCVOID pbSendBuffer,DWORD cbSendLength,
					LPVOID pbRecvBuffer,DWORD cbRecvLength,LPDWORD lpBytesReturned)
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardControl: Linux \n");

    return  L_SCardControl( hCard, dwControlCode, pbSendBuffer, cbSendLength, pbRecvBuffer, cbRecvLength, lpBytesReturned);
}

LONG WINAPI wine_SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardDisconnect: Linux \n");

    return  L_SCardDisconnect( hCard,dwDisposition);
}

LONG WINAPI wine_SCardEndTransaction(SCARDHANDLE hCard,DWORD dwDisposition)
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardEndTransaction: Linux \n");

    return  L_SCardEndTransaction( hCard, dwDisposition);
}

LONG WINAPI wine_SCardFreeMemory(SCARDCONTEXT hContext,LPCVOID pvMem)
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardFreeMemory: Linux \n");

    return  L_SCardFreeMemory( hContext, pvMem);
}

LONG WINAPI wine_SCardGetAttrib(SCARDHANDLE hCard,DWORD dwAttrId,LPBYTE pbAttr,LPDWORD pcbAttrLen)
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardGetAttrib: Linux \n");

    return  L_SCardGetAttrib( hCard, dwAttrId, pbAttr, pcbAttrLen);
}

LONG WINAPI wine_SCardGetStatusChange(SCARDCONTEXT hContext,DWORD dwTimeout,SCARD_READERSTATE *rgReaderStates,
		DWORD cReaders) 	
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardGetStatusChange: Linux \n");

    return  L_SCardGetStatusChange( hContext, dwTimeout, rgReaderStates,cReaders);
}

LONG WINAPI wine_SCardIsValidContext(SCARDCONTEXT hContext)
{
    LONG ret;
	 if(ISDEBUG)fprintf(stderr,"wine_SCardIsValidContext: Linux \n");

    ret =  L_SCardIsValidContext(hContext);
     if(ISDEBUG)fprintf(stderr,"wineSCardIsValidContext hContext :%ld , ret :%ld \n",hContext, ret);
    return ret;
}

LONG WINAPI wine_SCardListReaderGroups(SCARDCONTEXT hContext,LPSTR mszGroups,LPDWORD pcchGroups)
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardListReaderGroups: Linux \n");

    return   L_SCardListReaderGroups( hContext, mszGroups, pcchGroups);
}

LONG WINAPI wine_SCardReconnect(SCARDHANDLE hCard,DWORD dwShareMode,DWORD dwPreferredProtocols,
		DWORD dwInitialization,LPDWORD pdwActiveProtocol)
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardReconnect: Linux \n");

    return  L_SCardReconnect(hCard,dwShareMode,dwPreferredProtocols,
		dwInitialization,pdwActiveProtocol);
}

LONG WINAPI wine_SCardSetAttrib(SCARDHANDLE hCard,DWORD dwAttrId,LPCBYTE pbAttr,DWORD cbAttrLen)
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardSetAttrib: Linux \n");

    return  L_SCardSetAttrib( hCard, dwAttrId, pbAttr, cbAttrLen);
}

LONG WINAPI wine_SCardStatus(SCARDHANDLE hCard,LPSTR szReaderName,LPDWORD  	pcchReaderLen,
		LPDWORD pdwState,LPDWORD pdwProtocol,LPBYTE pbAtr,LPDWORD pcbAtrLen)
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardStatus: Linux \n");

    return  L_SCardStatus( hCard, szReaderName,pcchReaderLen, pdwState, pdwProtocol, pbAtr, pcbAtrLen);
}

LONG WINAPI  wine_SCardTransmit(SCARDHANDLE hCard,const SCARD_IO_REQUEST *pioSendPci,LPCBYTE pbSendBuffer,
				DWORD cbSendLength,SCARD_IO_REQUEST *pioRecvPci,LPBYTE pbRecvBuffer,
				LPDWORD pcbRecvLength)
{
	 if(ISDEBUG)fprintf(stderr,"wine_SCardTransmit: Linux \n");

    return  L_SCardTransmit( hCard,pioSendPci, pbSendBuffer,
				 cbSendLength, pioRecvPci, pbRecvBuffer,pcbRecvLength);
}

LONG WINAPI wine_SCardBeginTransaction(SCARDHANDLE hHandle)
{
	     if(ISDEBUG)fprintf(stderr,"wine_SCardBeginTransaction: Linux \n");

	    return  L_SCardBeginTransaction( hHandle);
}

LONG WINAPI wine_SCardCancel(SCARDCONTEXT hContext)
{
	     if(ISDEBUG)fprintf(stderr,"wine_SCardCancel: Linux \n");

	    return  L_SCardCancel( hContext);
}



