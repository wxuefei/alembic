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

//-*****************************************************************************
void visitProperties( ICompoundProperty, string & );

typedef struct ABCZ_FILE {
    char tag[4];            // ABCZ
    uint64_t    filesize;   // block data size
    uint32_t    blockcount; // block count
}ABCZ_FILE;
typedef struct ABCZ_BLOCK {
    uint64_t    pos;        // block target pos
    uint64_t    blocksize;  // block data size
}ABCZ_BLOCK;

static char* archive_addr = NULL;
static uint64_t archive_length = 0;
static bool isGeom = false;
uint32_t totalsize_max = 0;
map<uint64_t, uint32_t> addrMap;   // addr, size
map<uint32_t, uint32_t> sizeMap;   // size, num
//-*****************************************************************************
void visitSimpleArrayProperty(IArrayProperty iProp, const string &iIndent ){
    string ptype = "ArrayProperty ";
    size_t asize = 0;

    const AbcA::DataType& dt = iProp.getDataType();
    AbcA::ArraySamplePtr samp;
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
            uint64_t oPos=0, oSize=0;
            iProp.getPtr()->getSamplePos(i, oPos, oSize);
            //printf("i=%lld, pos= %llx, size=%lld\n", i, oPos, oSize);
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
            //samp->getData
        }
    }

    size_t numsamps = iProp.getNumSamples();
    size_t sampsize = asize * iProp.getDataType().getNumBytes();
    size_t totalsize = sampsize * numsamps;

    cout << iIndent << "  " << ptype << "name=" << iProp.getName()
        << ";interpretation=" << iProp.getMetaData().get("interpretation")
        << ";datatype=" << dt 
        << ";arraysize=" << asize 
        << ";numsamps=" << numsamps
        << ", sample size=" << sampsize
        << ", total size=" << totalsize
        << endl;
    if (totalsize > totalsize_max)totalsize_max = totalsize;
    if (iProp.getDataType().getPod() == kFloat32POD) {
        //wxf, save object's vertex & uv
        for (const auto& it : addrMap) {
            uint64_t pos = it.first;
            uint32_t size = it.second;
            uint32_t count = sizeMap[size];
            if (count > 1) {    // && size > 1024
                printf("write pos:%llx, size: %u, count=%u\n", pos, size, count);
                //*(archive_addr + oPos) = 1;
                //memset((archive_addr + pos), 0, size);
            }
            else
                printf("skip pos:%llx, size: %u, count=%u\n", pos, size, count);
        }
    }
}

//-*****************************************************************************
void visitSimpleScalarProperty(IScalarProperty iProp, const string &iIndent )
{
    string ptype = "ScalarProperty ";
    size_t asize = 0;

    const AbcA::DataType &dt = iProp.getDataType();
    const Alembic::Util ::uint8_t extent = dt.getExtent();
    Alembic::Util::Dimensions dims( extent );
    AbcA::ArraySamplePtr samp = AbcA::AllocateArraySample( dt, dims );
    index_t maxSamples = iProp.getNumSamples();
    for ( index_t i = 0 ; i < maxSamples; ++i ){
        //iProp.get( const_cast<void*>( samp->getData() ), ISampleSelector( i ) );
        //asize = samp->size();
        //printf("i=%d, ptr: %p, size=%d\n", i, samp->getData(), asize);
    };

    stringstream msg;
    msg << iIndent << "  " << ptype << "name=" << iProp.getName()
        << ";interpretation=" << iProp.getMetaData().get("interpretation")
        << ";datatype=" << dt << ", size=" << dt.getNumBytes()
        << ";arraysize=" << asize
        << ";numsamps=" << iProp.getNumSamples() << endl;

    cout << msg.str();
}

//-*****************************************************************************
void visitCompoundProperty( ICompoundProperty iProp, string &ioIndent ){
    string ptype = "CompoundProperty ";
    string oldIndent = ioIndent;
    ioIndent += "  ";

    cout << ioIndent << ptype << "name=" << iProp.getName()
        << ";schema=" << iProp.getMetaData().get("schema") << endl;
    bool oldIsGeom = isGeom;
    isGeom = iProp.getName() == ".geom";

    visitProperties( iProp, ioIndent );

    isGeom = oldIsGeom;

    ioIndent = oldIndent;
}

//-*****************************************************************************
void visitProperties( ICompoundProperty iParent, string &ioIndent )
{
    string oldIndent = ioIndent;
    for ( size_t i = 0 ; i < iParent.getNumProperties() ; i++ )
    {
        PropertyHeader header = iParent.getPropertyHeader( i );
        string name = header.getName();
        if ( header.isCompound()){
            visitCompoundProperty( ICompoundProperty( iParent, name), ioIndent );
        }else if ( header.isScalar()){
            visitSimpleScalarProperty( IScalarProperty( iParent, name), ioIndent );
        }else if (header.isArray()) {
            visitSimpleArrayProperty( IArrayProperty( iParent, name), ioIndent );
        }else {
            //wxf
        }
    }

    ioIndent = oldIndent;
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
void visitObject( IObject iObj, string iIndent )
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
    visitProperties( props, iIndent );
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
        visitObject( child, iIndent );
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

#define sizes_max 1600
#define addr_max  (16000)
static uint64_t addr[addr_max] = { 0 };
static uint64_t addr_size[addr_max] = { 0 };
static int no[sizes_max] = { 0 };
static int sizes[sizes_max] = { 0 }; //20172*144 207368*72 1228Microsoft Visual Studio Community 2019800*45
static char bin_path[128] = { 0 };

void dumpData(char* p, uint64_t iPos, uint64_t iSize) {
    char path[256];
    char filename[128];
    printf("\tdump: pos=%llx, size=%lld", iPos, iSize);
    if (iPos == 0)archive_addr = p;
    if (iSize == 4) { // read num or length
        uint32_t* pu32 = (uint32_t*)p;
        printf("\t\tval=0x%x, %u", *pu32, *pu32 );
    }
    else if (iSize == 8) { // read num or length
        uint64_t* pu64 = (uint64_t*)p;
        printf("\t\tval=0x%llx, %lld", *pu64, *pu64 & 0x7fffffffffffffff);
    }
    printf("\n");
    //return;
    //dump data
    int i = 0;
    for (i = 0; i < sizes_max; i++) {
        if (sizes[i] == iSize) {
            break;
        }
    }
    if (iSize > 1024 && i == sizes_max) {
        for (i = 0; i < sizes_max; i++) {
            if (sizes[i] == 0) {
                sizes[i] = (uint32_t)iSize;
                break;
            }
        }
    }
    if (i < sizes_max) {
        int n = 0;
        for (; n < addr_max; n++) {
            if (addr[n] == iPos) {
                //printf("*********\n");
                return;
            }
            else if (addr[n] == NULL) {
                addr[n] = iPos;
                addr_size[n] = iSize;
                break;
            }
        }
        if (n == addr_max) {
            n = addr_max;
        }
        sprintf(path, "E:\\Projects\\maya\\aaa\\abc\\%s\\%lld\\", bin_path, iSize);
        mkdirs(path);
        sprintf(filename, "%d_%llx.bin", no[i]++, iPos);
        strcat(path, filename);
        FILE* fp = fopen(path, "wb+");
        if (fp) {
            fwrite(p, 1, iSize, fp);
            fclose(fp);
            printf("write file: %s\n", filename);
        }
    }
    else {
        if (iSize > 1024)
            i = i;
    }
    //if (iSize == 1228800)
    //    iSize = 1228800;
}
void printDumpStat() {
    char* p = archive_addr;
    for (int i = 0; i < sizes_max; i++) {
        if (sizes[i] == 0) {
            break;
        }
        printf("printDumpStat size: %d, %d\n", sizes[i], no[i]);
    }
    for (int i = 0; i < addr_max; i++) {
        if (addr[i] == 0) {
            break;
        }
        int nn = 0;
        for (int j = 0; j < sizes_max; j++) if (sizes[j] == addr_size[i]) { nn = no[j]; break; }

        printf("printDumpStat addr: %llx, %lld", addr[i], addr_size[i]);
        if (nn > 2) {
            printf(", memset 0");
            //memset(p + addr[i], 0, addr_size[i]);
        }
        printf("\n");
    }

}
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
        cerr << "USAGE: " << exe << " <AlembicArchive.abc>" << endl;
        exit(-1);
    }
    char* filename = argv[1];
    filename = "1plane.abc";
    filename = "1plane_tri.abc";
    filename = "a1_nonormals.abc";
    filename = "a1_1.abc";
    //filename = "chr_DaJiaZhang.abc";      // totalsize_max: 87,700,200
    //filename = "chr_WuMa.abc";            // totalsize_max: 177,828,336
    //filename = "DaJiaZhangMaCheA.abc";    // totalsize_max: 1,481,316,480

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
        printf("tsp NumStoredTimes=%llu\n", tsp->getNumStoredTimes());
        printf("tsp SampleTime=%f\n", tsp->getSampleTime(0));
        const std::vector < chrono_t >& st = tsp->getStoredTimes();
        printf("st getStoredTimes=%lld, startFrame=%f\n", st.size(), st[0]);
        TimeSamplingType tst = tsp->getTimeSamplingType();
        chrono_t cycle = tst.getTimePerCycle();
        float fps = 1 / cycle;
        printf("cycle=%f, fps=%f\n", cycle, fps);
        index_t maxSample = archive.getMaxNumSamplesForTimeSamplingIndex(TimeSamplingIndex);
        printf("startFrame=%f, %lld\n", st[0]/cycle, maxSample);

        if (appName != ""){
            cout << "  file written by: " << appName << endl;
            cout << "  using Alembic : " << libraryVersionString << endl;
            cout << "  written on : " << whenWritten << endl;
            cout << "  user description : " << userDescription << endl << endl;
        }
        else{
            cout << filename << endl;
            cout << "  (file doesn't have any ArchiveInfo)" << endl << endl;
        }
        visitObject(archive.getTop(), "");
    }

    printf("abcz done\n");
    //printDumpStat();
    printf("press any key to exit.\n"); 
    //char a = getch();
    /*exit(0);*/
    return 0;
}
void testHalf(){
    //half f1;
    //f1.setBits(11);
    //printf("f1=%f\n", (float)f1);
    //return 0;
    //printDumpStat(NULL);
}
void init() {
    //system("mode con:cols=120 lines=20000");
    //SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), { 120,20000 });
    //for (int i = 0; i < 9031; i++)printf("%d\n", i);
}

//-*****************************************************************************
int main( int argc, char *argv[] ){
    init();
//    testHalf();
    //system("cmd");
    auto now = std::chrono::system_clock::now();
    
    abcz(argc, argv);

    auto duration = std::chrono::system_clock::now() - now;
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    printf("totalsize_max: %u\n", totalsize_max);
    printf("spent time: %.2fs\n", ((float)ms) / 1000);
    return 0;
}
