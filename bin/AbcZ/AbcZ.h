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
#include <windows.h>
#include <lib7zr.h>
#include "resource.h"
//-*****************************************************************************
using namespace Alembic::AbcGeom;
using namespace Alembic::Ogawa;
using namespace Alembic;
using namespace std;
#define REORDER
//-*****************************************************************************
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

class AbcZ {
private:
    enum COMPRESS_MODE {
        CM_FLOAT32,
        CM_FLOAT32_REORDER,
        CM_FLOAT16,
        CM_FLOAT16_REORDER,
    };

    int compress_mode = CM_FLOAT32;
    FILE* fp_patch = NULL;
    bool demcompress = false;
    char* archive_addr = NULL;
    uint64_t archive_length = 0;
    bool isGeom = false;
    uint32_t totalsize_max = 0;
    map<uint64_t, uint32_t> addrMap;   // addr, size
    map<uint32_t, uint32_t> sizeMap;   // size, num
    string tmpPath;
//-*****************************************************************************

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
    int copyfile(const char* filename, const char* abcFile);
    bool checkFreeSpace(char* filename);
    uint64_t fsize(char* filename);
    uint64_t getDrvFreeSpace(char drv);
    string getTempPath(uint64_t fs);

public:
    int compress(char* filename);
    int decompress(char* filename);
};

