#ifndef PCSC_MACRO_H__
#define PCSC_MACRO_H__

#undef PCSCLITE_HP_DROPDIR
#define PCSCLITE_HP_DROPDIR "/usr/local/lib/pcsc/drivers"

#undef BUNDLE
#define BUNDLE "ifd-ccid.bundle"

#undef PCSC_ARCH
#define PCSC_ARCH "Linux"

#undef USE_IPCDIR
#define USE_IPCDIR "/tmp/.scardsvr"

#endif
