#ifndef MIRACL_H
#define MIRACL_H

#define MR_BYTE unsigned char
#define mr_unsign32 unsigned int

#define TRUE 1
#define FALSE 0

typedef int BOOL;

#ifdef MR_BITSINCHAR
 #if MR_BITSINCHAR == 8
  #define MR_TOBYTE(x) ((MR_BYTE)(x))
 #else
  #define MR_TOBYTE(x) ((MR_BYTE)((x)&0xFF))
 #endif
#else
 #define MR_TOBYTE(x) ((MR_BYTE)(x))
#endif


#define MR_ECB   0
#define MR_CBC   1
#define MR_CFB1  2
#define MR_CFB2  3
#define MR_CFB4  5
#define MR_PCFB1 10
#define MR_PCFB2 11
#define MR_PCFB4 13
#define MR_OFB1  14
#define MR_OFB2  15
#define MR_OFB4  17
#define MR_OFB8  21
#define MR_OFB16 29

typedef struct {
int Nk,Nr;
int mode;
mr_unsign32 fkey[60];
mr_unsign32 rkey[60];
char f[16];
} aes;


#endif
