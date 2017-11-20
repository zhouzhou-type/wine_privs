/**************************************************/
// new file, locates in ~/dlls/user32/
//
// 2017.08.14, by ysl
/**************************************************/

#ifndef __PRESTART_H__
#define __PRESTART_H__

#include "windef.h"
#include "winnt.h"

typedef struct  _WinDispItem
{
	LIST_ENTRY WinDispList;
	HWND Hwnd;
	int Cmd;
}WinDispItem, *PWinDispItem;


typedef struct _PrestartStuff
{
	BOOL Activated;
	LIST_ENTRY WinDispList;
}PrestartStuff, *PPrestartStuff;

extern BOOL post_prestart_activated(void); 
extern void post_prestart_init(void);
extern void post_prestart_cleanup(void);
extern void add_win_disp(WinDispItem *wditem);

#endif // ~__PRESTART_H__
