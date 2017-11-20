/* i386-linux.elf-main.c -- stub loader for Linux x86 ELF executable

   This file is part of the UPX executable compressor.

   Copyright (C) 1996-2017 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1996-2017 Laszlo Molnar
   Copyright (C) 2000-2017 John F. Reiser
   All Rights Reserved.

   UPX and the UCL library are free software; you can redistribute them
   and/or modify them under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Markus F.X.J. Oberhumer              Laszlo Molnar
   <markus@oberhumer.com>               <ezerotven+github@gmail.com>

   John F. Reiser
   <jreiser@users.sourceforge.net>
 */

#ifndef DEBUG  /*{*/
#define DEBUG 0
#endif  

#include "include/linux.h"
void *mmap(void *, size_t, int, int, int, off_t);
#  define mmap_privanon(addr,len,prot,flgs) mmap((addr),(len),(prot), MAP_PRIVATE|MAP_ANONYMOUS|(flgs),-1,0)
ssize_t write(int, void const *, size_t);

// In order to make it much easier to move this code at runtime and execute
// it at an address different from it load address:  there must be no
// static data, and no string constants.

#if !DEBUG  /*{*/
#define DPRINTF(a) /* empty: no debug drivel */
#define DEBUG_STRCON(name, value) /* empty */
#else  /*}{ DEBUG */
#define va_arg      __builtin_va_arg
#define va_end      __builtin_va_end
#define va_list     __builtin_va_list
#define va_start    __builtin_va_start

#define PIC_STRING(value, var) \
    __asm__ __volatile__ ( \
        "call 0f; .asciz \"" value "\"; \
      0: pop %0;" : "=r"(var) : \
    )

#define DEBUG_STRCON(name, strcon) \
    static char const *name(void) { \
        register char const *rv; PIC_STRING(strcon, rv); \
        return rv; \
    }

static unsigned
div10(unsigned x)
{
    return x / 10u;
}

static int
unsimal(unsigned x, char *ptr, int n)
{
    if (10<=x) {
        unsigned const q = div10(x);
        x -= 10 * q;
        n = unsimal(q, ptr, n);
    }
    ptr[n] = '0' + x;
    return 1+ n;
}

static int
decimal(int x, char *ptr, int n)
{
    if (x < 0) {
        x = -x;
        ptr[n++] = '-';
    }
    return unsimal(x, ptr, n);
}

DEBUG_STRCON(STR_hex, "0123456789abcdef");

static int
heximal(unsigned x, char *ptr, int n)
{
    if (16<=x) {
        n = heximal(x>>4, ptr, n);
        x &= 0xf;
    }
    ptr[n] = STR_hex()[x];
    return 1+ n;
}


#define DPRINTF(a) dprintf a

static int
dprintf(char const *fmt, ...)
{
    char c;
    int n= 0;
    char *ptr;
    char buf[20];
    va_list va; va_start(va, fmt);
    ptr= &buf[0];
    while (0!=(c= *fmt++)) if ('%'!=c) goto literal;
    else switch (c= *fmt++) {
    default: {
literal:
        n+= write(2, fmt-1, 1);
    } break;
    case 0: goto done;  /* early */
    case 'u': {
        n+= write(2, buf, unsimal(va_arg(va, unsigned), buf, 0));
    } break;
    case 'd': {
        n+= write(2, buf, decimal(va_arg(va, int), buf, 0));
    } break;
    case 'p':  /* same as 'x'; relies on sizeof(int)==sizeof(void *) */
    case 'x': {
        buf[0] = '0';
        buf[1] = 'x';
        n+= write(2, buf, heximal(va_arg(va, int), buf, 2));
    } break;
    }
done:
    va_end(va);
    return n;
}
#endif  /*}*/

#define MAX_ELF_HDR 512  // Elf32_Ehdr + n*Elf32_Phdr must fit in this

typedef struct {
    size_t size;  // must be first to match size[0] uncompressed size
    char *buf;
} Extent;

DEBUG_STRCON(STR_xread, "xread %%p(%%x %%p) %%p %%x\\n")

static void
#if (ACC_CC_GNUC >= 0x030300) && defined(__i386__)  /*{*/
__attribute__((__noinline__, __used__, regparm(3), stdcall))
#endif  /*}*/
xread(Extent *x, char *buf, size_t count)
{
    char *p=x->buf, *q=buf;
    size_t j;
    DPRINTF((STR_xread(), x, x->size, x->buf, buf, count));
    if (x->size < count) {
        exit(127);
    }
    for (j = count; 0!=j--; ++p, ++q) {
        *q = *p;
    }
    x->buf  += count;
    x->size -= count;
}

#if 1  //{  save space
#define ERR_LAB error: exit(127);
#define err_exit(a) goto error
#else  //}{  save debugging time
#define ERR_LAB /*empty*/
DEBUG_STRCON(STR_exit, "err_exit %%x\\n");

static void __attribute__ ((__noreturn__))
err_exit(int a)
{
    DPRINTF((STR_exit(), a));
    (void)a;  // debugging convenience
    exit(127);
}
#endif  //}

static void *
do_brk(void *addr)
{
    return brk(addr);
}

typedef void f_unfilter(
    nrv_byte *,  // also addvalue
    nrv_uint,
    unsigned cto8, // junk in high 24 bits
    unsigned ftid
);
typedef int f_expand(
    const nrv_byte *, nrv_uint,
          nrv_byte *, nrv_uint *, unsigned );

DEBUG_STRCON(STR_unpackExtent,
        "unpackExtent in=%%p(%%x %%p)  out=%%p(%%x %%p)  %%p %%p\\n");
DEBUG_STRCON(STR_err5, "sz_cpr=%%x  sz_unc=%%x  xo->size=%%x\\n");

#if 1 //lyl
static void* memcpy(void*dest,const void*src,unsigned int cnt)
{
	char*new=(char*)dest;
	char*old=(char*)src;
	while(cnt)
	{
		*new=*old;
		new++;
		old++;
		cnt--;
	}
	return dest;
}

#include "miracl.h"
#define MR_WORD mr_unsign32
#define NB 4
#define ROTL(x) (((x)>>7)|((x)<<1))

#define ROTL8(x) (((x)<<8)|((x)>>24))
#define ROTL16(x) (((x)<<16)|((x)>>16))
#define ROTL24(x) (((x)<<24)|((x)>>8))

static MR_WORD pack(const MR_BYTE *b)
{ /* pack bytes into a 32-bit Word */
    return ((MR_WORD)b[3]<<24)|((MR_WORD)b[2]<<16)|((MR_WORD)b[1]<<8)|(MR_WORD)b[0];
}

static void unpack(MR_WORD a,MR_BYTE *b)
{ /* unpack bytes from a word */
    b[0]=MR_TOBYTE(a);
    b[1]=MR_TOBYTE(a>>8);
    b[2]=MR_TOBYTE(a>>16);
    b[3]=MR_TOBYTE(a>>24);
}

static MR_BYTE bmul(MR_BYTE x,MR_BYTE y)
{ /* x.y= AntiLog(Log(x) + Log(y)) */
#include "inc/ptab.h"
#include "inc/ltab.h"
    if (x && y) return ptab[(ltab[x]+ltab[y])%255];
    else return 0;
}

static MR_WORD SubByte(MR_WORD a)
{
#include "inc/fbsub.h"
    MR_BYTE b[4];
    unpack(a,b);
    b[0]=fbsub[b[0]];
    b[1]=fbsub[b[1]];
    b[2]=fbsub[b[2]];
    b[3]=fbsub[b[3]];
    return pack(b);
}

static MR_BYTE product(MR_WORD x,MR_WORD y)
{ /* dot product of two 4-byte arrays */
    MR_BYTE xb[4],yb[4];
    unpack(x,xb);
    unpack(y,yb);
    return bmul(xb[0],yb[0])^bmul(xb[1],yb[1])^bmul(xb[2],yb[2])^bmul(xb[3],yb[3]);
}

static MR_WORD InvMixCol(MR_WORD x)
{ /* matrix Multiplication */
    MR_WORD y,m;
    MR_BYTE b[4];

#include "inc/InCo.h"
    m=pack(InCo);
    b[3]=product(m,x);
    m=ROTL24(m);
    b[2]=product(m,x);
    m=ROTL24(m);
    b[1]=product(m,x);
    m=ROTL24(m);
    b[0]=product(m,x);
    y=pack(b);
    return y;
}

static void aes_reset(aes *a,int mode,char *iv)
{ /* reset mode, or reset iv */
    int i;
    a->mode=mode;
    for (i=0;i<4*NB;i++)
        a->f[i]=0;
    if (mode!=MR_ECB && iv!=0x0)
    {
        for (i=0;i<4*NB;i++)
            a->f[i]=iv[i];
    }
}

static BOOL aes_init(aes* a,int mode,int nk,char *key,char *iv)
{ 
#include "inc/rco.h"

    int i,j,k,N,nr;
    MR_WORD CipherKey[8];
    nk/=4;
    if (nk!=4 && nk!=6 && nk!=8) return FALSE;
  /* nr is number of rounds */
    nr=6+nk;
    a->Nk=nk; a->Nr=nr;
    aes_reset(a,mode,iv);
    N=NB*(nr+1);
    for (i=j=0;i<nk;i++,j+=4)
    {
        CipherKey[i]=pack((MR_BYTE *)&key[j]);
    }
    for (i=0;i<nk;i++) a->fkey[i]=CipherKey[i];
    for (j=nk,k=0;j<N;j+=nk,k++)
    {
        a->fkey[j]=a->fkey[j-nk]^SubByte(ROTL24(a->fkey[j-1]))^rco[k];
        if (nk<=6)
        {
            for (i=1;i<nk && (i+j)<N;i++)
                a->fkey[i+j]=a->fkey[i+j-nk]^a->fkey[i+j-1];
        }
        else
        {
            for (i=1;i<4 && (i+j)<N;i++)
                a->fkey[i+j]=a->fkey[i+j-nk]^a->fkey[i+j-1];
            if ((j+4)<N) a->fkey[j+4]=a->fkey[j+4-nk]^SubByte(a->fkey[j+3]);
            for (i=5;i<nk && (i+j)<N;i++)
                a->fkey[i+j]=a->fkey[i+j-nk]^a->fkey[i+j-1];
        }
    }
    for (j=0;j<NB;j++) a->rkey[j+N-NB]=a->fkey[j];
    for (i=NB;i<N-NB;i+=NB)
    {
        k=N-NB-i;
        for (j=0;j<NB;j++) a->rkey[k+j]=InvMixCol(a->fkey[i+j]);
    }
    for (j=N-NB;j<N;j++) a->rkey[j-N+NB]=a->fkey[j];

    return TRUE;
}

static void aes_ecb_encrypt(aes *a,MR_BYTE *buff)
{
#include "inc/ftable.h"
#include "inc/fbsub.h"

#include "inc/ftable1.h"
#include "inc/ftable2.h"
#include "inc/ftable3.h"

    int i,j,k;
    MR_WORD p[4],q[4],*x,*y,*t;
    for (i=j=0;i<NB;i++,j+=4)
    {
        p[i]=pack((MR_BYTE *)&buff[j]);
        p[i]^=a->fkey[i];
    }
    k=NB;
    x=p; y=q;

    for (i=1;i<a->Nr;i++)
    { /* Nr is number of rounds. May be odd. */
        y[0]=a->fkey[k]^ftable[MR_TOBYTE(x[0])]^
             ftable1[MR_TOBYTE(x[1]>>8)]^
             ftable2[MR_TOBYTE(x[2]>>16)]^
             ftable3[x[3]>>24];
        y[1]=a->fkey[k+1]^ftable[MR_TOBYTE(x[1])]^
             ftable1[MR_TOBYTE(x[2]>>8)]^
             ftable2[MR_TOBYTE(x[3]>>16)]^
             ftable3[x[0]>>24];
        y[2]=a->fkey[k+2]^ftable[MR_TOBYTE(x[2])]^
             ftable1[MR_TOBYTE(x[3]>>8)]^
             ftable2[MR_TOBYTE(x[0]>>16)]^
             ftable3[x[1]>>24];
        y[3]=a->fkey[k+3]^ftable[MR_TOBYTE(x[3])]^
             ftable1[MR_TOBYTE(x[0]>>8)]^
             ftable2[MR_TOBYTE(x[1]>>16)]^
             ftable3[x[2]>>24];
        k+=4;
        t=x; x=y; y=t;      /* swap pointers */
    }

    y[0]=a->fkey[k]^(MR_WORD)fbsub[MR_TOBYTE(x[0])]^
         ROTL8((MR_WORD)fbsub[MR_TOBYTE(x[1]>>8)])^
         ROTL16((MR_WORD)fbsub[MR_TOBYTE(x[2]>>16)])^
         ROTL24((MR_WORD)fbsub[x[3]>>24]);
    y[1]=a->fkey[k+1]^(MR_WORD)fbsub[MR_TOBYTE(x[1])]^
         ROTL8((MR_WORD)fbsub[MR_TOBYTE(x[2]>>8)])^
         ROTL16((MR_WORD)fbsub[MR_TOBYTE(x[3]>>16)])^
         ROTL24((MR_WORD)fbsub[x[0]>>24]);
    y[2]=a->fkey[k+2]^(MR_WORD)fbsub[MR_TOBYTE(x[2])]^
         ROTL8((MR_WORD)fbsub[MR_TOBYTE(x[3]>>8)])^
         ROTL16((MR_WORD)fbsub[MR_TOBYTE(x[0]>>16)])^
         ROTL24((MR_WORD)fbsub[x[1]>>24]);
    y[3]=a->fkey[k+3]^(MR_WORD)fbsub[MR_TOBYTE(x[3])]^
         ROTL8((MR_WORD)fbsub[MR_TOBYTE(x[0]>>8)])^
         ROTL16((MR_WORD)fbsub[MR_TOBYTE(x[1]>>16)])^
         ROTL24((MR_WORD)fbsub[x[2]>>24]);

    for (i=j=0;i<NB;i++,j+=4)
    {
        unpack(y[i],(MR_BYTE *)&buff[j]);
        x[i]=y[i]=0;   /* clean up stack */
    }
}

static mr_unsign32 aes_decrypt(aes *a,unsigned char *buff)
{
    int j,bytes;
 
    bytes=a->mode-MR_OFB1+1;
    aes_ecb_encrypt(a,(MR_BYTE *)(a->f));
    for (j=0;j<bytes;j++) buff[j]^=a->f[j];
    return 0;
}

static void aes_end(aes *a)
{ /* clean up */
    int i;
    for (i=0;i<NB*(a->Nr+1);i++)
        a->fkey[i]=a->rkey[i]=0;
    for (i=0;i<4*NB;i++)
        a->f[i]=0;
}

static void cipher_init(void* inst, unsigned len)
{
        aes a;
        char key[32];
        char iv[16];
        int i;

        if(len < sizeof(a))
                return;
        key[31]=0xEF; key[30]=0x43; key[29]=0x59; key[28]=0xD8; key[27]=0xD5; key[26]=0x80; key[25]=0xAA; key[24]=0x4F;
        key[23]=0x7F; key[22]=0x03; key[21]=0x6D; key[20]=0x6F; key[19]=0x04; key[18]=0xFC; key[17]=0x6A; key[16]=0x94;
        key[15]=0x2b; key[14]=0x7E; key[13]=0x15; key[12]=0x16; key[11]=0x28; key[10]=0xAE; key[9]=0xD2;  key[8]=0xA6;
        key[7]=0xAB;  key[6]=0xF7;  key[5]=0x15;  key[4]=0x88;  key[3]=0x09;  key[2]=0xCF;  key[1]=0x4F;  key[0]=0x3C;

        for(i=0; i<16; i++)
                iv[i] = i;

        aes_init(&a,MR_OFB1,32,key,iv);
        memcpy(inst,(char*)&a,sizeof(a));
        return;
}

static void cipher_decrypt(void* inst,unsigned char* in, unsigned in_len, unsigned char* out, unsigned* out_len)
{
        aes a;
        unsigned i;
        unsigned char* t;

        memcpy((char*)&a, (char*)inst, sizeof(a));
        memcpy(out,in,in_len);
        for(i=0; i<in_len; i++)
        {
                t = out +i;
                aes_decrypt(&a,t);
        }
        memcpy(inst,(char*)&a,sizeof(a));
        *out_len = in_len;

}

static void cipher_end(void *inst)
{
        aes a;
        memcpy((char*)&a,(char*)inst,sizeof(a));
        aes_end(&a);
}

#endif

static void
unpackExtent(
    Extent *const xi,  // input
    Extent *const xo,  // output
    f_expand *const f_decompress,
    f_unfilter *f_unf,
    unsigned char hp //lyl
)
{
    DPRINTF((STR_unpackExtent(),
        xi, xi->size, xi->buf, xo, xo->size, xo->buf, f_decompress, f_unf));
    while (xo->size) {
        struct b_info h;
        //   Note: if h.sz_unc == h.sz_cpr then the block was not
        //   compressible and is stored in its uncompressed form.

        // Read and check block sizes.
        xread(xi, (char *)&h, sizeof(h));
        if (h.sz_unc == 0) {                     // uncompressed size 0 -> EOF
            if (h.sz_cpr != UPX_MAGIC_LE32)      // h.sz_cpr must be h->magic
                err_exit(2);
            if (xi->size != 0)                 // all bytes must be written
                err_exit(3);
            break;
        }
        if (h.sz_cpr <= 0) {
            err_exit(4);
ERR_LAB
        }
        if (h.sz_cpr > h.sz_unc
        ||  h.sz_unc > xo->size ) {
            DPRINTF((STR_err5(), h.sz_cpr, h.sz_unc, xo->size));
            err_exit(5);
        }
        // Now we have:
        //   assert(h.sz_cpr <= h.sz_unc);
        //   assert(h.sz_unc > 0 && h.sz_unc <= blocksize);
        //   assert(h.sz_cpr > 0 && h.sz_cpr <= blocksize);

        if (h.sz_cpr < h.sz_unc) { // Decompress block
            nrv_uint out_len = h.sz_unc;  // EOF for lzma

#if 1 //lyl
int j;
if(h.sz_cpr == hp)
{
	unsigned char src[hp];
	char inst[512];
	cipher_init(inst,512);
	cipher_decrypt(inst,(unsigned char*)xi->buf,h.sz_cpr,src,&h.sz_cpr);
	cipher_end(inst);
	j = (*f_decompress)(src, h.sz_cpr,(unsigned char *)xo->buf, &out_len, *(int *)(void *)&h.b_method );
}
else
{
	j = (*f_decompress)((unsigned char *)xi->buf, h.sz_cpr,(unsigned char *)xo->buf, &out_len, *(int *)(void *)&h.b_method );
}
#endif 
            if (j != 0 || out_len != (nrv_uint)h.sz_unc)
                err_exit(7);
            // Skip Ehdr+Phdrs: separate 1st block, not filtered
            if (h.b_ftid!=0 && f_unf  // have filter
            &&  ((512 < out_len)  // this block is longer than Ehdr+Phdrs
              || (xo->size==(unsigned)h.sz_unc) )  // block is last in Extent
            ) {
                (*f_unf)((unsigned char *)xo->buf, out_len, h.b_cto8, h.b_ftid);
            }
            xi->buf  += h.sz_cpr;
            xi->size -= h.sz_cpr;
        }
        else { // copy literal block
            xread(xi, xo->buf, h.sz_cpr);
        }
        xo->buf  += h.sz_unc;
        xo->size -= h.sz_unc;
    }
}

DEBUG_STRCON(STR_make_hatch, "make_hatch %%p %%x %%x\\n");

static void *
make_hatch_x86(Elf32_Phdr const *const phdr, unsigned const reloc)
{
    unsigned *hatch = 0;
    DPRINTF((STR_make_hatch(),phdr,reloc,0));
    if (phdr->p_type==PT_LOAD && phdr->p_flags & PF_X) {
        // The format of the 'if' is
        //  if ( ( (hatch = loc1), test_loc1 )
        //  ||   ( (hatch = loc2), test_loc2 ) ) {
        //      action
        //  }
        // which uses the comma to save bytes when test_locj involves locj
        // and the action is the same when either test succeeds.

        // Try page fragmentation just beyond .text .
        if ( ( (hatch = (void *)(phdr->p_memsz + phdr->p_vaddr + reloc)),
                ( phdr->p_memsz==phdr->p_filesz  // don't pollute potential .bss
                &&  4<=(~PAGE_MASK & -(int)hatch) ) ) // space left on page
        // Try Elf32_Ehdr.e_ident[12..15] .  warning: 'const' cast away
        ||   ( (hatch = (void *)(&((Elf32_Ehdr *)phdr->p_vaddr + reloc)->e_ident[12])),
                (phdr->p_offset==0) ) ) {
            // Omitting 'const' saves repeated literal in gcc.
            unsigned /*const*/ escape = 0xc36180cd;  // "int $0x80; popa; ret"
            // Don't store into read-only page if value is already there.
            if (* (volatile unsigned*) hatch != escape) {
                * hatch  = escape;
            }
        }
        else {
            hatch = 0;
        }
    }
    return hatch;
}

static void
#if defined(__i386__)  /*{*/
__attribute__((regparm(2), stdcall))
#endif  /*}*/
upx_bzero(char *p, size_t len)
{
    if (len) do {
        *p++= 0;
    } while (--len);
}
#define bzero upx_bzero


static Elf32_auxv_t *
#if defined(__i386__)  /*{*/
__attribute__((regparm(2), stdcall))
#endif  /*}*/
auxv_find(Elf32_auxv_t *av, unsigned const type)
{
    Elf32_auxv_t *avail = 0;
    if (av
#if defined(__i386__)  /*{*/
    && 0==(1&(int)av)  /* PT_INTERP usually inhibits, except for hatch */
#endif  /*}*/
    ) {
        for (;; ++av) {
            if (av->a_type==type)
                return av;
            if (av->a_type==AT_IGNORE)
                avail = av;
        }
        if (0!=avail && AT_NULL!=type) {
                av = avail;
                av->a_type = type;
                return av;
        }
    }
    return 0;
}

DEBUG_STRCON(STR_auxv_up, "auxv_up  %%p %%x %%x\\n");

static void
#if defined(__i386__)  /*{*/
__attribute__((regparm(3), stdcall))
#endif  /*}*/
auxv_up(Elf32_auxv_t *av, unsigned const type, unsigned const value)
{
    DPRINTF((STR_auxv_up(),av,type,value));
    av = auxv_find(av, type);
    if (av) {
        av->a_un.a_val = value;
    }
}

// The PF_* and PROT_* bits are {1,2,4}; the conversion table fits in 32 bits.
#define REP8(x) \
    ((x)|((x)<<4)|((x)<<8)|((x)<<12)|((x)<<16)|((x)<<20)|((x)<<24)|((x)<<28))
#define EXP8(y) \
    ((1&(y)) ? 0xf0f0f0f0 : (2&(y)) ? 0xff00ff00 : (4&(y)) ? 0xffff0000 : 0)
#define PF_TO_PROT(pf) \
    ((PROT_READ|PROT_WRITE|PROT_EXEC) & ( \
        ( (REP8(PROT_EXEC ) & EXP8(PF_X)) \
         |(REP8(PROT_READ ) & EXP8(PF_R)) \
         |(REP8(PROT_WRITE) & EXP8(PF_W)) \
        ) >> ((pf & (PF_R|PF_W|PF_X))<<2) ))

DEBUG_STRCON(STR_xfind_pages, "xfind_pages  %%x  %%p  %%d  %%p\\n");

// Find convex hull of PT_LOAD (the minimal interval which covers all PT_LOAD),
// and mmap that much, to be sure that a kernel using exec-shield-randomize
// won't place the first piece in a way that leaves no room for the rest.
static unsigned long  // returns relocation constant
__attribute__((regparm(3), stdcall))
xfind_pages(unsigned mflags, Elf32_Phdr const *phdr, int phnum,
    char **const p_brk
#define page_mask PAGE_MASK
)
{
    size_t lo= ~0, hi= 0, szlo= 0;
    char *addr;
    DPRINTF((STR_xfind_pages(), mflags, phdr, phnum, p_brk));
    for (; --phnum>=0; ++phdr) if (PT_LOAD==phdr->p_type
                                                ) {
        if (phdr->p_vaddr < lo) {
            lo = phdr->p_vaddr;
            szlo = phdr->p_filesz;
        }
        if (hi < (phdr->p_memsz + phdr->p_vaddr)) {
            hi =  phdr->p_memsz + phdr->p_vaddr;
        }
    }
    szlo += ~page_mask & lo;  // page fragment on lo edge
    lo   -= ~page_mask & lo;  // round down to page boundary
    hi    =  page_mask & (hi - lo - page_mask -1);  // page length
    szlo  =  page_mask & (szlo    - page_mask -1);  // page length
    if (MAP_FIXED & mflags) {
        addr = (char *)lo;
    }
    else {
        addr = mmap_privanon((void *)lo, hi, PROT_NONE, mflags);
        //munmap(szlo + addr, hi - szlo);
    }
    *p_brk = hi + addr;  // the logical value of brk(0)
    return (unsigned long)addr - lo;
}

DEBUG_STRCON(STR_do_xmap,
    "do_xmap  fdi=%%x  ehdr=%%p  xi=%%p(%%x %%p)  av=%%p  p_reloc=%%p  f_unf=%%p\\n")

static Elf32_Addr  // entry address
do_xmap(int const fdi, Elf32_Ehdr const *const ehdr, Extent *const xi,
    Elf32_auxv_t *const av, unsigned *const p_reloc, f_unfilter *const f_unf,unsigned char hp)
{
    Elf32_Phdr const *phdr = (Elf32_Phdr const *) (ehdr->e_phoff +
        (void const *)ehdr);
    unsigned const frag_mask = ~PAGE_MASK;
    char *v_brk;
    unsigned const reloc = xfind_pages(((ET_EXEC==ehdr->e_type) ? MAP_FIXED : 0),
         phdr, ehdr->e_phnum, &v_brk
        );
    int j;
    DPRINTF((STR_do_xmap(),
        fdi, ehdr, xi, (xi? xi->size: 0), (xi? xi->buf: 0), av, p_reloc, f_unf));
    for (j=0; j < ehdr->e_phnum; ++phdr, ++j)
    if (xi && PT_PHDR==phdr->p_type) {
        auxv_up(av, AT_PHDR, phdr->p_vaddr + reloc);
    }
    else if (PT_LOAD==phdr->p_type
                          ) {
        unsigned const prot = PF_TO_PROT(phdr->p_flags);
        Extent xo;
        size_t mlen = xo.size = phdr->p_filesz;
        char * addr = xo.buf  =  (char *)(phdr->p_vaddr + reloc);
        char *const haddr =     phdr->p_memsz + addr;
        size_t frag  = (int)addr & frag_mask;
        mlen += frag;
        addr -= frag;

#  define LEN_OVER 3

        if (xi) {
            if (addr != mmap_privanon(addr, LEN_OVER + mlen,
                    prot | PROT_WRITE, MAP_FIXED) )
                err_exit(6);
            unpackExtent(xi, &xo, (f_expand *)fdi,((PROT_EXEC & prot) ? f_unf : 0) ,hp);
        }
        else {
            if (addr != mmap(addr, mlen, prot, MAP_FIXED | MAP_PRIVATE,
                    fdi, phdr->p_offset - frag) )
                err_exit(8);
        }
        // Linux does not fixup the low end, so neither do we.
        // Indeed, must leave it alone because some PT_GNU_RELRO
        // dangle below PT_LOAD (but still on the low page)!
        //if (PROT_WRITE & prot) {
        //    bzero(addr, frag);  // fragment at lo end
        //}
        frag = (-mlen) & frag_mask;  // distance to next page boundary
        if (PROT_WRITE & prot) { // note: read-only .bss not supported here
            bzero(mlen+addr, frag);  // fragment at hi end
        }
        if (xi) {
            void *const hatch = make_hatch_x86(phdr, reloc);
            if (0!=hatch) {
                /* always update AT_NULL, especially for compressed PT_INTERP */
                auxv_up((Elf32_auxv_t *)(~1 & (int)av), AT_NULL, (unsigned)hatch);
            }
            if (0!=mprotect(addr, mlen, prot)) {
                err_exit(10);
ERR_LAB
            }
        }
        addr += mlen + frag;  /* page boundary on hi end */
        if (addr < haddr) { // need pages for .bss
            if (addr != mmap_privanon(addr, haddr - addr, prot, MAP_FIXED)) {
                err_exit(9);
            }
        }
        else if (xi) { // cleanup if decompressor overrun crosses page boundary
            mlen = frag_mask & (3+ mlen);
            if (mlen<=3) { // page fragment was overrun buffer only
                munmap(addr, mlen);
            }
        }
    }
    if (xi && ET_DYN!=ehdr->e_type) {
        // Needed only if compressed shell script invokes compressed shell.
        do_brk(v_brk);
    }
    if (0!=p_reloc) {
        *p_reloc = reloc;
    }
    return ehdr->e_entry + reloc;
}

DEBUG_STRCON(STR_upx_main,
    "upx_main av=%%p  szc=%%x  f_dec=%%p  f_unf=%%p  "
    "  xo=%%p(%%x %%p)  xi=%%p(%%x %%p)  dynbase=%%x\\n")

/*************************************************************************
// upx_main - called by our entry code
//
// This function is optimized for size.
**************************************************************************/

void *upx_main(
    Elf32_auxv_t *const av,
    unsigned const sz_compressed,
    f_expand *const f_decompress,
    f_unfilter */*const*/ f_unfilter,
    Extent xo,
    Extent xi,
    unsigned const volatile dynbase,
    unsigned const sys_munmap
) __asm__("upx_main");
void *upx_main(
    Elf32_auxv_t *const av,
    unsigned const sz_compressed,
    f_expand *const f_decompress,
    f_unfilter */*const*/ f_unf,
    Extent xo,  // {sz_unc, ehdr}    for ELF headers
    Extent xi,  // {sz_cpr, &b_info} for ELF headers
    unsigned const volatile dynbase,  // value+result: compiler must not change
    unsigned const sys_munmap
)
{
    Elf32_Ehdr *const ehdr = (Elf32_Ehdr *)(void *)xo.buf;  // temp char[MAX_ELF_HDR+OVERHEAD]
    Elf32_Phdr const *phdr = (Elf32_Phdr const *)(1+ ehdr), *zhdr = phdr;
    Elf32_Addr reloc;
    Elf32_Addr entry;

    // sizeof(Ehdr+Phdrs),   compressed; including b_info header
    size_t const sz_pckhdrs = xi.size;
    unsigned char hp=xi.size-12;

    DPRINTF((STR_upx_main(),
        av, sz_compressed, f_decompress, f_unf, &xo, xo.size, xo.buf,
        &xi, xi.size, xi.buf, dynbase));
    f_unf = (f_unfilter *)(2+ (long)f_decompress);

    // Uncompress Ehdr and Phdrs.
    unpackExtent(&xi, &xo, f_decompress, 0,hp);

    // Prepare to decompress the Elf headers again, into the first PT_LOAD.
    xi.buf  -= sz_pckhdrs;
    xi.size  = sz_compressed;

    // Some kernels omit AT_PHNUM,AT_PHENT,AT_PHDR because this stub has no PT_INTERP.
    // That is "too much" optimization.  Linux 2.6.x seems to give all AT_*.
    //auxv_up(av, AT_PAGESZ, PAGE_SIZE);  /* ld-linux.so.2 does not need this */
    auxv_up(av, AT_PHNUM , ehdr->e_phnum);
    auxv_up(av, AT_PHENT , ehdr->e_phentsize);
    {
        while (PT_LOAD!=zhdr->p_type) ++zhdr;  // skip ARM PT_EXIDX and others
        auxv_up(av, AT_PHDR  , dynbase + (unsigned)(1+(Elf32_Ehdr *)zhdr->p_vaddr));
    }
    // AT_PHDR.a_un.a_val  is set again by do_xmap if PT_PHDR is present.
    // This is necessary for ET_DYN if|when we override a prelink address.

    (void)sys_munmap;  // UNUSED
    entry = do_xmap((int)f_decompress, ehdr, &xi, av, &reloc, f_unf,hp);
    auxv_up(av, AT_ENTRY , entry);

  { // Map PT_INTERP program interpreter
    int j;
    for (j=0; j < ehdr->e_phnum; ++phdr, ++j) if (PT_INTERP==phdr->p_type) {
        int const fdi = open(reloc + (char const *)phdr->p_vaddr, O_RDONLY, 0);
        if (0 > fdi) {
            err_exit(18);
        }
        if (MAX_ELF_HDR!=read(fdi, (void *)ehdr, MAX_ELF_HDR)) {
ERR_LAB
            err_exit(19);
        }
        entry = do_xmap(fdi, ehdr, 0, av, &reloc, 0,hp);
        auxv_up(av, AT_BASE, reloc);  // uClibc and musl
        close(fdi);
        break;
    }
  }
    return (void *)entry;
}

