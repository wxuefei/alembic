//-*****************************************************************************
// Copyright (c) 2023-2023
// Compress name=P, name=N, name=uv(.vals) section data in abc
//-*****************************************************************************

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreAbstract/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/Util/All.h>
#include <Alembic/Abc/TypedPropertyTraits.h>
#include <Alembic/Ogawa/IStreams.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <conio.h>
#include <stdio.h>
#include <sys/stat.h>
#include <direct.h>
#include <corecrt_io.h>
//-*****************************************************************************
using namespace Alembic::AbcGeom;
using namespace Alembic::Ogawa;
using namespace Alembic;
using namespace std;
#define REORDER
//-*****************************************************************************
void visitProperties(ICompoundProperty);
int compress_float(float*, float*, size_t);
int decompress_float(float* frame1, float* frame2, size_t count);

typedef struct ABCZ_FILE {
    char tag[4];            // ABCZ
    uint64_t    filesize;   // block data size
    uint32_t    blockcount; // block count
}ABCZ_FILE;
typedef struct ABCZ_BLOCK {
    uint32_t    frameCount;
    uint32_t    floatCountPerFrame;
    //uint64_t    blocksize;  // block data size
    uint64_t    pos[0];        // frame data pos
}ABCZ_BLOCK;
enum COMPRESS_MODE {
    CM_FLOAT32,
    CM_FLOAT32_REORDER,
    CM_FLOAT16,
    CM_FLOAT16_REORDER,
};
static int compress_mode = CM_FLOAT32;
static FILE* gfp = NULL;
static bool demcompress = false;
static char* archive_addr = NULL;
static uint64_t archive_length = 0;
static bool isGeom = false;
uint32_t totalsize_max = 0;
map<uint64_t, uint32_t> addrMap;   // addr, size
map<uint32_t, uint32_t> sizeMap;   // size, num
//-*****************************************************************************

/*******************************************************************************
 * 文件结构：
 *  方案一：使用 ABCZ_BLOCK 结构存储压缩数据，这样的好处是不用遍历原 ABC 数据，释放数据快一点
 *  方案二：使用原 ABC 索引数据顺序存储，好处是没有额外数据，需要遍历 ABC 数据稍微慢一点。
 * 数据压缩：
 *  压缩率：                      mache   wuma    a1    dajiazhang
 *  方案一：原始 xyz 顺序，       33%     35%     23%   20%
 *  方案二：帧内 xxxyyyzzz，      39%     51%     23%   28%
 *  方案三：全局 xxxyyyzzz,       39%     51%     23%   28%
 *  方案四：f1(p1)f2(p1)f3(p1),   22%     23%     22%   14%
 *******************************************************************************/
void visitSimpleArrayProperty(IArrayProperty iProp){
    size_t asize = 0;

    const AbcA::DataType& dt = iProp.getDataType();
    AbcA::ArraySamplePtr samp;
    uint64_t pos0 = 0;
    uint64_t oPos = 0, oSize = 0;
    index_t maxSamples = iProp.getNumSamples();
    bool isPoint = false, isNormal = false, isuv = false;
    if (isGeom) {
        isPoint = iProp.getName() == "P";
        isNormal = iProp.getName() == "N";
    } else {
        isuv = isUV(iProp.getHeader());
    }
    addrMap.clear();
    sizeMap.clear();
    for ( index_t i = 0 ; i < maxSamples; ++i ){
        if (isPoint || isNormal || isuv) {
            iProp.getPtr()->getSamplePos(i, oPos, oSize);
            if (oSize > 0xffffffff) {
                continue;//should never happen
            }
            if (addrMap.count(oPos) == 0) {
                addrMap[oPos] = (uint32_t)oSize;
                sizeMap[(uint32_t)oSize] ++;
            }
            else {
                printf("skip repeated: i=%lld, pos= %llx, size=%lld\n", i, oPos, oSize);
            }
        }
        if (asize == 0) 
        {
            iProp.get(samp, ISampleSelector(i));
            asize = samp->size();
            pos0 = oPos;
        }
    }

    size_t numsamps = iProp.getNumSamples();
    size_t sampsize = asize * iProp.getDataType().getNumBytes();
    size_t totalsize = sampsize * numsamps;

    if (totalsize > totalsize_max)totalsize_max = totalsize;
    if (dt.getPod() == kFloat32POD && oSize > 10240) {  // compress the data block size greater than 10k
        //for (const auto& it : addrMap) {
        //    uint64_t pos = it.first;
        //    uint32_t size = it.second;
        //    uint32_t count = sizeMap[size];
        //    if (count > 1) {    // && size > 1024
        //        printf("write pos:%llx, size: %u, count=%u\n", pos, size, count);
        //        //*(archive_addr + oPos) = 1;
        //        //memset((archive_addr + pos), 0, size);
        //    }
        //    else
        //        printf("skip pos:%llx, size: %u, count=%u\n", pos, size, count);
        //}
        //
        float* pf = (float*)samp.get()->getData();
        size_t floatCount = asize * dt.getExtent();
        
        float* frameBuf = new float[floatCount];
        size_t bufSize  = floatCount * (maxSamples - 1) * sizeof(float) + (maxSamples * sizeof(uint64_t) + sizeof(ABCZ_BLOCK)); // ABCZ_BLOCK + uint64_t[]
        size_t abczSize = floatCount * (maxSamples - 1) * sizeof(uint16_t) + (maxSamples * sizeof(uint64_t) + sizeof(ABCZ_BLOCK)); // ABCZ_BLOCK + uint64_t[]
        char* buf = new char[bufSize];
        ABCZ_BLOCK* block = (ABCZ_BLOCK*)buf;
        uint16_t* fp16 = (uint16_t*)(buf+sizeof(ABCZ_BLOCK) + maxSamples * sizeof(uint64_t));
        memset(buf, 0, bufSize);
        block->frameCount = maxSamples;
        block->floatCountPerFrame = floatCount;
        block->pos[0] = pos0;
        int nn = 0;
        if (gfp) {
            fpos_t fpos;
            fgetpos(gfp, &fpos);
            printf("gfp pos: %lld\n", fpos);
        }
        int segSize = 3 * asize;
        uint16_t* x = fp16;
        uint16_t* y = x + asize * (maxSamples - 1);
        uint16_t* z = y + asize * (maxSamples - 1);

        for (index_t frameIndex = 1; frameIndex < maxSamples; ++frameIndex) {
            uint64_t oPos = 0, oSize = 0;
            iProp.getPtr()->getSamplePos(frameIndex, oPos, oSize);
            uint32_t count = sizeMap[oSize];
            if (count > 1 && dt.getExtent() == 3){
                //printf("write pos:%llx, size:\t%d\n", sizeof(float) * count);
                float* frame = (float*)(archive_addr+oPos);
                if (demcompress) {
                    decompress_float(pf, frame, floatCount);
                    memcpy(pf, frame, sizeof(float) * floatCount);
                }
                else {
                    memcpy(frameBuf, frame, floatCount * sizeof(float));
                    compress_float(pf, frameBuf, floatCount);
                    //memset(frame, 0, sizeof(float)* floatCount);
#ifdef REORDER
                    nn++;
                    block->pos[frameIndex] = oPos;

                    uint16_t* frame16 = (uint16_t*)frameBuf;
#if 0
                    // global xxxyyyzzz
                    for (int vIndex = 0; vIndex < asize; vIndex++) {
                        *x++ = frame16[vIndex * 3 + 0];
                    }
                    for (int vIndex = 0; vIndex < asize; vIndex++) {
                        *y++ = frame16[vIndex * 3 + 1];
                    }
                    for (int vIndex = 0; vIndex < asize; vIndex++) {
                        *z++ = frame16[vIndex * 3 + 2];
                    }
#elif 0
                    for (int vIndex = 0; vIndex < asize; vIndex++) {
                        *fp16++ = frame16[vIndex * 3 + 0];
                    }
                    for (int vIndex = 0; vIndex < asize; vIndex++) {
                        *fp16++ = frame16[vIndex * 3 + 1];
                    }
                    for (int vIndex = 0; vIndex < asize; vIndex++) {
                        *fp16++ = frame16[vIndex * 3 + 2];
                    }
#elif 1
                    for (int floatIndex = 0; floatIndex < floatCount; floatIndex++) {   // f1(p1) f2(p1)....
                        int index = floatIndex * (maxSamples-1) + frameIndex - 1;
                        fp16[index] = frame16[floatIndex];
                    }
#else
                    memcpy(fp16 + (frameIndex - 1) * floatCount, frameBuf, floatCount * sizeof(uint16_t));   //xyz
#endif
#else
                    memcpy(fp16 + (frameIndex - 1) * floatCount, frameBuf, floatCount * sizeof(uint16_t));   //xyz
                    if (gfp) {
                        fwrite(frameBuf, sizeof(float)/2, floatCount, gfp);
                    }
#endif
                }
            }
        }
#ifdef REORDER
        if (gfp) {
            fwrite(buf, 1, abczSize, gfp);
        }
#endif
        delete[] buf;
        if (maxSamples != nn+1) {
            printf("nn=%d, maxSamples=%d\n", nn, maxSamples);
        }
    }

}

//-*****************************************************************************
void visitSimpleScalarProperty(IScalarProperty iProp, const string &iIndent )
{}

//-*****************************************************************************
void visitCompoundProperty( ICompoundProperty iProp){
    bool oldIsGeom = isGeom;
    isGeom = iProp.getName() == ".geom";

    visitProperties( iProp );

    isGeom = oldIsGeom;

}

//-*****************************************************************************
void visitProperties( ICompoundProperty iParent){
    for ( size_t i = 0 ; i < iParent.getNumProperties() ; i++ ){
        PropertyHeader header = iParent.getPropertyHeader( i );
        string name = header.getName();
        if ( header.isCompound()){
            visitCompoundProperty( ICompoundProperty( iParent, name) );
        }else if ( header.isScalar()){
            //visitSimpleScalarProperty( IScalarProperty( iParent, name) );
        }else if (header.isArray()) {
            visitSimpleArrayProperty( IArrayProperty( iParent, name));
        }else {
            //wxf
        }
    }
}

template<typename T>
void GetMinAndMaxTime(T& Schema, float& MinTime, float& MaxTime)
{
    Alembic::AbcCoreAbstract::TimeSamplingPtr TimeSampler = Schema.getTimeSampling();
    MinTime = (float)TimeSampler->getSampleTime(0);
    MaxTime = (float)TimeSampler->getSampleTime(Schema.getNumSamples() - 1);
}
template<typename T>
void GetStartTimeAndFrame(T& Schema, float& StartTime, int& StartFrame)
{
    Alembic::AbcCoreAbstract::TimeSamplingPtr TimeSampler = Schema.getTimeSampling();

    StartTime = (float)TimeSampler->getSampleTime(0);
    Alembic::AbcCoreAbstract::TimeSamplingType SamplingType = TimeSampler->getTimeSamplingType();
    // We know the seconds per frame, so if we take the time for the first stored sample we can work out how many 'empty' frames come before it
    // Ensure that the start frame is never lower that 0
    StartFrame = (int)ceil(StartTime / (float)SamplingType.getTimePerCycle());
}

//-*****************************************************************************
void visitObject( IObject iObj)
{
    // Object has a name, a full name, some meta data,
    // and then it has a compound property full of properties.
    string path = iObj.getFullName();

    if ( path != "/" )
    {
        cout << "Object " << "name=" << path << endl;
    }

    // Get the properties.
    ICompoundProperty props = iObj.getProperties();
    visitProperties( props );
    if (IPolyMesh::matches(iObj.getMetaData())) //wxf
    {
        IPolyMesh mesh(iObj);
        IPolyMeshSchema ms = mesh.getSchema();
        size_t NumSamples = ms.getNumSamples();
        float MinTime = 0, MaxTime = 0;
        int StartFrameIndex = 0;
        GetMinAndMaxTime(ms, MinTime, MaxTime);
        GetStartTimeAndFrame(ms, MinTime, StartFrameIndex);

        printf("mesh: %s,NumSamples=%llu, StartFrameIndex=%u, MinTime=%f, MaxTime=%f\n", iObj.getName().c_str(), NumSamples, StartFrameIndex, MinTime, MaxTime);
    }

    // now the child objects
    for ( size_t i = 0 ; i < iObj.getNumChildren() ; i++ )
    {
        IObject child(iObj, iObj.getChildHeader(i).getName());
        visitObject( child );
    }
}

void mkdirs(char* muldir)
{
    size_t i, len;
    char str[512] = { 0 };
    strncpy(str, muldir, 512);
    str[511] = 0;
    len = strlen((const char*)str);
    for (i = 0; i < len; i++){
        if (str[i] == '\\'){
            str[i] = '\0';
            if (access(str, 0) != 0){
                mkdir((const char*)str);
            }
            str[i] = '\\';
        }
    }
    if (len > 0 && access(str, 0) != 0){
        mkdir(str);
    }
    return;
}

static char bin_path[256] = { 0 };

void setBinPath(char* filename) {
    char* p = strrchr(filename, '\\');
    if (p)p++;
    else p = filename;
    strcpy(bin_path, "bin_");
    strcat(bin_path, p);
}
uint64_t fsize(char* filename) {
    uint64_t ret = 0;
    struct stat fileStat;
    if (stat(filename, &fileStat) == 0) {
        if ((fileStat.st_mode & S_IFMT) == S_IFREG) {
            ret = (uint64_t)fileStat.st_size;
        }
    }
    return ret;
}
//-*****************************************************************************
int abcz(int argc, char* argv[]) {
    if (argc != 2) {
        char* exe = argv[0];
        char*p = strrchr(exe, '\\');
        if (p)exe = p + 1;
        cerr << "USAGE: " << exe << " <filename.abc>" << endl;
        exit(-1);
    }
    char* filename = argv[1];

    setBinPath(filename);
    //setDumpData(dumpData);
    Alembic::AbcCoreFactory::IFactory factory;
    factory.setPolicy(ErrorHandler::kQuietNoopPolicy);
    IArchive archive = factory.getArchive(filename);

    cout << "AbcZ for " << Alembic::AbcCoreAbstract::GetLibraryVersion() << endl;
    if (archive){
        string appName, libraryVersionString, whenWritten, userDescription;
        uint32_t libraryVersion;
        GetArchiveInfo(archive, appName, libraryVersionString, libraryVersion, whenWritten, userDescription);
        archive_addr = (char*)archive.getPtr()->getMemoryMapPtr();
        archive_length = fsize(filename);
        const int TimeSamplingIndex = archive.getNumTimeSamplings() > 1 ? 1 : 0;

        printf("archive NumTimeSamplings=%d\n", archive.getNumTimeSamplings());
        AbcA::TimeSamplingPtr tsp = archive.getTimeSampling(TimeSamplingIndex);
        printf("Time sampling: NumStoredTimes=%llu\n", tsp->getNumStoredTimes());
        printf("Time sampling: SampleTime=%f\n", tsp->getSampleTime(0));
        const std::vector < chrono_t >& st = tsp->getStoredTimes();
        printf("Stored times: getStoredTimes=%lld, startFrame=%f\n", st.size(), st[0]);
        TimeSamplingType tst = tsp->getTimeSamplingType();
        chrono_t cycle = tst.getTimePerCycle();
        float fps = 1 / cycle;
        printf("cycle=%f, fps=%.1f\n", cycle, fps);
        index_t maxSample = archive.getMaxNumSamplesForTimeSamplingIndex(TimeSamplingIndex);
        printf("startFrame=%f, %lld\n", st[0]/cycle, maxSample);

        visitObject(archive.getTop());
    }

    printf("abcz done\n");

    return 0;
}
void compress_test();
void decompress_test();
int compress_bin();
int restore(char* abcFile, const char* f16File);
//-*****************************************************************************
int main( int argc, char *argv[] ){

    auto now = std::chrono::system_clock::now();

    //compress_bin();    exit(0);

    //compress_test();
    //decompress_test();
    char* filename = argv[1];
    filename = "1plane.abc";
    filename = "1plane_tri.abc";
    filename = "a1_nonormals.abc";
    filename = "a1_1.abc";
    filename = "chr_DaJiaZhang.abc";      // totalsize_max: 87,700,200
    //filename = "chr_WuMa.abc";            // totalsize_max: 177,828,336
    //filename = "DaJiaZhangMaCheA.abc";    // totalsize_max: 1,481,316,480
    //demcompress = true;
    string f16_file = ""; // "E:\\";
    f16_file += filename;
    f16_file += ".f16";
#if 1   //compress
    gfp = fopen(f16_file.c_str(), "wb");
    char* argv2[] = { NULL, filename };

    argc = 2;
    abcz(argc, argv2);
#else   //decompress
    restore(filename, f16_file.c_str());
#endif
    auto duration = std::chrono::system_clock::now() - now;
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    printf("totalsize_max: %u\n", totalsize_max);
    printf("spent time: %.2fs\n", ((float)ms) / 1000);
    if(gfp)fclose(gfp);
    return 0;
}
