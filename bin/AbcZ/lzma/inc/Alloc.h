/* Alloc.h -- Memory allocation functions
2021-07-13 : Igor Pavlov : Public domain */

#ifndef __COMMON_ALLOC_H
#define __COMMON_ALLOC_H

#include "7zTypes.h"

extern void *MyAlloc(size_t size);
extern void MyFree(void *address);
#ifdef __cplusplus
extern "C" {
#endif
	extern const ISzAlloc g_Alloc;
#ifdef __cplusplus
}
#endif




#endif
