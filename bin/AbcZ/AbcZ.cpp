//-*****************************************************************************
// Copyright (c) 2023-2023
// Compress name=P, name=N, name=uv(.vals) section data in abc
//-*****************************************************************************

#include "AbcZ.h"
#include "Float16.h"
#include <lib7zr.h>
#include <chrono>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <windows.h>

#define ABCZ_TMP_DIR ".abcz"
#define ABCZ_TMP_BASE "\\a.abc"
#define ABCZ_TMP_PATCH "\\b"

/*******************************************************************************
 * 文件结构：
 *  方案一：使用 ABCZ_BLOCK 结构存储压缩数据，这样的好处是不用遍历原 ABC 数据，释放数据快一点
 *  方案二：使用原 ABC 索引数据顺序存储，好处是没有额外数据，需要遍历 ABC 数据稍微慢一点。
 * 数据压缩：
 *  压缩率：                      mache   wuma    a1    dajiazhang
 *  方案一：原始 xyz 顺序，       33%     35%     23%   20%
 *  方案二：帧内 xxxyyyzzz，      39%     51%     23%   28%
 *  方案三：全局 xxxyyyzzz,       39%     51%     23%   28%
 *  方案四：f1.p1.x f2.p1.x....   22%     23%     22%   14%
 *******************************************************************************/
void AbcZ::visitSimpleArrayProperty(IArrayProperty iProp) {
    size_t asize = 0;
    uint64_t pos0 = 0;
    uint64_t oPos = 0, oSize = 0;
    bool isPoint = false, isNormal = false, isuv = false;
    AbcA::ArraySamplePtr samp;
    const AbcA::DataType& dt = iProp.getDataType();
    size_t maxSamples = iProp.getNumSamples();

    if (isGeom) {
        isPoint = iProp.getName() == "P";
        isNormal = iProp.getName() == "N";
    } else {
        isuv = isUV(iProp.getHeader());
    }
    addrMap.clear();
    sizeMap.clear();
    bool* skipFrame = new bool[maxSamples];
    memset(skipFrame, 0, maxSamples);
    for (size_t i = 0 ; i < maxSamples; ++i ){
        if (isPoint || isNormal || isuv) {
            iProp.getPtr()->getSamplePos(i, oPos, oSize);
            if (oSize > 0xffffffff) {   // frame data larger than 4g ? should never happen
                continue;
            }
            if (addrMap.count(oPos) == 0) {
                addrMap[oPos] = (uint32_t)oSize;
                sizeMap[(uint32_t)oSize] ++;
            }
            else {
                skipFrame[i] = true;
                printf("skip repeated: i=%lld, pos= %llx, size=%lld\n", i, oPos, oSize);
            }
        }
        if (asize == 0){
            iProp.get(samp, ISampleSelector((index_t)i));
            asize = samp->size();
            pos0 = oPos;
        }
    }

    size_t sampsize = asize * iProp.getDataType().getNumBytes();
    size_t totalsize = sampsize * maxSamples;

    //if (totalsize > totalsize_max)totalsize_max = totalsize;
    if (dt.getPod() == kFloat32POD && oSize > 10240) {  // compress the data block that size greater than 10k
        float* frame0 = (float*)samp.get()->getData(); // first frame data
        size_t floatCount = asize * dt.getExtent();
        
        float* frameBuf = new float[floatCount];
        size_t bugetFileSize  = floatCount * (maxSamples - 1) * sizeof(float) + (maxSamples * sizeof(uint64_t) + sizeof(ABCZ_BLOCK)); // ABCZ_BLOCK + uint64_t[]
        size_t abczSize = floatCount * (maxSamples - 1) * sizeof(uint16_t) + (maxSamples * sizeof(uint64_t) + sizeof(ABCZ_BLOCK)); // ABCZ_BLOCK + uint64_t[]
        char* buf = new char[bugetFileSize];
        ABCZ_BLOCK* block = (ABCZ_BLOCK*)buf;
        uint16_t* pf16 = (uint16_t*)(buf + sizeof(ABCZ_BLOCK) + maxSamples * sizeof(uint64_t));
        memset(buf, 0, bugetFileSize);
        block->frameCount = maxSamples;
        block->floatCountPerFrame = floatCount;
        block->pos[0] = pos0;
        int processedFrame = 0;
        if (fpPatch) {
            fpos_t fpos;
            fgetpos(fpPatch, &fpos);
            printf("fpPatch pos: %lld\n", fpos);
        }
        size_t frameDataSize = sizeof(float) * floatCount;
        for (size_t frameIndex = 1; frameIndex < maxSamples; ++frameIndex) {
            iProp.getPtr()->getSamplePos(frameIndex, oPos, oSize);
            if(skipFrame[frameIndex]){
                printf("SKIP repeated: i=%lld, pos= %llx, size=%lld\n", frameIndex, oPos, oSize);
                continue;
            }

            uint32_t count = sizeMap[oSize];
            printf("write frame %lld, pos: 0x%llx, size: %lld, count=%d\n", frameIndex, oPos, frameDataSize,count);
            if (count > 1 && dt.getExtent() == 3){
                float* frame = (float*)(archiveAddr + oPos);
                memcpy(frameBuf, frame, frameDataSize);
                covertToFloat16(frame0, frameBuf, floatCount);
                memset(frame, 0, frameDataSize);

                processedFrame++;
                block->pos[frameIndex] = oPos;
                uint16_t* frame16 = (uint16_t*)frameBuf;
                for (int floatIndex = 0; floatIndex < floatCount; floatIndex++) {   // f1.p1.x f2.p1.x....
                    size_t index = floatIndex * (maxSamples-1) + frameIndex - 1;
                    pf16[index] = frame16[floatIndex];
                }
            }
        }

        if (fpPatch) {
            fwrite(buf, 1, abczSize, fpPatch);
        }

        delete[] buf;
        delete[] frameBuf;
        delete[] skipFrame;
        if (maxSamples != processedFrame +1) {
            printf("processedFrame=%d, maxSamples=%lld\n", processedFrame, maxSamples);
        }
    }

}

//-*****************************************************************************
void AbcZ::visitSimpleScalarProperty(IScalarProperty iProp, const string &iIndent )
{}

//-*****************************************************************************
void AbcZ::visitCompoundProperty( ICompoundProperty iProp){
    bool oldIsGeom = isGeom;
    isGeom = iProp.getName() == ".geom";

    visitProperties( iProp );

    isGeom = oldIsGeom;

}

//-*****************************************************************************
void AbcZ::visitProperties( ICompoundProperty iParent){
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
void AbcZ::GetMinAndMaxTime(T& Schema, float& MinTime, float& MaxTime)
{
    Alembic::AbcCoreAbstract::TimeSamplingPtr TimeSampler = Schema.getTimeSampling();
    MinTime = (float)TimeSampler->getSampleTime(0);
    MaxTime = (float)TimeSampler->getSampleTime(Schema.getNumSamples() - 1);
}
template<typename T>
void AbcZ::GetStartTimeAndFrame(T& Schema, float& StartTime, int& StartFrame)
{
    Alembic::AbcCoreAbstract::TimeSamplingPtr TimeSampler = Schema.getTimeSampling();

    StartTime = (float)TimeSampler->getSampleTime(0);
    Alembic::AbcCoreAbstract::TimeSamplingType SamplingType = TimeSampler->getTimeSamplingType();
    // We know the seconds per frame, so if we take the time for the first stored sample we can work out how many 'empty' frames come before it
    // Ensure that the start frame is never lower that 0
    StartFrame = (int)ceil(StartTime / (float)SamplingType.getTimePerCycle());
}

//-*****************************************************************************
void AbcZ::visitObject( IObject iObj)
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

void AbcZ::mkdirs(char* muldir){
    size_t i, len;
    char str[512] = { 0 };
    strncpy(str, muldir, 512);
    str[511] = 0;
    len = strlen((const char*)str);
    for (i = 0; i < len; i++){
        if (str[i] == '\\'){
            str[i] = '\0';
            if (_access(str, 0) != 0){
                _mkdir((const char*)str);
            }
            str[i] = '\\';
        }
    }
    if (len > 0 && _access(str, 0) != 0){
        _mkdir(str);
    }
    return;
}

bool AbcZ::checkFreeSpace(char* filename) {
    uint64_t needSize = getFileSize(filename) * 3;
    bool ret = makeTempPath(needSize);
    return ret;
}

uint64_t AbcZ::getFileSize(char* filename) {
    uint64_t ret = 0;
    struct stat fileStat;
    if (stat(filename, &fileStat) == 0) {
        if ((fileStat.st_mode & S_IFMT) == S_IFREG) {
            ret = (uint64_t)fileStat.st_size;
        }
    }
    return ret;
}
int AbcZ::copyfile(const char*filename, const char* dstFile) {
    int ret = 0;
    bool isabc = false;
    size_t t1 = 0, t2 = 0;
    FILE* fp1 = fopen(filename, "rb");
    if (fp1) {
        FILE* fp2 = fopen(dstFile, "wb");
        if (fp2) {
            char *buf = new char[1024*128];
            while (!feof(fp1)) {
                size_t readLen = fread(buf, 1, sizeof(buf), fp1);
                if (readLen <= 0)
                    break;
                if (!isabc) {
                    if (strncmp(buf, "Ogawa", 5) == 0)
                        isabc = true;
                    else {
                        ret = -1;
                        break;
                    }
                }
                t1 += readLen;
                size_t writeLen = fwrite(buf, 1, readLen, fp2);
                if (writeLen <= 0)
                    break;
                t2 += writeLen;
            }
            delete[] buf;
            fclose(fp2);
        }
        fclose(fp1);
    }
    if (t1 != t2) {
        ret = -2;
    }
    return ret;
}

uint64_t AbcZ::getDrvFreeSpace(char drv) {
    ULARGE_INTEGER ab = { 0 }, tb = { 0 }, fb = { 0 };
    TCHAR drvPath[4] = { drv, ':', '\\', 0 };
    GetDiskFreeSpaceEx(drvPath, &ab, &tb, &fb);
    return ab.QuadPart;
}

bool AbcZ::makeTempPath(uint64_t needSize) {
    tmpPath = ABCZ_TMP_DIR;
    if (_access(tmpPath.c_str(), 0) == 0) {
        return true;
    }
#if 0
    TCHAR szCurPath[MAX_PATH] = { 0 };
    PCHAR pszDrive = szCurPath;
    GetCurrentDirectory(MAX_PATH, szCurPath);
    uint64_t size = getDrvFreeSpace(szCurPath[0]);
    if (size < needSize) {
        GetLogicalDriveStrings(MAX_PATH - 1, szCurPath);
        while (*pszDrive) {
            UINT dt = GetDriveType(pszDrive);
            if (dt == DRIVE_FIXED) {
                uint64_t size = getDrvFreeSpace(pszDrive[0]);
                if (size > needSize) {
                    tmpPath = pszDrive;  // "C:\" "D:\" 
                    tmpPath += ABCZ_TMP_DIR;
                    break;
                }
            }
            pszDrive += lstrlen(pszDrive) + 1;
        }
    }
#endif
    int err = _mkdir(tmpPath.c_str());
    if (err == 0)
        return true;
    return false;
}
int AbcZ::removeTempPath(string abcFile, string patchFile) {
    int ret = 0;
    ret = remove(abcFile.c_str());
    ret = remove(patchFile.c_str());
    ret = _rmdir(tmpPath.c_str());
    return ret;
}
int AbcZ::covertToFloat16(float* frame1, float* frame2, size_t count) {
    float16* frame16 = new float16[count * 2];
    memset(frame16, 0, sizeof(float16) * count * 2);
    for (int i = 0; i < count; i++) {
        frame2[i] -= frame1[i];
        frame16[i] = float32To16(frame2[i]);
        frame1[i] += float16To32(frame16[i]);    // prepare next frame data
    }
    memcpy(frame2, frame16, sizeof(float) * count);

    delete[] frame16;
    return 0;
}
int AbcZ::covertToFloat(float* frame1, float* frame2, size_t count) {
    float16* frame16 = (float16*)frame2;
    for (size_t i = count; i > 0; i--) {
        frame2[i - 1] = frame1[i - 1] + float16To32(frame16[i - 1]);
    }
    return 0;
}
/*
*/
int AbcZ::reorderAbcData(string abcFile, string patchFile) {
    int ret = 0;
    Alembic::AbcCoreFactory::IFactory factory;
    factory.setPolicy(ErrorHandler::kQuietNoopPolicy);
    IArchive archive = factory.getArchive(abcFile);

    if (!archive) {
        ret = -2; // wrong abc
        return ret;
    }
    else {
        fpPatch = fopen(patchFile.c_str(), "wb");
        if (!fpPatch) {
            ret = -3;
        }
        else {
            string appName, libraryVersionString, whenWritten, userDescription;
            uint32_t libraryVersion;
            GetArchiveInfo(archive, appName, libraryVersionString, libraryVersion, whenWritten, userDescription);
            archiveAddr = (char*)archive.getPtr()->getMemoryMapPtr();
            const int TimeSamplingIndex = archive.getNumTimeSamplings() > 1 ? 1 : 0;

            printf("archive NumTimeSamplings=%d\n", archive.getNumTimeSamplings());
            AbcA::TimeSamplingPtr tsp = archive.getTimeSampling(TimeSamplingIndex);
            printf("Time sampling: NumStoredTimes=%llu\n", tsp->getNumStoredTimes());
            printf("Time sampling: SampleTime=%f\n", tsp->getSampleTime(0));
            const std::vector < chrono_t >& st = tsp->getStoredTimes();
            printf("Stored times: getStoredTimes=%lld, startFrame=%f\n", st.size(), st[0]);
            TimeSamplingType tst = tsp->getTimeSamplingType();
            chrono_t cycle = tst.getTimePerCycle();
            chrono_t fps = 1 / cycle;
            float startFrame = st[0] / cycle;
            index_t maxSample = archive.getMaxNumSamplesForTimeSamplingIndex(TimeSamplingIndex);
            printf("cycle=%f, fps=%.1f\n", cycle, fps);
            printf("startFrame=%f, %lld\n", startFrame, maxSample);

            visitObject(archive.getTop());
            fclose(fpPatch);

        }
    }
    return ret;
}
/*
*/
int AbcZ::compress(char* filename) {
    int ret = 0;
    if (!checkFreeSpace(filename)) {
        printf("Disk space is not enough\n");
        return -1;
    }
    try {
        string abcFile = tmpPath + ABCZ_TMP_BASE;
        string patchFile = tmpPath + ABCZ_TMP_PATCH;

        printf("begin copy....\n");
        if (0 != copyfile(filename, abcFile.c_str())) {
            printf("copy failed\n");
            return -2;
        }
        ret = reorderAbcData(abcFile, patchFile);
        if (ret != 0) {
            printf("reorderAbcData failed\n");
            return -3;
        }
        string outFile = makeAbczFilename(filename);
        ret = compressTo7z(outFile, abcFile, patchFile);

        removeTempPath(abcFile, patchFile);
    }
    catch (...) {
        printf("Fatal exception!!\n");
    }
    return ret;
}
/*
* generate abcz filename for compress
*/
string AbcZ::makeAbczFilename(char* filename) {
    string fn(filename);
    if (fn.length() < 4) {
        fn += ".abcz";
    }
    else {
        std::string ext = fn.substr(fn.length() - 4);
        for (int i = 0; i < ext.length(); ++i) {
            ext[i] = std::tolower(ext[i]);
        }

        if (ext.compare(".abc") == 0) {
            fn += "z";
        }
        else
            fn += ".abcz";
    }
    return fn;
}
/*
* generate abc filename for decompress
*/
string AbcZ::makeAbcFilename(char* filename) {
    string fn(filename);
    if (fn.length() > 5) {
        std::string ext = fn.substr(fn.length() - 5);
        for (int i = 0; i < ext.length(); ++i) {
            ext[i] = std::tolower(ext[i]);
        }

        if (ext.compare(".abcz") == 0) {
            fn = fn.substr(0, fn.length() - ext.length());
        }
    }

    string s = fn + ".abc";
    if (_access(s.c_str(), 0) == 0) {
        char tmpFile[256] = { 0 };
        for (int i = 1; i < 10000; i++) {
            sprintf(tmpFile, "%s(%d).abc", fn.c_str(), i);
            if (_access(tmpFile, 0) != 0) {
                s = tmpFile;
                break;
            }
         }
    }

    return s;
}

int AbcZ::restore(char* abcFile, const char* patchFile) {
    FILE* fpAbc = fopen(abcFile, "r+b");
    if (!fpAbc) {
        return -1;
    };
    fpPatch = fopen(patchFile, "r+b");
    if (!fpPatch) {
        fclose(fpAbc);
        return -2;
    }
    ABCZ_BLOCK block;
    while (!feof(fpPatch)) {
        fpos_t fpos;
        fgetpos(fpPatch, &fpos);
        printf("fpPatch pos: %lld\n", fpos);
        if (fread(&block, sizeof(block), 1, fpPatch) == 1) {
            if (block.frameCount == 0 || block.floatCountPerFrame == 0) {
                printf("data error.\n");
                return -1;
            }
            uint64_t* framePos = new uint64_t[block.frameCount];
            if (fread(framePos, sizeof(uint64_t), block.frameCount, fpPatch) == block.frameCount) {
                uint64_t dataCount = (block.frameCount - 1) * block.floatCountPerFrame;
                float* frame1 = new float[block.floatCountPerFrame];
                //float* floatData = new float[dataCount];
                uint16_t* f16Data = new uint16_t[dataCount];
                fread(f16Data, sizeof(uint16_t), dataCount, fpPatch);
                if (f16Data) {
                    if (framePos[0] == 0) {
                        printf("The first frame data is wrong, please check it!!\n");
                        return -1;
                    }
                    _fseeki64(fpAbc, framePos[0], SEEK_SET);
                    if (fread(frame1, sizeof(float), block.floatCountPerFrame, fpAbc) == block.floatCountPerFrame) {
                        printf("floatCountPerFrame=%d, frameCount=%d, frame1:%llx\n", block.floatCountPerFrame, block.frameCount, framePos[0]);
                    }
                    uint16_t* pf16 = f16Data;
                    for (uint32_t frameIndex = 1; frameIndex < block.frameCount; ++frameIndex) {
                        if (framePos[frameIndex] == 0) {
                            printf("skip empty frame.\n");
                            continue;
                        }
                        // orginal xyz
                        // global xxxyyyzzz
                        // frame xxxyyyzzz
                        // f1(p1)f2(p2)
                        for (uint32_t floatIndex = 0; floatIndex < block.floatCountPerFrame; floatIndex++) {
                            int index = floatIndex * (block.frameCount - 1) + frameIndex - 1;
                            if (index > dataCount) {
                                printf("out of data, index=%d, dataCount=%lld\n", index, dataCount);
                            }
                            frame1[floatIndex] += float16To32(f16Data[index]);
                            //frame1[floatIndex] += float16To32(pf16[0]);  //xyz
                            pf16++;
                        }

                        _fseeki64(fpAbc, framePos[frameIndex], SEEK_SET);
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
    fclose(fpAbc);
    fclose(fpPatch);
    return 0;
}

int AbcZ::decompress(char* filename) {
    int ret = 0;
    if (!checkFreeSpace(filename)) {
        printf("Disk space is not enough\n");
        return -1;
    }
    try{
        if (_access(filename, 0) != 0) {
            return -1;
        }
        ret = lib7z_list(filename);
        if (ret != 0 || lib7z_list_num() < 2) {
            printf("wrong abcz file\n");
            return -2;
        }
        string abcBase(ABCZ_TMP_DIR ABCZ_TMP_BASE);
        string abcPatch(ABCZ_TMP_DIR ABCZ_TMP_PATCH);
        for (int i = 0; i < lib7z_list_num(); i++) {
            char path[256];
            unsigned __int64 filesize, packsize;
            lib7z_list_get(i, path, filesize, packsize);
            //printf("%s\t%lld\t%lld\n", path, filesize, packsize);
            if (abcBase.compare(path) != 0 && abcPatch.compare(path) != 0) {
                printf("wrong abcz file\n");
                return -3;
            }
        }

        printf("begin decompress....\n");
        ret = lib7z_decompress(filename);
        if (ret != 0) {
            printf("decompress error\n");
            return -4;
        }
        ret = restore(ABCZ_TMP_DIR ABCZ_TMP_BASE, ABCZ_TMP_DIR ABCZ_TMP_PATCH);
        if (ret != 0) {
        }
        string abcFilename = makeAbcFilename(filename);
        ret = rename(abcBase.c_str(), abcFilename.c_str());

        removeTempPath(abcBase, abcPatch);
    }
    catch (...)
    {
        ret = -3; // (NExitCode::kFatalError);
    }
    return ret;
}
int AbcZ::compressTo7z(string outFile, string abcFile, string patchFile){
    int res = 0;
    try
    {
        printf("begin compress....\n");
        //res = lib7z_compress("a.7z", "e:\chr_WuMa.abc.f16", "e:\chr_WuMa.abc.f16");
        res = lib7z_compress(outFile.c_str(), abcFile.c_str(), patchFile.c_str());

        if (res != 0) {
            printf("compress err %d\n", res);
        }
    }
    catch (...)
    {
        return -1; // (NExitCode::kFatalError);
    }
    return 0;
}

//-*****************************************************************************
void usage(char* argv[]) {
    char* exe = argv[0];
    char* p = strrchr(exe, '\\');
    if (p)exe = p + 1;
    cerr << "Usage: " << exe << " <filename.abc>" << endl;
    cerr << "  -c       compress(default)" << endl;
    cerr << "  -d       decompress" << endl;
    exit(-1);
}
int abcz(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv);
    }

    char* filename = argv[1];

    AbcZ abcz;
    int ret = 0;
    if (argc > 2) {
        filename = argv[2];
        if (strcmp(argv[1], "-d") == 0) {
            ret = abcz.decompress(filename);
            if (0 != ret) {
                printf("decompress failed, err=%d\n", ret);
            }
            return ret;
        }
        else if (strcmp(argv[1], "-c") == 0) {
            //compress
        }
        else {
            usage(argv);
        }
    }

    ret = abcz.compress(filename);
    if (0 != ret) {
        printf("compress failed, err=%d\n", ret);
    }

    return 0;
}

