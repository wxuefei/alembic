//-*****************************************************************************
//
// Copyright(c) 2023-2024, PASSION PAINT ANIMATION
//
//-*****************************************************************************

#include <iostream>
#include <sstream>
#include <algorithm>
#include "util.h"

using namespace std;
using namespace Alembic::AbcGeom;
using namespace Alembic::AbcCoreAbstract;
using namespace Alembic::AbcCoreFactory;

void joinSample(OCurvesSchema &oSchema, ICurvesSchema &iSchema, index_t reqIdx,int i, bool test) {
    IV2fGeomParam iUVs = iSchema.getUVsParam();
    IN3fGeomParam iNormals = iSchema.getNormalsParam();
    IFloatGeomParam iWidths = iSchema.getWidthsParam();
    //IFloatArrayProperty iKnots = iSchema.getKnotsProperty();    //control verts
    //IUcharArrayProperty iOrders = iSchema.getOrdersProperty();

    ICurvesSchema::Sample iSamp = iSchema.getValue(reqIdx);

    OCurvesSchema::Sample oSamp;
    Abc::P3fArraySamplePtr posPtr = iSamp.getPositions();
    if (posPtr) {
        if (test) {
            //float* pf = (float*)posPtr->getData();
            //printf("data: %.2f\n", pf[0]);
            oSamp.setPositions(*posPtr);
        }
        else {
            //float* pf = (float*)posPtr->getData();
            //pf[0] = -1.0;  //-10.84,169.75, 9.04
            //pf[1] = 200.0;
            //pf[2] = 20.0;
            //printf("data: %.2f\n", pf[0]);
            oSamp.setPositions(*posPtr);
        }
    }

    Abc::V3fArraySamplePtr velocPtr = iSamp.getVelocities();
    if (velocPtr)
        oSamp.setVelocities(*velocPtr);

    oSamp.setType(iSamp.getType());
    Abc::Int32ArraySamplePtr curvsNumPtr = iSamp.getCurvesNumVertices();
    if (curvsNumPtr)
        oSamp.setCurvesNumVertices(*curvsNumPtr);
    oSamp.setWrap(iSamp.getWrap());
    oSamp.setBasis(iSamp.getBasis());

    Abc::FloatArraySamplePtr knotsPtr = iSamp.getKnots();
    //size_t nc = iKnots.getNumSamples();
    //long uc = knotsPtr.use_count();
    //printf("cv %d\n", knotsPtr->size());
    if (knotsPtr) {
        oSamp.setKnots(*knotsPtr);
    }

    Abc::UcharArraySamplePtr ordersPtr = iSamp.getOrders();
    if (ordersPtr) {
        oSamp.setOrders(*ordersPtr);
    }

    IFloatGeomParam::Sample iWidthSample;
    OFloatGeomParam::Sample oWidthSample;
    if (iWidths) {
        getOGeomParamSamp <IFloatGeomParam, IFloatGeomParam::Sample, OFloatGeomParam::Sample>
            (iWidths, iWidthSample, oWidthSample, reqIdx +  0 );
        oSamp.setWidths(oWidthSample);
    }

    IV2fGeomParam::Sample iUVSample;
    OV2fGeomParam::Sample oUVSample;
    if (iUVs) {
        getOGeomParamSamp <IV2fGeomParam, IV2fGeomParam::Sample, OV2fGeomParam::Sample>(iUVs, iUVSample, oUVSample, reqIdx);
        oSamp.setUVs(oUVSample);
    }

    IN3fGeomParam::Sample iNormalsSample;
    ON3fGeomParam::Sample oNormalsSample;
    if (iNormals) {
        getOGeomParamSamp <IN3fGeomParam, IN3fGeomParam::Sample, ON3fGeomParam::Sample>(iNormals, iNormalsSample, oNormalsSample, reqIdx);
        oSamp.setNormals(oNormalsSample);
    }
    oSchema.set(oSamp);
    //return oSamp;
}

/*
* totalSamples, just used to append empty frames
*/
OObject joinCurves(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples) {
    OCurvesSchema oSchema = ceateDataSchema<ICurves, ICurvesSchema, OCurves, OCurvesSchema>
        (iObjects, oParentObj, iTimeMap, totalSamples);
    OObject outObj = oSchema.getObject();
    OCurvesSchema::Sample emptySample(P3fArraySample::emptySample(), Int32ArraySample::emptySample());
    
    // stitch the CurvesSchemas
    //
    //index_t oNumSamples = iObjects.size();
    size_t numInputObjects = iObjects.size();
    //numInputObjects = 1; //for test
    for (size_t i = 0; i < numInputObjects; i++) {
        if (!iObjects[i].valid()) {
            continue;
        }
        ICurvesSchema iSchema = ICurves(iObjects[i]).getSchema();

        index_t numSamples = iSchema.getNumSamples();
        index_t numEmpty = 0;
        index_t oNumSamples = oSchema.getNumSamples();
        //TimeSamplingPtr oTime = oSchema.getTimeSampling();
        //TimeSamplingPtr iTime = iSchema.getTimeSampling();
        index_t reqIdx = 0; //

        //reqIdx = getIndexSample(oNumSamples, oTime, numSamples, iTime, numEmpty);
        //printf("reqIdx[%d]=%lld\n", i, reqIdx);
        reqIdx = 0;
        for (index_t emptyIdx = 0; emptyIdx < numEmpty; ++emptyIdx) {
            oSchema.set(emptySample);
        }

        for (; reqIdx < numSamples; reqIdx++) {
            joinSample(oSchema, iSchema, reqIdx,i, false);
        }
    }

    for (size_t i = oSchema.getNumSamples(); i < totalSamples; ++i) {
        oSchema.set(emptySample);
    }
    return outObj;
}
//-*****************************************************************************
// a recursive function that reads all inputs and write to the given oObject
// node if there's no gap in the frame range for animated nodes
//
void visitObjects(vector<IObject>& iObjects, OObject& oParentObj, const TimeAndSamplesMap& iTimeMap, bool atRoot) {
    OObject outObj;
    IObject inObj;
    for (vector<IObject>::iterator it = iObjects.begin(); it != iObjects.end(); ++it) {
        if (it->valid()) {
            inObj = *it;
            break;
        }
    }
    assert(inObj.valid());

    if (iTimeMap.isVerbose()) {
        cout << inObj.getFullName() << endl;
    }

    const AbcA::ObjectHeader& header = inObj.getHeader();
    size_t totalSamples = 0;

    // there are a number of things that needs to be checked for each node
    // to make sure they can be properly stitched together
    //
    // for xform node:
    //      locator or normal xform node
    //      if an xform node, numOps and type of ops match static or no, and if not, timesampling type matches
    //      if sampled, timesampling type should match
    //      if sampled, no frame gaps
    //
    if (IXform::matches(header)) {
        outObj = joinXform(oParentObj, iObjects, iTimeMap, totalSamples);
    //} else if (ISubD::matches(header)) {
    //    outObj = joinSubD(oParentObj, iObjects, iTimeMap, totalSamples);
    //} else if (IPolyMesh::matches(header)) {
    //    outObj = joinPolyMesh(oParentObj, iObjects, iTimeMap, totalSamples);
    //} else if (ICamera::matches(header)) {
    //    outObj = joinCamera(oParentObj, iObjects, iTimeMap, totalSamples);
    } else if (ICurves::matches(header)) {
        outObj = joinCurves(oParentObj, iObjects, iTimeMap, totalSamples);
    //} else if (IPoints::matches(header)) {
    //    outObj = joinPoints(oParentObj, iObjects, iTimeMap, totalSamples);
    //} else if (INuPatch::matches(header)) {
    //    outObj = joinNuPatch(oParentObj, iObjects, iTimeMap, totalSamples);
    } else {
        if (!atRoot) {
            outObj = OObject(oParentObj, header.getName(), header.getMetaData());
        } else {
            // for stitching properties of the top level objects
            outObj = oParentObj;
        }

        // collect the top level compound property
        ICompoundPropertyVec iCompoundProps(iObjects.size());
        for (size_t i = 0; i < iObjects.size(); ++i) {
            if (!iObjects[i].valid()) {
                continue;
            }

            iCompoundProps[i] = iObjects[i].getProperties();
        }

        OCompoundProperty oCompoundProperty = outObj.getProperties();
        stitchCompoundProp(iCompoundProps, oCompoundProperty, iTimeMap);
    }

    // After done writing THIS OObject node, if input nodes have children,
    // go deeper, otherwise we are done here
    for (size_t i = 0; i < iObjects.size(); i++) {
        if (!iObjects[i].valid()) {
            continue;
        }

        size_t childNum = iObjects[i].getNumChildren();
        for (size_t j = 0; j < childNum; ++j) {
            vector< IObject > childObjects;
            string childName = iObjects[i].getChildHeader(j).getName();
            // skip names that we've already written out
            if (outObj.getChildHeader(childName) != NULL) {
                continue;
            }

            for (size_t k = i; k < iObjects.size(); ++k) {
                childObjects.push_back(iObjects[k].getChild(childName));
            }

            visitObjects(childObjects, outObj, iTimeMap, false);
        }
    }

}
TimeSamplingPtr g_frame1Tsp;
TimeSamplingPtr getFrame1Tsp() {
    return g_frame1Tsp;
}
//-*****************************************************************************
//-*****************************************************************************
// DO IT.
//-*****************************************************************************
//-*****************************************************************************
int main2(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: abcjoin [-v] outFile.abc inFile1.abc inFile2.abc (inFile3.abc ...)\n\n"
                "Options:\n"
                "  -v      Verbose for the IObject.\n" ;
        return -1;
    }

    {
        size_t numInputs = argc - 2;
        string fileName = argv[1];

        // look for optional verbose
        int inStart = 2;
        TimeAndSamplesMap timeMap;
        if (fileName == "-v") {
            timeMap.setVerbose(true);
            fileName = argv[2];
            inStart++;
            numInputs--;
        }

        vector< chrono_t > minVec;
        minVec.reserve(numInputs);

        vector< IArchive > iArchives;
        iArchives.reserve(numInputs);

        map< chrono_t, size_t > minIndexMap;

        IFactory factory;
        factory.setPolicy(ErrorHandler::kThrowPolicy);
        IFactory::CoreType coreType;

        for (int i = inStart; i < argc; ++i) {
            IArchive archive = factory.getArchive(argv[i], coreType);
            if (!archive.valid()) {
                cerr << "ERROR: " << argv[i] << " not a valid Alembic file" << endl;
                return 1;
            }

            // reorder the input files according to their mins
            chrono_t min = DBL_MAX;
            uint32_t numSamplings = archive.getNumTimeSamplings();
            timeMap.add(archive.getTimeSampling(0), archive.getMaxNumSamplesForTimeSamplingIndex(0));
            //printf("getMaxNumSamplesForTimeSamplingIndex=%lld\n", archive.getMaxNumSamplesForTimeSamplingIndex(0));
            if (numSamplings > 1) {
                // timesampling index 0 is special, so it will be skipped
                // use the first time on the next time sampling to determine
                // our archive order the archive order
                min = archive.getTimeSampling(1)->getSampleTime(0);

                for (uint32_t s = 1; s < numSamplings; ++s) {
                    timeMap.add(archive.getTimeSampling(s), archive.getMaxNumSamplesForTimeSamplingIndex(s));
                    TimeSamplingPtr tsp = archive.getTimeSampling(s);
                    if (!g_frame1Tsp) {
                        g_frame1Tsp = tsp;
                    }
                    //TimeSamplingType tst = tsp->getTimeSamplingType();
                    //printf("s=%d, getMaxNumSamplesForTimeSamplingIndex=%lld, TimePerCycle=%f\n", s, archive.getMaxNumSamplesForTimeSamplingIndex(s),tst.getTimePerCycle());
                }
                const int TimeSamplingIndex = numSamplings > 1 ? 1 : 0;

                AbcA::TimeSamplingPtr tsp = archive.getTimeSampling(TimeSamplingIndex);
                printf("Time sampling: NumStoredTimes=%llu\n", tsp->getNumStoredTimes());
                printf("Time sampling: SampleTime=%f\n", tsp->getSampleTime(0));
                const std::vector < chrono_t >& st = tsp->getStoredTimes();
                //printf("Stored times: getStoredTimes=%lld, startFrame=%f\n", st.size(), st[0]);
                TimeSamplingType tst = tsp->getTimeSamplingType();
                chrono_t cycle = tst.getTimePerCycle();
                chrono_t fps = 1 / cycle;
                float startFrame = st[0] / cycle;
                index_t maxSample = archive.getMaxNumSamplesForTimeSamplingIndex(TimeSamplingIndex);
                printf("frame cycle=%f, fps=%.1f\n", cycle, fps);
                printf("frame start num=%f, frame count=%lld\n\n", startFrame, maxSample);

                printf("%s start frame %f\n\n", argv[i], min / cycle);
                minVec.push_back(min);
                if (minIndexMap.count(min) == 0) {
                    minIndexMap.insert(make_pair(min, i - inStart));
                } else if (argv[inStart] != argv[i]) {
                    cerr << "ERROR: overlapping frame range between " << argv[inStart] << " and " << argv[i] << endl;
                    return 1;
                }
            }

            iArchives.push_back(archive);
        }

        // now reorder the input nodes so they are in increasing order of their
        // min values in the frame range
        sort(minVec.begin(), minVec.end());
        vector< IArchive > iOrderedArchives;
        iOrderedArchives.reserve(numInputs);

        for (size_t f = 0; f < numInputs; ++f) {
            size_t index = minIndexMap.find(minVec[f])->second;
            iOrderedArchives.push_back(iArchives[index]);
        }

        // since important meta data hints can be on the archive
        // and will likely be part of every input in the sequence
        // propagate the one from the first archive to our output
        MetaData md = iOrderedArchives[0].getTop().getMetaData();
        string appWriter = "AbcJoin";
        string userStr = md.get(Abc::kUserDescriptionKey);
        if (!userStr.empty()) {
            userStr = "AbcJoin: " + userStr;
        }

        // Create an archive with the default writer
        OArchive oArchive;
        if (coreType == IFactory::kOgawa) {
            oArchive = CreateArchiveWithInfo(Alembic::AbcCoreOgawa::WriteArchive(),
                           fileName, appWriter, userStr, md, ErrorHandler::kThrowPolicy);

        }
#ifdef ALEMBIC_WITH_HDF5
        else if (coreType == IFactory::kHDF5) {
            oArchive = CreateArchiveWithInfo(
                           Alembic::AbcCoreHDF5::WriteArchive(),
                           fileName, appWriter, userStr, md, ErrorHandler::kThrowPolicy);
        }
#endif

        OObject oRoot = oArchive.getTop();
        if (!oRoot.valid()) {
            return -1;
        }

        vector<IObject> iRoots(numInputs);

        for (size_t i = 0; i < numInputs; ++i) {
            iRoots[i] = iOrderedArchives[i].getTop();
        }

        visitObjects(iRoots, oRoot, timeMap, true);
    }

    return 0;
}

int main(int argc, char* argv[]) {
#if 1
    return main2(argc, argv);
#else
#if 0
    //-v m3b.abc m3\f42.abc m3\f43.abc m3\f44.abc m3\f45.abc m3\f46.abc
    char* argv2[] = { "", "-v", "m5b.abc", "m3/f42.abc", "m3/f43.abc", "m3/f44.abc", "m3/f45.abc", "m3/f46.abc" };
#elif 1
    //-v m5b.abc m5\f42.abc m5\f43.abc m5\f44.abc m5\f45.abc m5\f46.abc
    char* argv2[] = { "", "-v", "m5b.abc", "m5/f42.abc", "m5/f43.abc", "m5/f44.abc", "m5/f45.abc", "m5/f46.abc" };
#endif
    argc = sizeof(argv2) / (sizeof (char*));
    return main2(argc, argv2);
#endif
}