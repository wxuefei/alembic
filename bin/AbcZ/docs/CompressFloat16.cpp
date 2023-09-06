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
    uint64_t    pos;        // block target pos
    uint64_t    blocksize;  // block data size
}ABCZ_BLOCK;
FILE* gfp = NULL;
static bool demcompress = false;
static char* archive_addr = NULL;
static uint64_t archive_length = 0;
static bool isGeom = false;
uint32_t totalsize_max = 0;
map<uint64_t, uint32_t> addrMap;   // addr, size
map<uint32_t, uint32_t> sizeMap;   // size, num
//-*****************************************************************************
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
        size_t bufSize  = floatCount * (maxSamples-1) * sizeof(float);
        char* buf = new char[bufSize];
        uint16_t* fp16 = (uint16_t*)buf;
        memset(buf, 0, bufSize);
        for (index_t i = 1; i < maxSamples; ++i) {
            uint64_t oPos = 0, oSize = 0;
            iProp.getPtr()->getSamplePos(i, oPos, oSize);
            uint32_t count = sizeMap[oSize];
            if (count > 1) {
                //printf("write pos:%llx, size:\t%d\n", sizeof(float) * count);
                float* frame = (float*)(archive_addr+oPos);
                if (demcompress) {
                    decompress_float(pf, frame, floatCount);
                    memcpy(pf, frame, sizeof(float) * floatCount);
                }
                else {
                    memcpy(frameBuf, frame, floatCount * sizeof(float));
                    compress_float(pf, frameBuf, floatCount);
#ifdef REORDER
                    uint16_t* frame16 = (uint16_t*)frameBuf;
                    for (int floatIndex = 0; floatIndex < floatCount; floatIndex++) {
                        fp16[floatIndex * maxSamples + i - 1] = frame16[floatIndex];
                    }
                    //memcpy(buf + (i - 1) * (floatCount * sizeof(uint16_t), frameBuf, floatCount * sizeof(uint16_t));
#else
                    if (gfp) {
                        fwrite(frameBuf, sizeof(float)/2, floatCount, gfp);
                    }
#endif
                    //memset(frame, 0, sizeof(float)*count);
                }
            }
        }
#ifdef REORDER
        if (gfp) {
            fwrite(buf, 1, bufSize / 2, gfp);
        }
#endif
        delete[] buf;
    }

}

//-*****************************************************************************
void visitSimpleScalarProperty(IScalarProperty iProp, const string &iIndent )
{
    //string ptype = "ScalarProperty ";
    //size_t asize = 0;

    //const AbcA::DataType &dt = iProp.getDataType();
    //const Alembic::Util ::uint8_t extent = dt.getExtent();
    //Alembic::Util::Dimensions dims( extent );
    //AbcA::ArraySamplePtr samp = AbcA::AllocateArraySample( dt, dims );
    //index_t maxSamples = iProp.getNumSamples();
    //for ( index_t i = 0 ; i < maxSamples; ++i ){
    //    iProp.get( const_cast<void*>( samp->getData() ), ISampleSelector( i ) );
    //    asize = samp->size();
    //};

    //cout << iIndent << "  " << ptype << "name=" << iProp.getName()
    //    << ";interpretation=" << iProp.getMetaData().get("interpretation")
    //    << ";datatype=" << dt << ", size=" << dt.getNumBytes()
    //    << ";arraysize=" << asize
    //    << ";numsamps=" << iProp.getNumSamples() << endl;
}

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
//-*****************************************************************************
int main( int argc, char *argv[] ){

    auto now = std::chrono::system_clock::now();

    //compress_bin();    exit(0);

    //compress_test();
    //decompress_test();
    gfp = fopen("e:\\abc.bin", "wb");
    char* filename = argv[1];
    filename = "1plane.abc";
    filename = "1plane_tri.abc";
    filename = "a1_nonormals.abc";
    filename = "a1_1.abc";
    filename = "chr_DaJiaZhang.abc";      // totalsize_max: 87,700,200
    filename = "chr_WuMa.abc";            // totalsize_max: 177,828,336
    filename = "DaJiaZhangMaCheA.abc";    // totalsize_max: 1,481,316,480
    //demcompress = true;
    char* argv2[] = { NULL, filename };
    argc = 2;
    abcz(argc, argv2);

    auto duration = std::chrono::system_clock::now() - now;
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    printf("totalsize_max: %u\n", totalsize_max);
    printf("spent time: %.2fs\n", ((float)ms) / 1000);
    if(gfp)fclose(gfp);
    return 0;
}
