#ifndef _M_TYPE__
#define _M_TYPE__

#ifdef VMRP
#include "../../header/types.h"
#else
typedef char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;


typedef int ptrdiff_t;

typedef unsigned int  size_t;
typedef unsigned int  uintptr_t;
typedef long long  intmax_t;

typedef int BOOL;

#define FALSE 0
#define TRUE 1

#ifndef NULL
#define NULL (void*)0
#endif

#ifndef offsetof
#define offsetof(type, field) ((size_t) & ((type *)0)->field)
#endif
#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

#endif // VMRP

#endif // _M_TYPE__
