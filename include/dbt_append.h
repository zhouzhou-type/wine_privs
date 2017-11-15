/*
 * Copyright (C) 2004 Ulrich Czekalla
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

#ifndef __WINE_DBT_APPEND_H
#define __WINE_DBT_APPEND_H

#define DBT_DEVTYP_WINE					0x00000007
#define DBT_REGDEVNOTIF                 0x8007
#define DBT_UNREGDEVNOTIF                 0x8008

typedef struct _DEV_BROADCAST_WINE
{
	DWORD       dbcw_size;
	DWORD       dbcw_devicetype;
	DWORD       dbcw_reserved;
	DWORD       dbcw_vid;
	DWORD       dbcw_pid;
	DWORD		dbcw_interfacenum;
	char*		dbcw_devpath;
	char*		dbcw_sysname;
	char*		dbcw_interfacename;
	char*		dbcw_serialnum;
	char*		dbcw_syspath;
} DEV_BROADCAST_WINE, *PDEV_BROADCAST_WINE;

typedef struct DEV_NOTIFICATION_INFO_T
{
	DWORD       size;
    DWORD       eventtype;
    DWORD       registed_handle;
    DWORD       flags;
    DWORD       wnd;
	LPWSTR      service_name;
} DEV_NOTIFICATION_INFO;

#endif /* __WINE_DBT_APPEND_H */
