/**************************************************/
// new file, locates in ~/dlls/user32/
//
// 2017.08.18, by ysl
/**************************************************/

#include "prestart.h"

//#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "winbase.h"
#include "winternl.h"
//#include "wingdi.h"
#include "winuser.h"

//#include "controls.h"
//#include "user_private.h"
//#include "win.h"
//#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(graphics);

static CRITICAL_SECTION ps_section;
static PrestartStuff ps;

//
static void post_prestart_sighandler(int signo, siginfo_t *siginfo, void *context);
static int init_prestart_signal(void);
static int recover_signal(void);

static BOOL post_prestart_activate(void);
static void remove_all_win_disp(void);

// signal
static struct sigaction newact, oldact;
static const int prestart_signal = SIGUSR1;


static void post_prestart_sighandler(int signo, siginfo_t *siginfo, void *context)
{
	//MESSAGE("[post-prestart] got signal %d, while prestart_signal=%d\n", signo, prestart_signal);
	if (signo == prestart_signal)
	{
		int si = siginfo->si_int;
		const char *env_val = getenv("WINEPRESTART");
		char val_str[40];

		sprintf(val_str, "%d", si);
		if (!env_val ||strcmp(env_val, val_str))
			return;
		
		//EnterCriticalSection(&ps_section);
		
		post_prestart_activate();
		// Do not need win disp any longer
		remove_all_win_disp();
		// recover signal
		recover_signal();

		//LeaveCriticalSection(&ps_section);
	}
}

static int init_prestart_signal()
{
	newact.sa_sigaction = post_prestart_sighandler;
	newact.sa_flags = SA_SIGINFO;
	sigemptyset(&newact.sa_mask);

	return sigaction(prestart_signal, &newact, &oldact);
}

static int recover_signal()
{
	return sigaction(prestart_signal, &oldact, NULL);
}


static void remove_all_win_disp()
{
	PLIST_ENTRY head, entry;

	head = &ps.WinDispList;
	entry = head->Flink;
	while (entry != head)
	{
		PLIST_ENTRY e = entry->Flink;
		WinDispItem *pitem = CONTAINING_RECORD(entry, WinDispItem, WinDispList);
		
		RemoveEntryList(entry);
		free(pitem);
		entry = e;
	}
}

BOOL post_prestart_activated()
{
	return ps.Activated;
}

static BOOL post_prestart_activate()
{
	BOOL old = ps.Activated;
	PLIST_ENTRY head, entry;

	// activate window
	ps.Activated = TRUE;
	head = &ps.WinDispList;
	for (entry = head->Flink; entry != head; entry = entry->Flink)
	{
		WinDispItem *pitem = CONTAINING_RECORD(entry, WinDispItem, WinDispList);
		// MESSAGE("[%s] WinDisp=%p, cmd=%d\n", __FUNCTION__, pitem->Hwnd, pitem->Cmd);
		ShowWindow(pitem->Hwnd, pitem->Cmd);
	}

	return old;
}

void post_prestart_init()
{
	InitializeCriticalSection(&ps_section);
	EnterCriticalSection(&ps_section);

	ps.Activated = FALSE;
	InitializeListHead(&ps.WinDispList);

	init_prestart_signal();
	LeaveCriticalSection(&ps_section);
}

 void post_prestart_cleanup()
 {
	EnterCriticalSection(&ps_section);

	ps.Activated = TRUE;
	remove_all_win_disp();

	recover_signal();
	
	LeaveCriticalSection(&ps_section);
	 
	DeleteCriticalSection(&ps_section);
 }


 void add_win_disp(WinDispItem *item)
{
	PLIST_ENTRY head, entry;
	WinDispItem *newItem;

	if (!item->Hwnd)
		return;
	
	if (ps.Activated)
		return;
	
	EnterCriticalSection(&ps_section);

	head = &ps.WinDispList;
	for (entry = head->Flink; entry != head; entry = entry->Flink)
	{
		WinDispItem *pitem = CONTAINING_RECORD(entry, WinDispItem, WinDispList);
		if (pitem->Hwnd == item->Hwnd)
			break;
	}
	if (entry != head) // exists
	{
		WinDispItem *pitem = CONTAINING_RECORD(entry, WinDispItem, WinDispList);

		//update cmd
		pitem->Cmd = item->Cmd;
		
		// move entry to list tail
		RemoveEntryList(entry);
		InsertTailList(&ps.WinDispList, entry);
		
		return;
	}

	newItem = (PWinDispItem)malloc(sizeof(WinDispItem));
	newItem->Cmd = item->Cmd;
	newItem->Hwnd = item->Hwnd;
	InsertTailList(&ps.WinDispList, &newItem->WinDispList);
	
	LeaveCriticalSection(&ps_section);
}

