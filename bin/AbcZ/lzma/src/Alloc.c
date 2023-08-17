/* Alloc.c -- Memory allocation functions
2021-07-13 : Igor Pavlov : Public domain */

#include <stdlib.h>
#include "Alloc.h"


void *MyAlloc(size_t size)
{
    if (size == 0)
        return NULL;
    return malloc(size);
}

void MyFree(void *address)
{
    free(address);
}


static void *SzAlloc(ISzAllocPtr p, size_t size)
{
    (void)p;
    return MyAlloc(size);
}
static void SzFree(ISzAllocPtr p, void *address)
{
    (void)p;
    MyFree(address);
}

const ISzAlloc g_Alloc = { SzAlloc, SzFree };
