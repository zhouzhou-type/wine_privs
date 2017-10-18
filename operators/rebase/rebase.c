/*
 * Copyright 2017 WINEGAR 
 *
 
 */

#define WIN32_LEAN_AND_MEAN
//#include "wine/port.h"

#include <windows.h>

#include "config.h"


#include <assert.h>
#include <stdarg.h>
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "delayloadhandler.h"

#include "wine/exception.h"
#include "wine/library.h"
#include "wine/unicode.h"
#include "wine/debug.h"
#include "wine/server.h"
//#include "ntdll_misc.h"
//#include "ddk/wdm.h"

static const UINT_PTR page_size = 0x1000;



/* convert PE image VirtualAddress to Real Address */
static inline void *get_rva_rebase( HMODULE module, DWORD va )
{
    return (void *)((char *)module + va);
}





typedef int WINAPI DOWINMAIN(HMODULE hMod, LPSTR cmdline);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdline, int cmdshow)
{

    HMODULE hModule;
    DOWINMAIN *doWinMain;
    DWORD test=10;


    //hModule = LoadLibraryA("hhctrl.ocx");
    //doWinMain = (DOWINMAIN*) GetProcAddress(hModule, "doWinMain");

    //return doWinMain(hInst, cmdline); 
    return 0;
}

/***********************************************************************
 *           RtlImageNtHeader   
 */
PIMAGE_NT_HEADERS RtlImageNtHeader_rebase(HMODULE hModule)
{
    IMAGE_NT_HEADERS *ret;

    __TRY
    {
        IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)hModule;

        ret = NULL;
        if (dos->e_magic == IMAGE_DOS_SIGNATURE)
        {
            ret = (IMAGE_NT_HEADERS *)((char *)dos + dos->e_lfanew);
            if (ret->Signature != IMAGE_NT_SIGNATURE) ret = NULL;
        }
    }
    __EXCEPT_PAGE_FAULT
    {
        return NULL;
    }
    __ENDTRY
    return ret;
}

/***********************************************************************
 *           LdrProcessRelocationBlock  (NTDLL.@)
 *
 * Apply relocations to a given page of a mapped PE image.
 */
IMAGE_BASE_RELOCATION * LdrProcessRelocationBlock_rebase( void *page, UINT count,
                                                          USHORT *relocs, INT_PTR delta )
{
    while (count--)
    {
        USHORT offset = *relocs & 0xfff;
        int type = *relocs >> 12;
        switch(type)
        {
        case IMAGE_REL_BASED_ABSOLUTE:
            break;
        case IMAGE_REL_BASED_HIGH:
            *(short *)((char *)page + offset) += HIWORD(delta);
            break;
        case IMAGE_REL_BASED_LOW:
            *(short *)((char *)page + offset) += LOWORD(delta);
            break;
        case IMAGE_REL_BASED_HIGHLOW:
            *(int *)((char *)page + offset) += delta;
            break;
#ifdef __x86_64__
        case IMAGE_REL_BASED_DIR64:
            *(INT_PTR *)((char *)page + offset) += delta;
            break;
#elif defined(__arm__)
        case IMAGE_REL_BASED_THUMB_MOV32:
        {
            DWORD inst = *(INT_PTR *)((char *)page + offset);
            DWORD imm16 = ((inst << 1) & 0x0800) + ((inst << 12) & 0xf000) +
                          ((inst >> 20) & 0x0700) + ((inst >> 16) & 0x00ff);
            DWORD hi_delta;

            if ((inst & 0x8000fbf0) != 0x0000f240)
                ERR("wrong Thumb2 instruction %08x, expected MOVW\n", inst);

            imm16 += LOWORD(delta);
            hi_delta = HIWORD(delta) + HIWORD(imm16);
            *(INT_PTR *)((char *)page + offset) = (inst & 0x8f00fbf0) + ((imm16 >> 1) & 0x0400) +
                                                  ((imm16 >> 12) & 0x000f) +
                                                  ((imm16 << 20) & 0x70000000) +
                                                  ((imm16 << 16) & 0xff0000);

            if (hi_delta != 0)
            {
                inst = *(INT_PTR *)((char *)page + offset + 4);
                imm16 = ((inst << 1) & 0x0800) + ((inst << 12) & 0xf000) +
                        ((inst >> 20) & 0x0700) + ((inst >> 16) & 0x00ff);

                if ((inst & 0x8000fbf0) != 0x0000f2c0)
                    ERR("wrong Thumb2 instruction %08x, expected MOVT\n", inst);

                imm16 += hi_delta;
                if (imm16 > 0xffff)
                    ERR("resulting immediate value won't fit: %08x\n", imm16);
                *(INT_PTR *)((char *)page + offset + 4) = (inst & 0x8f00fbf0) +
                                                          ((imm16 >> 1) & 0x0400) +
                                                          ((imm16 >> 12) & 0x000f) +
                                                          ((imm16 << 20) & 0x70000000) +
                                                          ((imm16 << 16) & 0xff0000);
            }
        }
            break;
#endif
        default:
            //FIXME("Unknown/unsupported fixup type %x.\n", type);
            return NULL;
        }
        relocs++;
    }
    return (IMAGE_BASE_RELOCATION *)relocs;  /* return address of next block */
}


NTSTATUS rebase_perform_relocations( void *module, SIZE_T len )
{
    //MESSAGE("Operators:  rebase!!!\n");
    IMAGE_NT_HEADERS *nt;
    char *base;
    IMAGE_BASE_RELOCATION *rel, *end;
    const IMAGE_DATA_DIRECTORY *relocs;
    const IMAGE_SECTION_HEADER *sec;
    INT_PTR delta;
    ULONG protect_old[96], i;

    nt = RtlImageNtHeader_rebase( module );
    base = (char *)nt->OptionalHeader.ImageBase;

    assert( module != base );

    /* no relocations are performed on non page-aligned binaries */
    if (nt->OptionalHeader.SectionAlignment < page_size)
        return STATUS_SUCCESS;

    if (!(nt->FileHeader.Characteristics & IMAGE_FILE_DLL) && NtCurrentTeb()->Peb->ImageBaseAddress)
        return STATUS_SUCCESS;

    relocs = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

    if (nt->FileHeader.Characteristics & IMAGE_FILE_RELOCS_STRIPPED)
    {
        //WARN( "Need to relocate module from %p to %p, but there are no relocation records\n",
          //    base, module );
        return STATUS_CONFLICTING_ADDRESSES;
    }

    if (!relocs->Size) return STATUS_SUCCESS;
    if (!relocs->VirtualAddress) return STATUS_CONFLICTING_ADDRESSES;

    if (nt->FileHeader.NumberOfSections > sizeof(protect_old)/sizeof(protect_old[0]))
        return STATUS_INVALID_IMAGE_FORMAT;

    sec = (const IMAGE_SECTION_HEADER *)((const char *)&nt->OptionalHeader +
                                         nt->FileHeader.SizeOfOptionalHeader);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        void *addr = get_rva_rebase( module, sec[i].VirtualAddress );
        SIZE_T size = sec[i].SizeOfRawData;
        NtProtectVirtualMemory( NtCurrentProcess(), &addr,
                                &size, PAGE_READWRITE, &protect_old[i] );
    }

    //TRACE( "relocating from %p-%p to %p-%p\n",
      //     base, base + len, module, (char *)module + len );
	
    //winuxgear-zw  relocate dll address
     //MESSAGE( "relocating from %p-%p to %p-%p\n",
         //  base, base + len, module, (char *)module + len );

    rel = get_rva_rebase( module, relocs->VirtualAddress );
    end = get_rva_rebase( module, relocs->VirtualAddress + relocs->Size );
    delta = (char *)module - base;

    while (rel < end - 1 && rel->SizeOfBlock)
    {
        if (rel->VirtualAddress >= len)
        {
            //WARN( "invalid address %p in relocation %p\n", get_rva( module, rel->VirtualAddress ), rel );
            return STATUS_ACCESS_VIOLATION;
        }
        rel = LdrProcessRelocationBlock_rebase( get_rva_rebase( module, rel->VirtualAddress ),
                                         (rel->SizeOfBlock - sizeof(*rel)) / sizeof(USHORT),
                                         (USHORT *)(rel + 1), delta );
        if (!rel) return STATUS_INVALID_IMAGE_FORMAT;
    }

    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        void *addr = get_rva_rebase( module, sec[i].VirtualAddress );
        SIZE_T size = sec[i].SizeOfRawData;
        NtProtectVirtualMemory( NtCurrentProcess(), &addr,
                                &size, protect_old[i], &protect_old[i] );
    }

    return STATUS_SUCCESS;
}

}


