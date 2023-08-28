#include <iostream>
#include <LzmaDec.h>

#include "vFile.h"
#include "Alloc.h"

#include "LzmaLib.h"
#include "7zFile.h"

//#include<half.h>
typedef unsigned short float16;
float float16ToFloat(float16 h) {
    uint16_t* p = (uint16_t*)(&h);
    uint32_t fval = 0;
    fval |= (*p & 0x8000) << 16;				// sign
    uint32_t mant = *p & 0x03ff;
    uint32_t exp = (*p & 0x7c00) >> 10;			// exponential
    if (exp == 0x1f) {							// NaN or Infinity
        fval |= mant ? 0x7fc00000 : 0x7f800000;
    }
    else if (exp > 0) {							// normalized
        fval |= (exp + 0x70) << 23;
        if (mant != 0) {
            fval |= mant << 13;
        }
    }
    else if (mant != 0) {							// denormarlized
        for (int i = 9; i >= 0; i--) {
            if (mant & (1 << i)) {
                fval |= ((0x67 + i) << 23) | ((mant << (23 - i)) & 0x7fffff);
                break;
            }
        }
    }
    // else;									// 0.0, -0.0

    return *((float*)&fval);
}

float16 floatToFloat16(float f) {
    uint32_t* p = (uint32_t*)(&f);
    uint16_t hval = 0;
    hval |= (*p >> 16) & 0x8000;				// sign
    int32_t mant = *p & 0x7fffff;
    uint16_t exp = (*p >> 23) & 0xff;
    if (exp == 0xff) {							// NaN or Infinity
        hval |= mant ? 0x7e00 : 0x7c00;
    }
    else if (exp >= 0x8f) {						// overflow, Infinity instead
        hval |= 0x7c00;
    }
    else if (exp >= 0x71) {						// normalized
        hval |= ((exp - 0x70) << 10) | (mant >> 13);
    }
    else if (exp >= 0x67) {						// denormalized
        hval |= (mant | 0x800000) >> (0x7e - exp);
    }
    // else;									// 0.0, -0.0 or loss of precision

    return *((float16*)&hval);
}

// 顶点数据 float 型，xyz 3 个值
//     ArrayProperty name=P;interpretation=point;datatype=float32_t[3];arraysize=25921;numsamps=72
// 面数数据,只存储一份，表示面是几边形，一般是 3/4边形
//     ArrayProperty name=.faceCounts;interpretation=;datatype=int32_t;arraysize=25600;numsamps=72
// 面数据是顶点索引， int 型，4边面 4 个顶点一个面，3角形3个顶点
//     ArrayProperty name=.faceIndices;interpretation=;datatype=int32_t;arraysize=102400;numsamps=72
//     ArrayProperty name=.indices;interpretation=;datatype=uint32_t;arraysize=102400;numsamps=72
// 面法线，每个面一个，xyz 
//     ArrayProperty name=N;interpretation=normal;datatype=float32_t[3];arraysize=102400;numsamps=45
// uv 数据, uv 2 个值，float
//     ArrayProperty name=.vals;interpretation=vector;datatype=float32_t[2];arraysize=25921;numsamps=72

void read_bin() {
    const char* fn = "E:\\Projects\\maya\\aaa\\abc\\bin\\1228800\\1.bin";
    //fn = "E:\\Projects\\maya\\aaa\\abc\\bin\\409600\\0.bin"; //face, vert index, int
    //fn = "E:\\Projects\\maya\\aaa\\abc\\bin\\207368\\0.bin"; //uv,  float 0.0 ~ 1.0
    //fn = "E:\\Projects\\maya\\aaa\\abc\\bin\\1228800\\0.bin"; //
    //fn = "E:\\Projects\\maya\\aaa\\abc\\bin_a1_nonor\\311052\\0.bin"; //

    FILE* fp = fopen(fn, "rb");
    if (fp) {
        float x = 0, y = 1, z = 0;
        unsigned char* p = (unsigned char*)&x;
        //for (int i = 0; i < 17408; i++) {
        while (!feof(fp)) {
            size_t n = fread(&x, 1, sizeof(x), fp);
            n = fread(&y, 1, sizeof(x), fp);
            n = fread(&z, 1, sizeof(x), fp);
            if (n > 0)
                printf("%f,%f,%f %02x%02x%02x%02x\n", x, y, z, p[3], p[2], p[1], p[0]);
        }
        fclose(fp);
    }
    else {
        printf("open file failed, %s\n", fn);
    }
    exit(0);
}

size_t fsize(FILE* fp)
{
    long n;
    fpos_t fpos;
    fgetpos(fp, &fpos);
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fsetpos(fp, &fpos);
    return n;
}
char* fread(std::string path, char*buf, size_t* len) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (fp) {
        size_t flen = fsize(fp);
        char* p = buf ? buf: new char[flen];
        size_t n = fread(p, 1, flen, fp);
        fclose(fp);
        *len = flen;
        return p;
    }
    else {
        printf("open file failed, %s\n", path.c_str());
    }
    return NULL;
}
int fwrite(std::string path, char* buf, size_t len) {
    FILE* fp = fopen(path.c_str(), "wb");
    if (fp) {
        size_t n = fwrite((const void*)buf, 1, len, fp);
        fclose(fp);
    }
    else {
        printf("open file failed, %s\n", path.c_str());
    }
    return 0;
}
void combo_bin() {
    int fnum = 72, fLen = 311052;
    char path[256]; // = "E:\\Projects\\maya\\aaa\\abc\\bin_a1_nonor\\311052\\";
    //path = "E:\\Projects\\maya\\aaa\\abc\\bin_a1_nonor\\207368\\"; fLen = 207368;
    //sprintf(path, "E:\\Projects\\maya\\aaa\\abc\\bin_a1_nonor\\%d\\", fLen);
    fLen = 1228800; fnum = 45;  //法线
    fLen = 207368; fnum = 27;   //uv
    fLen = 311052; fnum = 45;   //顶点
    sprintf(path, "E:\\Projects\\maya\\aaa\\abc\\bin\\%d\\", fLen);
    char fn[256];
    int totalLen = fLen * fnum;
    float* fdata = (float*)malloc(totalLen);
    if (fdata) {
        memset(fdata, 0, totalLen);
        for (int i = 0; i < fnum; i++) {
            sprintf(fn, "%s%d.bin", path, i);
            FILE* fp = fopen(fn, "rb");
            if (fp) {
                for (int j = 0; j < fLen / sizeof(float); j++) {
                    unsigned char* p = (unsigned char*)&fdata[j * fnum + i];
                    size_t n = fread(p, 1, sizeof(float), fp);
                }
                fclose(fp);
            }
            else {
                printf("open file failed, %s\n", fn);
                return;
            }
        }
#if 1
        for (int j = 0; j < fLen / sizeof(float); j++) {
            float* pf = &fdata[j * fnum];
            float f1 = *pf;
            for (int i = 1; i < fnum; i++) {
                float f2 = pf[i];
                pf[i] = f2 - f1;
                uint16_t f16 = floatToFloat16(pf[i]);
                uint16_t* pf16 = (uint16_t*)&pf[i];
                *pf16++ = f16;
                *pf16 = 0;

                float f32 = float16ToFloat(f16)+f1;
                float delta = f32 - f2;
                if (abs(delta) > 0.0005) {
                    printf("f32=%f, f16=%f, delta=%f\n", f2, f32, delta);
                }
                //printf("%f, %f\n", pf[i], f32);
            }
        }
#endif
        sprintf(fn, "%sdata.bin", path);
        FILE* fp = fopen(fn, "wb");
        if (fp) {
            fwrite(fdata, 1, totalLen, fp);
            fclose(fp);
        }
        free(fdata);
    }
//    exit(0);
}

int compress_bin() {
    int fLen = 207368;
    fLen = 311052;
    char path[256];// = "E:\\Projects\\maya\\aaa\\abc\\bin\\207368\\data.bin";
    char path_out[256];// = "E:\\Projects\\maya\\aaa\\abc\\bin\\207368\\data.zabc";
    sprintf(path, "E:\\Projects\\maya\\aaa\\abc\\bin\\%d\\data.bin", fLen);
    sprintf(path_out, "E:\\Projects\\maya\\aaa\\abc\\bin\\%d\\data.zabc", fLen);
    sprintf(path, "E:\\wuma.f16");
    sprintf(path_out, "E:\\wuma.f16.abcz");

    FILE*fp = fopen(path, "rb+"); 
    if (fp == NULL)
        return printf( "Cannot open input file\n");

    long Dsize = fsize(fp); 
    printf("lzma file size:%llu\n", Dsize);
    unsigned char* Dfile = (unsigned char*)malloc(Dsize);

    fread(Dfile, 1, Dsize, fp); 

    unsigned char* Lfile = (unsigned char*)malloc(Dsize);

    unsigned long long Lsize = Dsize;
    int res = LzmaCompress(Lfile, &Lsize, Dfile, Dsize); 
    if (res != SZ_OK)
    {
        printf("lzma failed to compress, res:%d", res);
        return res;
    }
    free(Dfile);
    printf("lzma success, new file size:%llu\n", Lsize);
    fclose(fp);

    fp = fopen(path_out, "wb+");
    if (fp == NULL)
        return printf("Cannot open or creat file\n");

    fwrite(Lfile, 1, Lsize, fp);
    fclose(fp);
    free(Lfile);
    return 0;
}
int decompress_bin() {
    char path[256] = "E:\\Projects\\maya\\aaa\\abc\\bin\\data1.abcz";
    char path_out[256] = "E:\\Projects\\maya\\aaa\\abc\\bin\\data1.dat";
    FILE*fp = fopen(path, "rb+");
    if (fp == NULL)
        return printf("Cannot open input file\n");

    long Esize = fsize(fp);  /* 获取待解压的文件大小 */
    printf("Pack size:%lu\n", Esize);

    Byte* Efile = (Byte*)malloc(Esize);  /* 创建待解压的文件内存 */
    fread(Efile, 1, Esize, fp);  /* 将待解压的文件写入内存 */

    Byte Probs[LZMA_PROPS_SIZE + 8];  /* 放入文件头 */

    memcpy(Probs, Efile, sizeof(Probs)); /* 获取文件头 */
    Esize -= sizeof(Probs);

    // for (int i = 0; i < sizeof(Probs); i++)
    // {
    //     printf("%02X ", (unsigned char)Probs[i]);
    // }

    UInt64 unpack_size = 0;
    for (int i = 0; i < 8; i++)
    {
        unpack_size |= (unsigned char)Probs[LZMA_PROPS_SIZE + i] << (i * 8);
    }

    CLzmaDec state;
    size_t inPos = 0, inSize = 0, outPos = 0;
    ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
    ELzmaStatus status;

    Byte inBuf[1024];
    Byte outBuf[1024];

    LzmaDec_Construct(&state);
    LzmaDec_Allocate(&state, Probs, LZMA_PROPS_SIZE, &g_Alloc);
    LzmaDec_Init(&state);
    vFile* vpf = vfopen(Efile + sizeof(Probs), Esize);

    fp = fopen(path_out, "wb+");
    if (fp == NULL)
        return printf("Cannot open input file\n");

    for (;;)
    {
        // printf("inPos:%lu ", inPos);
        if (inPos == inSize)
        {
            inSize = sizeof(inBuf);
            inSize = vfread(vpf, inBuf, inSize);
            inPos = 0;
            // printf("inSize:%lu ", inSize);
        }
        {
            SizeT inProcessed = inSize - inPos;
            SizeT outProcessed = sizeof(outBuf) - outPos;
            ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
            ELzmaStatus status;

            if (outProcessed > unpack_size)
            {
                outProcessed = (SizeT)unpack_size;
                finishMode = LZMA_FINISH_END;
            }

            int res = LzmaDec_DecodeToBuf(&state, outBuf + outPos, &outProcessed,
                inBuf + inPos, &inProcessed, finishMode, &status);
            inPos += inProcessed;
            outPos += outProcessed;
            unpack_size -= outProcessed;

            if (outPos > 0)
            {
                // printf("outPos:%lu\n", outPos);
                fwrite(outBuf, 1, outPos, fp);  /* 写入结果文件 */
                fseek(fp, 0, SEEK_END);
            }
            outPos = 0;

            if (res != SZ_OK)
            {
                printf("Unpack failed, exit:%d\n", res);
                break;
            }
            if (inProcessed == 0 && outProcessed == 0)
            {

                if (status != LZMA_STATUS_FINISHED_WITH_MARK)
                {
                    printf("Unpack success, new file size: %lu\n", ftell(fp));
                    res = SZ_ERROR_DATA;
                }
                break;
            }
        }
    }
    free(Efile);
    fclose(fp);
    return 0;
}

int compress_float0(float* frame1, float* frame2, size_t count) {
    size_t len = 0;
    char *p1 = fread("E:\\Projects\\maya\\aaa\\abc\\bin_a1_1\\20172\\0.bin", NULL, &len);
    char* p2 = fread("E:\\Projects\\maya\\aaa\\abc\\bin_a1_1\\20172\\1.bin", NULL, &len);
    frame1 = (float*)p1;
    frame2 = (float*)p2;
    count = len / sizeof(float);
    //float16* frame16 = (float16*)frame2;
    float16* frame16 = new float16[count*2];
    float* b1 = new float[count];
    float* b2 = new float[count];
    memcpy(b1, frame2, sizeof(float) * count);
    memset(frame16, 0, sizeof(float16) * count);
    for (int i = 0; i < count; i++) {
        b1[i] -= frame1[i];
        frame16[i] = floatToFloat16(b1[i]);
        //frame1[i] += float16ToFloat(frame16[i]);    // prepare next frame data
    }
    memcpy(frame2, frame16, sizeof(float) * count);

    for (int i = 0; i < count; i++) {
        b2[i] = frame1[i] + float16ToFloat(frame16[i]);
        float delta = b2[i] - frame1[i];
        printf("%d: %f\n", i, delta);
    }
    delete b1;
    delete b2;
    delete p1;
    delete p2;
    return 0;
}
int compress_float(float* frame1, float* frame2, size_t count) {
    float16* frame16 = new float16[count*2];
    memset(frame16, 0, sizeof(float16) * count*2);
    for (int i = 0; i < count; i++) {
        frame2[i] -= frame1[i];
        frame16[i] = floatToFloat16(frame2[i]);
        frame1[i] += float16ToFloat(frame16[i]);    // prepare next frame data
    }
    memcpy(frame2, frame16, sizeof(float) * count);

    delete[] frame16;
    return 0;
}
int decompress_float(float* frame1, float* frame2, size_t count) {
    float16* frame16 = (float16*)frame2;
    for (size_t i = count; i >0; i--) {
        frame2[i-1] = frame1[i-1] + float16ToFloat(frame16[i-1]);
    }
    return 0;
}

void compress_test() {
    std::string path = "E:\\Projects\\maya\\aaa\\abc\\bin_a1_1\\20172\\";
    std::string file1 = path + "0.bin";
    std::string file2 = path + "1.bin";
    std::string file3 = path + "2.bin";
    size_t len = 0;
    char* p1 = fread(file1, NULL, &len);
    char* p2 = fread(file2, NULL, &len);
    char* p3 = fread(file3, NULL, &len);

    size_t count = len / sizeof(float);

    float* f1 = (float*)p1;
    float* f2 = (float*)p2;
    float* f3 = (float*)p3;
    for (int i = 0; i < count; i++) {
        float d1 = f2[i] - f1[i];
        float d2 = f3[i] - f2[i];
        printf("%d: %f, %f\n", i, d1, d2);
    }

    compress_float((float*)p1, (float*)p2, count);
    compress_float((float*)p1, (float*)p3, count);
    //fwrite(file2 + "z", p2, len);
    //fwrite(file3 + "z", p3, len);


}
void decompress_test() {
    std::string path = "E:\\Projects\\maya\\aaa\\abc\\bin_a1_1\\20172\\";
    std::string file1 = path + "0.bin";
    std::string file2 = path + "1.binz";
    std::string file3 = path + "2.binz";
    size_t len = 0;
    char* p1 = fread(file1, NULL, &len);
    char* p2 = fread(file2, NULL, &len);
    char* p3 = fread(file3, NULL, &len);

    size_t count = len / sizeof(float);

    decompress_float((float*)p1, (float*)p2, count);
    decompress_float((float*)p1, (float*)p3, count);
    float* f1 = (float*)p1;
    float* f2 = (float*)p2;
    float* f3 = (float*)p3;
    for (int i = 0; i < count; i++) {
        float d1 = f2[i] - f1[i];
        float d2 = f3[i] - f2[i];
        printf("%d: %f, %f\n", i, d1, d2);
    }

    //fwrite(f2 + "z", p2, len);
    //fwrite(f3 + "z", p3, len);
}
typedef struct ABCZ_BLOCK {
    uint32_t    frameCount;
    uint32_t    floatCountPerFrame;
    //uint64_t    blocksize;  // block data size
    uint64_t    pos[0];        // frame data pos
}ABCZ_BLOCK;

int restore(char* abcFile, const char* f16File) {
    FILE* fpAbc = fopen(abcFile, "r+b");
    FILE* fp16 = fopen(f16File, "r+b");
    if (fpAbc && fp16) {
        ABCZ_BLOCK block;
        while (!feof(fp16)) {
            fpos_t fpos;
            fgetpos(fp16, &fpos);
            printf("fp16 pos: %lld\n", fpos);
            if (fread(&block, sizeof(block), 1, fp16) == 1) {
                if (block.frameCount == 0 || block.floatCountPerFrame == 0) {
                    printf("data error.\n");
                    return -1;
                }
                uint64_t* framePos = new uint64_t[block.frameCount];
                if (fread(framePos, sizeof(uint64_t), block.frameCount, fp16) == block.frameCount) {
                    uint64_t dataCount = (block.frameCount -1) * block.floatCountPerFrame;
                    float* frame1 = new float[block.floatCountPerFrame];
                    //float* floatData = new float[dataCount];
                    uint16_t* f16Data = new uint16_t[dataCount];
                    fread(f16Data, sizeof(uint16_t), dataCount, fp16);
                    if (f16Data) {
                        if (framePos[0] == 0) {
                            printf("wrong frame data.\n");
                            return -1;
                        }
                        fseek(fpAbc, framePos[0], SEEK_SET);
                        if (fread(frame1, sizeof(float), block.floatCountPerFrame, fpAbc) == block.floatCountPerFrame) {
                            printf("floatCountPerFrame=%d, frameCount=%d, frame1:%llx\n", block.floatCountPerFrame, block.frameCount, framePos[0]);

                        }
                        uint16_t* pf16 = f16Data;
                        for (int frameIndex = 1; frameIndex < block.frameCount; ++frameIndex) {
                            if (framePos[frameIndex] == 0) {
                                printf("skip empty frame.\n");
                                continue;
                            }
                            // orginal xyz
                            // global xxxyyyzzz
                            // frame xxxyyyzzz
                            // f1(p1)f2(p2)
                            for (int floatIndex = 0; floatIndex < block.floatCountPerFrame; floatIndex++) {
                                int index = floatIndex * (block.frameCount - 1) + frameIndex - 1;
                                if (index > dataCount) {
                                    printf("out of data, index=%d, dataCount=%d\n", index, dataCount);
                                }
                                frame1[floatIndex] += float16ToFloat(f16Data[index]);
                                //frame1[floatIndex] += float16ToFloat(pf16[0]);  //xyz
                                pf16++;
                            }

                            fseek(fpAbc, framePos[frameIndex], SEEK_SET);
                            if (fwrite(frame1, sizeof(float), block.floatCountPerFrame, fpAbc) != block.floatCountPerFrame) {
                                printf("");
                            }
                        }
                    }

                    delete[] frame1;
                    //delete[] floatData;
                    delete[] f16Data;
                }
                delete[] framePos;
}
        }
    }
    if (fpAbc)fclose(fpAbc);
    if (fp16)fclose(fp16);
    return 0;
}

//e:\Projects\maya\abc\chr_DaJiaZhang.chr_DaJiaZhang_rig.render_mesh.abc
int main2(){
#if 0
    float f2 = 1.0;
    uint16_t f16 = floatToFloat16(f2);
    float f32 = float16ToFloat(f16);
    printf("%f, %f\n", f2, f32);
    fflush(stdout);
    //half f1(f2);
    return 0;
#endif
    //read_bin();
    //combo_bin();
    //compress_bin();
    //decompress_bin();

    return 0;
}
