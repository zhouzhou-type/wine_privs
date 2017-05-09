/**
*Is included in winscard.c and winscard_impl.c
*written 2017,gaofeng@iscas.ac.cn
*/

#ifndef WINSCARDIMPL
#define WINSCARDIMPL

#ifdef USR_LINUX  //winscard_impl.c
#include "winscard_linux.h"
#define SCARD_READERSTATE_T SCARD_READERSTATE

#define WINAPI __attribute__((__stdcall__)) 

#else
#define SCARD_READERSTATE_T SCARD_READERSTATEA
#endif

#define ISDEBUG 0
LONG WINAPI wine_SCardEstablishContext(DWORD dwScope,
		/*@null@*/ LPCVOID pvReserved1, /*@null@*/ LPCVOID pvReserved2,
		/*@out@*/ LPSCARDCONTEXT phContext);

LONG WINAPI wine_SCardStatus(SCARDHANDLE hCard,
		/*@null@*/ /*@out@*/ LPSTR mszReaderName,
		/*@null@*/ /*@out@*/ LPDWORD pcchReaderLen,
		/*@null@*/ /*@out@*/ LPDWORD pdwState,
		/*@null@*/ /*@out@*/ LPDWORD pdwProtocol,
		/*@null@*/ /*@out@*/ LPBYTE pbAtr,
		/*@null@*/ /*@out@*/ LPDWORD pcbAtrLen);

LONG WINAPI wine_SCardIsValidContext(SCARDCONTEXT hContext);

LONG WINAPI wine_SCardReleaseContext(SCARDCONTEXT hContext);

LONG WINAPI wine_SCardListReaders(SCARDCONTEXT hContext,
		LPCSTR mszGroups,
		LPSTR mszReaders,
		LPDWORD pcchReaders );

LONG WINAPI wine_SCardConnect(SCARDCONTEXT hContext,
				LPCSTR szReader,
				DWORD dwShareMode,
				DWORD dwPreferredProtocols,
				LPSCARDHANDLE phCard,
				LPDWORD pdwActiveProtocol);

LONG WINAPI wine_SCardControl(SCARDHANDLE hCard,
					DWORD dwControlCode,
					LPCVOID pbSendBuffer,
					DWORD cbSendLength,
					LPVOID pbRecvBuffer,
					DWORD cbRecvLength,
					LPDWORD lpBytesReturned 
				);

LONG WINAPI wine_SCardDisconnect(SCARDHANDLE hCard,
						DWORD dwDisposition);

LONG WINAPI wine_SCardEndTransaction(SCARDHANDLE hCard,DWORD dwDisposition);	


LONG WINAPI wine_SCardFreeMemory(SCARDCONTEXT hContext,LPCVOID pvMem);

LONG WINAPI wine_SCardGetAttrib(SCARDHANDLE hCard,DWORD dwAttrId,LPBYTE pbAttr,LPDWORD pcbAttrLen);

LONG WINAPI wine_SCardGetStatusChange(SCARDCONTEXT hContext,DWORD dwTimeout,SCARD_READERSTATE_T *rgReaderStates,
		DWORD cReaders);

LONG WINAPI wine_SCardListReaderGroups(SCARDCONTEXT hContext,LPSTR mszGroups,LPDWORD pcchGroups);

LONG WINAPI wine_SCardReconnect(SCARDHANDLE hCard,DWORD dwShareMode,DWORD dwPreferredProtocols,
		DWORD dwInitialization,LPDWORD pdwActiveProtocol);

LONG WINAPI wine_SCardSetAttrib(SCARDHANDLE hCard,DWORD dwAttrId,LPCBYTE pbAttr,DWORD cbAttrLen);

LONG WINAPI wine_SCardTransmit(SCARDHANDLE hCard,const SCARD_IO_REQUEST *pioSendPci,LPCBYTE pbSendBuffer,
				DWORD cbSendLength,SCARD_IO_REQUEST *pioRecvPci,LPBYTE pbRecvBuffer,
				LPDWORD pcbRecvLength);

LONG WINAPI wine_SCardBeginTransaction(SCARDHANDLE hHandle);

LONG WINAPI wine_SCardCancel(SCARDCONTEXT hContext);

#endif //WINSCARDIMPL

