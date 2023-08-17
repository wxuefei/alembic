/* LzmaLib.c -- LZMA library wrapper
2015-06-13 : Igor Pavlov : Public domain */

#include "Alloc.h"
#include "LzmaDec.h"
#include "LzmaEnc.h"
#include "LzmaLib.h"
#include "string.h"

#define LZMA_PROPS_SIZE 5

/**
 * @brief
 *
 * @param dest
 * @param destLen
 * @param src
 * @param srcLen
 * @param outProps
 * @param outPropsSize
 * @return MY_STDAPI
 */
MY_STDAPI LzmaCompress(unsigned char *dest, unsigned long long *destLen, const unsigned char *src, unsigned long long srcLen)
{
    CLzmaEncHandle enc;
    SRes res;
    CLzmaEncProps props;
    SizeT len;

    enc = LzmaEnc_Create(&g_Alloc);
    if (enc == 0)
        return SZ_ERROR_MEM;

    LzmaEncProps_Init(&props);
    props.level = 9;  /* 压缩等级 0 <= level <= 9, default = 5 */
    //dictSize:[12~30],设置字典大小
    // 12,解码占用20352 = 16256 + 4096字节内存,2E12,已经是最低了
    // 13,解码占用24448 = 16256 + 8192字节内存,2E13
    // 16,解码占用81792 = 16256 + 65536字节内存,2E16
    props.dictSize = 1 << 12;  /*字典大小 use (1 << N) or (3 << N). 4 KB < dictSize <= 128 MB */
    props.writeEndMark = 1; /* 0 - do not write EOPM, 1 - write EOPM, default = 0 */
    res = LzmaEnc_SetProps(enc, &props);

    if (res == SZ_OK)
    {
        Byte header[LZMA_PROPS_SIZE + 8] = {0};
        size_t headerSize = LZMA_PROPS_SIZE;

        res = LzmaEnc_WriteProperties(enc, header, &headerSize);
        memcpy(&header[headerSize], &srcLen, sizeof(srcLen));
        headerSize += 8;
        memcpy(dest, header, headerSize);
        len = *destLen - headerSize;
        res = LzmaEnc_MemEncode(enc, dest + headerSize, &len, src, srcLen, 0, NULL, &g_Alloc, &g_Alloc);
        *destLen = len + headerSize;
    }
    LzmaEnc_Destroy(enc, &g_Alloc, &g_Alloc);
    return res;
}

/**
 * @brief
 *
 * @param dest
 * @param destLen
 * @param src
 * @param srcLen
 * @param props
 * @param propsSize
 * @return MY_STDAPI
 */
MY_STDAPI LzmaUncompress(unsigned char *dest, size_t *destLen, const unsigned char *src, size_t *srcLen,
                         const unsigned char *props, size_t propsSize)
{
    ELzmaStatus status;
    return LzmaDecode(dest, destLen, src, srcLen, props, (unsigned)propsSize, LZMA_FINISH_ANY, &status, &g_Alloc);
}
