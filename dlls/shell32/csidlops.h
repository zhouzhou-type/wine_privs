#ifndef _CSIDLOP_H_
#define _CSIDLOP_H_

#include "winternl.h"

typedef enum _CSIDL_Type {
    CSIDL_Type_User,
    CSIDL_Type_AllUsers,
    CSIDL_Type_CurrVer,
    CSIDL_Type_Disallowed,
    CSIDL_Type_NonExistent,
    CSIDL_Type_WindowsPath,
    CSIDL_Type_SystemPath,
    CSIDL_Type_SystemX86Path,
    CSIDL_Type_ProgramData,
} CSIDL_Type;

extern BOOL WINAPI SHCreateDirectoryUserExW(LPCWSTR path, CSIDL_Type type);

extern NTSTATUS nt_to_unix_file_name_attr( const OBJECT_ATTRIBUTES *attr, ANSI_STRING *unix_name_ret,
		                                           UINT disposition );
#endif
