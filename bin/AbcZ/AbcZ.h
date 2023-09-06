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

using namespace Alembic::AbcGeom;
using namespace Alembic::Ogawa;
using namespace Alembic;
using namespace std;

class AbcZ {
private:
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

    void visitSimpleArrayProperty(IArrayProperty iProp);
    void visitSimpleScalarProperty(IScalarProperty iProp, const string& iIndent);
    void visitCompoundProperty(ICompoundProperty iProp);
    void visitProperties(ICompoundProperty iParent);

    template<typename T>
    void GetMinAndMaxTime(T& Schema, float& MinTime, float& MaxTime);
    template<typename T>
    void GetStartTimeAndFrame(T& Schema, float& StartTime, int& StartFrame);
    void visitObject(IObject iObj);

    void mkdirs(char* muldir);
    int copyfile(const char* filename, const char* dstFile);
    int restore(char* abcFile, const char* patchFile);
    bool checkFreeSpace(char* filename);
    uint64_t getFileSize(char* filename);
    uint64_t getDrvFreeSpace(char drv);
    bool makeTempPath(uint64_t fs);
    int covertToFloat16(float* frame1, float* frame2, size_t count);
    int covertToFloat(float* frame1, float* frame2, size_t count);
    int compressTo7z(string outFile, string abcFile, string f16_file);
    int removeTempPath(string abcFile, string f16_file);
    string makeAbcFilename(char* filename);
    string makeAbczFilename(char* filename);
    int reorderAbcData(string abcFile, string patchFile);
public:
    int compress(char* filename);
    int decompress(char* filename);

private:
    //int compressMode = CM_FLOAT32;
    FILE* fpPatch = NULL;
    char* archiveAddr = NULL;
    uint64_t archiveLength = 0;
    bool isGeom = false;
    map<uint64_t, uint64_t> addrMap;   // addr, size
    map<uint64_t, uint32_t> sizeMap;   // size, num
    string tmpPath;

};

