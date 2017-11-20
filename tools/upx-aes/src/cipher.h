#ifndef CIPHER_H
#define CIPHER_H

extern void cipher_init(void* inst, unsigned len);
extern void cipher_encrypt(void* inst,unsigned char* in, unsigned in_len, unsigned char* out, unsigned* out_len);
extern void cipher_decrypt(void* inst,unsigned char* in, unsigned in_len, unsigned char* out, unsigned* out_len);
extern void cipher_end(void *inst);

#endif 
