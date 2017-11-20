/**************************************************/
// New file, a copy of this file locates in ~/loader/
//
// 2017.08.22, by ysl
/**************************************************/

#ifndef __PRESTART_UTIL_H__
#define __PRESTART_UTIL_H__

#if defined(_WINNT_)

#include "windef.h"
#include "winnt.h"

#else // _WINNT_

#include <stddef.h> // for offsetof

typedef struct _LIST_ENTRY
{
    struct _LIST_ENTRY *Flink;
    struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY;

#define CONTAINING_RECORD(addr, type, field) \
    ((type *)((char *)(addr)-offsetof(type, field)))

#endif // _WINNT_

#if defined(__WINE_WINTERNL_H)

#include "winternl.h"

#else // __WINE_WINTERNL_H

/* list manipulation macros */
#define InitializeListHead(le) (void)((le)->Flink = (le)->Blink = (le))
#define InsertHeadList(le, e)        \
    do                               \
    {                                \
        PLIST_ENTRY f = (le)->Flink; \
        (e)->Flink = f;              \
        (e)->Blink = (le);           \
        f->Blink = (e);              \
        (le)->Flink = (e);           \
    } while (0)
#define InsertTailList(le, e)        \
    do                               \
    {                                \
        PLIST_ENTRY b = (le)->Blink; \
        (e)->Flink = (le);           \
        (e)->Blink = b;              \
        b->Flink = (e);              \
        (le)->Blink = (e);           \
    } while (0)
#define IsListEmpty(le) ((le)->Flink == (le))
#define RemoveEntryList(e)                          \
    do                                              \
    {                                               \
        PLIST_ENTRY f = (e)->Flink, b = (e)->Blink; \
        f->Blink = b;                               \
        b->Flink = f;                               \
        (e)->Flink = (e)->Blink = NULL;             \
    } while (0)
static inline PLIST_ENTRY RemoveHeadList(PLIST_ENTRY le)
{
    PLIST_ENTRY f, b, e;

    e = le->Flink;
    f = le->Flink->Flink;
    b = le->Flink->Blink;
    f->Blink = b;
    b->Flink = f;

    if (e != le)
        e->Flink = e->Blink = NULL;
    return e;
}
static inline PLIST_ENTRY RemoveTailList(PLIST_ENTRY le)
{
    PLIST_ENTRY f, b, e;

    e = le->Blink;
    f = le->Blink->Flink;
    b = le->Blink->Blink;
    f->Blink = b;
    b->Flink = f;

    if (e != le)
        e->Flink = e->Blink = NULL;
    return e;
}

#endif // __WINE_WINTERNL_H

extern int prestart_initialize(void);
extern const char *get_prestart_socket_path(void);

#endif // __PRESTART_UTIL_H__
