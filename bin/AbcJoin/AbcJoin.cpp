//-*****************************************************************************
//
// Copyright(c) 2023-2024, PASSION PAINT ANIMATION
//
//-*****************************************************************************

#include <Alembic/AbcGeom/All.h>

#ifdef ALEMBIC_WITH_HDF5
#include <Alembic/AbcCoreHDF5/All.h>
#endif

#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcCoreFactory/All.h>

#include "util.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>

using namespace std;
using namespace Alembic::AbcGeom;
using namespace Alembic::AbcCoreAbstract;
using namespace Alembic::AbcCoreFactory;

template< class IPARAM, class IPARAMSAMP, class OPARAMSAMP >
void getOGeomParamSamp(IPARAM& iGeomParam, IPARAMSAMP& iGeomSamp, OPARAMSAMP& oGeomSamp, index_t iIndex) {
    if (iGeomParam.isIndexed()) {
        iGeomParam.getIndexed(iGeomSamp, iIndex);
        oGeomSamp.setVals(*(iGeomSamp.getVals()));
        oGeomSamp.setScope(iGeomSamp.getScope());
        oGeomSamp.setIndices(*(iGeomSamp.getIndices()));
    } else {
        iGeomParam.getExpanded(iGeomSamp, iIndex);
        oGeomSamp.setVals(*(iGeomSamp.getVals()));
        oGeomSamp.setScope(iGeomSamp.getScope());
    }
}

template<class IData, class IDataSchema, class OData, class ODataSchema>
void init(vector< IObject >& iObjects, OObject& oParentObj,
          ODataSchema& oSchema, const TimeAndSamplesMap& iTimeMap, size_t& oTotalSamples) {
    // find the first valid IObject
    IObject inObj;
    for (size_t i = 0; i < iObjects.size(); ++i) {
        if (iObjects[i].valid()) {
            inObj = iObjects[i];
            break;
        }
    }

    const string fullNodeName = inObj.getFullName();

    // gather information from the first input node in the list:
    IDataSchema iSchema0 = IData(inObj).getSchema();

    TimeSamplingPtr tsPtr0 = iTimeMap.get(iSchema0.getTimeSampling(), oTotalSamples);

    TimeSamplingType tsType0 = tsPtr0->getTimeSamplingType();
    checkAcyclic(tsType0, fullNodeName);

    ICompoundPropertyVec iCompoundProps;
    iCompoundProps.reserve(iObjects.size());

    ICompoundPropertyVec iArbGeomCompoundProps;
    iArbGeomCompoundProps.reserve(iObjects.size());

    ICompoundPropertyVec iUserCompoundProps;
    iUserCompoundProps.reserve(iObjects.size());

    ICompoundPropertyVec iSchemaProps;
    iSchemaProps.reserve(iObjects.size());

    Abc::IBox3dProperty childBounds = iSchema0.getChildBoundsProperty();
    TimeSamplingPtr ctsPtr0;
    TimeSamplingType ctsType0;
    if (childBounds) {
        ctsPtr0 = childBounds.getTimeSampling();
        ctsType0 = ctsPtr0->getTimeSamplingType();
        string nameAndBounds = fullNodeName + " child bounds";
        checkAcyclic(ctsType0, nameAndBounds);
    }

    bool hasVisible = inObj.getProperties().getPropertyHeader("visible") != NULL;

    // sanity check (no frame range checking here)
    //      - timesamplying type has to be the same and can't be acyclic
    for (size_t i = 0; i < iObjects.size(); i++) {
        if (!iObjects[i].valid()) {
            continue;
        }

        IDataSchema iSchema = IData(iObjects[i]).getSchema();

        TimeSamplingPtr tsPtr = iSchema.getTimeSampling();
        TimeSamplingType tsType = tsPtr->getTimeSamplingType();
        checkAcyclic(tsType, fullNodeName);
        if (!(tsType0 == tsType)) {
            cerr << "Can not stitch different sampling type for node \"" << fullNodeName << "\"" << endl;
            // more details on this
            if (tsType.getNumSamplesPerCycle() != tsType0.getNumSamplesPerCycle()) {
                cerr << "\tnumSamplesPerCycle values are different" << endl;
            }
            if (tsType.getTimePerCycle() != tsType0.getTimePerCycle()) {
                cerr << "\ttimePerCycle values are different" << endl;
            }
            exit(1);
        }

        ICompoundProperty cp = iObjects[i].getProperties();
        iCompoundProps.push_back(cp);

        ICompoundProperty arbProp = iSchema.getArbGeomParams();
        if (arbProp)  // might be empty
            iArbGeomCompoundProps.push_back(arbProp);

        ICompoundProperty userProp = iSchema.getUserProperties();
        if (userProp)  // might be empty
            iUserCompoundProps.push_back(userProp);

        Abc::IBox3dProperty childBounds = iSchema.getChildBoundsProperty();
        TimeSamplingPtr ctsPtr;
        TimeSamplingType ctsType;
        if (childBounds) {
            ctsPtr = childBounds.getTimeSampling();
            ctsType = ctsPtr->getTimeSamplingType();
            iSchemaProps.push_back(iSchema);
        }

        if (!(ctsType0 == ctsType)) {
            cerr << "Can not stitch different sampling type for child bounds on\"" << fullNodeName << "\"" << endl;
            // more details on this
            if (ctsType.getNumSamplesPerCycle() != ctsType0.getNumSamplesPerCycle()) {
                cerr << "\tnumSamplesPerCycle values are different" << endl;
            }
            if (ctsType.getTimePerCycle() != ctsType0.getTimePerCycle()) {
                cerr << "\ttimePerCycle values are different" << endl;
            }
            if (!ctsPtr0 || !ctsPtr) {
                cerr << "\tchild bounds are missing on some archives" << endl;
            }
            exit(1);
        }
    }

    string inObjName = inObj.getName();
    string oParentName = oParentObj.getName();
    OData oData(oParentObj, inObjName, tsPtr0);
    oSchema = oData.getSchema();

    // stitch "visible" if it's points
    //
    if (hasVisible) {
        OCompoundProperty oCompoundProp = oData.getProperties();
        const PropertyHeader* propHeaderPtr = iCompoundProps[0].getPropertyHeader("visible");
        stitchScalarProp(*propHeaderPtr, iCompoundProps, oCompoundProp, iTimeMap);
    }

    // stitch ArbGeomParams and User Properties
    //
    if (iArbGeomCompoundProps.size() == iObjects.size()) {
        OCompoundProperty oArbGeomCompoundProp = oSchema.getArbGeomParams();
        stitchCompoundProp(iArbGeomCompoundProps, oArbGeomCompoundProp, iTimeMap);
    }

    if (iUserCompoundProps.size() == iObjects.size()) {
        OCompoundProperty oUserCompoundProp = oSchema.getUserProperties();
        stitchCompoundProp(iUserCompoundProps, oUserCompoundProp, iTimeMap);
    }

    if (!iSchemaProps.empty()) {
        stitchScalarProp(childBounds.getHeader(), iSchemaProps, oSchema, iTimeMap);
    }
}
OObject joinXform(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples) {
    OXformSchema oSchema;
    init<IXform, IXformSchema, OXform, OXformSchema>(iObjects, oParentObj, oSchema, iTimeMap, totalSamples);

    OObject outObj = oSchema.getObject();

    ICompoundPropertyVec iCompoundProps;
    iCompoundProps.reserve(iObjects.size());

    const PropertyHeader* locHeader = NULL;

    for (size_t i = 0; i < iObjects.size(); i++) {
        if (!iObjects[i].valid()) {
            iCompoundProps.push_back(ICompoundProperty());
            continue;
        }

        ICompoundProperty cp = iObjects[i].getProperties();
        iCompoundProps.push_back(cp);

        const PropertyHeader* childLocHeader = cp.getPropertyHeader("locator");
        if (!locHeader && childLocHeader) {
            locHeader = childLocHeader;
        }
    }

    // stitch the operations if this is an xform node
    size_t i = 0;
    for (i = 0; i < iObjects.size(); i++) {
        if (!iObjects[i].valid()) {
            continue;
        }

        IXformSchema iSchema = IXform(iObjects[i]).getSchema();
        index_t numSamples = iSchema.getNumSamples();
        index_t numEmpties = 0;
        index_t oNumSamples = oSchema.getNumSamples();
        TimeSamplingPtr oTime = oSchema.getTimeSampling();
        TimeSamplingPtr iTime = iSchema.getTimeSampling();
        index_t reqIdx = getIndexSample(oNumSamples, oTime, numSamples, iTime, numEmpties);

        // write empties only if we are also writing a sample, as the
        // first sample will be repeated over and over again
        for (index_t emptyIdx = 0; reqIdx < numSamples && emptyIdx < numEmpties; ++emptyIdx) {
            XformSample samp = iSchema.getValue(reqIdx);
            oSchema.set(samp);
        }

        for (; reqIdx < numSamples; reqIdx++) {
            XformSample samp = iSchema.getValue(reqIdx);
            oSchema.set(samp);
        }
    }

    // make sure we've set a sample, if we are going to extend them
    for (i = oSchema.getNumSamples(); i != 0 && i < totalSamples; ++i) {
        oSchema.setFromPrevious();
    }

    // stitch "locator" if it's a locator
    OCompoundProperty oCompoundProp = outObj.getProperties();
    if (locHeader) {
        stitchScalarProp(*locHeader, iCompoundProps, oCompoundProp, iTimeMap);
    }
    return outObj;
}
OObject joinSubD(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples) {
    OSubDSchema oSchema;
    init<ISubD, ISubDSchema, OSubD, OSubDSchema>(iObjects, oParentObj, oSchema, iTimeMap, totalSamples);
    OObject outObj = oSchema.getObject();

    OSubDSchema::Sample emptySample(P3fArraySample::emptySample(), Int32ArraySample::emptySample(), Int32ArraySample::emptySample());

    // stitch the SubDSchema
    //
    for (size_t i = 0; i < iObjects.size(); i++) {
        if (!iObjects[i].valid()) {
            continue;
        }

        ISubDSchema iSchema = ISubD(iObjects[i]).getSchema();
        index_t numSamples = iSchema.getNumSamples();
        IV2fGeomParam uvs = iSchema.getUVsParam();
        if (oSchema.getNumSamples() == 0 && uvs) {
            oSchema.setUVSourceName(GetSourceName(uvs.getMetaData()));
        }
        index_t numEmpties = 0;
        //index_t reqIdx = getIndexSample(oSchema.getNumSamples(),
        //    oSchema.getTimeSampling(), numSamples,
        //    iSchema.getTimeSampling(), numEmpties);
        index_t oNumSamples = oSchema.getNumSamples();
        TimeSamplingPtr oTime = oSchema.getTimeSampling();
        TimeSamplingPtr iTime = iSchema.getTimeSampling();
        index_t reqIdx = getIndexSample(oNumSamples, oTime, numSamples, iTime, numEmpties);

        for (index_t emptyIdx = 0; emptyIdx < numEmpties; ++emptyIdx) {
            oSchema.set(emptySample);
        }

        for (; reqIdx < numSamples; reqIdx++) {
            ISubDSchema::Sample iSamp = iSchema.getValue(reqIdx);
            OSubDSchema::Sample oSamp;

            Abc::P3fArraySamplePtr posPtr = iSamp.getPositions();
            if (posPtr)
                oSamp.setPositions(*posPtr);

            Abc::V3fArraySamplePtr velocPtr = iSamp.getVelocities();
            if (velocPtr)
                oSamp.setVelocities(*velocPtr);

            Abc::Int32ArraySamplePtr faceIndicesPtr = iSamp.getFaceIndices();
            if (faceIndicesPtr)
                oSamp.setFaceIndices(*faceIndicesPtr);

            Abc::Int32ArraySamplePtr faceCntPtr = iSamp.getFaceCounts();
            if (faceCntPtr)
                oSamp.setFaceCounts(*faceCntPtr);

            oSamp.setFaceVaryingInterpolateBoundary(iSamp.getFaceVaryingInterpolateBoundary());
            oSamp.setFaceVaryingPropagateCorners(iSamp.getFaceVaryingPropagateCorners());
            oSamp.setInterpolateBoundary(iSamp.getInterpolateBoundary());

            Abc::Int32ArraySamplePtr creaseIndicesPtr = iSamp.getCreaseIndices();
            if (creaseIndicesPtr)
                oSamp.setCreaseIndices(*creaseIndicesPtr);

            Abc::Int32ArraySamplePtr creaseLenPtr = iSamp.getCreaseLengths();
            if (creaseLenPtr)
                oSamp.setCreaseLengths(*creaseLenPtr);

            Abc::FloatArraySamplePtr creaseSpPtr = iSamp.getCreaseSharpnesses();
            if (creaseSpPtr)
                oSamp.setCreaseSharpnesses(*creaseSpPtr);

            Abc::Int32ArraySamplePtr cornerIndicesPtr = iSamp.getCornerIndices();
            if (cornerIndicesPtr)
                oSamp.setCornerIndices(*cornerIndicesPtr);

            Abc::FloatArraySamplePtr cornerSpPtr = iSamp.getCornerSharpnesses();
            if (cornerSpPtr)
                oSamp.setCornerSharpnesses(*cornerSpPtr);

            Abc::Int32ArraySamplePtr holePtr = iSamp.getHoles();
            if (holePtr)
                oSamp.setHoles(*holePtr);

            oSamp.setSubdivisionScheme(iSamp.getSubdivisionScheme());

            // set uvs
            IV2fGeomParam::Sample iUVSample;
            OV2fGeomParam::Sample oUVSample;
            if (uvs) {
                getOGeomParamSamp <IV2fGeomParam, IV2fGeomParam::Sample, OV2fGeomParam::Sample>
                (uvs, iUVSample, oUVSample, reqIdx);
                oSamp.setUVs(oUVSample);
            }

            oSchema.set(oSamp);
        }
    }

    for (size_t i = oSchema.getNumSamples(); i < totalSamples; ++i) {
        oSchema.set(emptySample);
    }
    return outObj;
}
OObject joinPolyMesh(OObject & oParentObj, vector<IObject>&iObjects, const TimeAndSamplesMap & iTimeMap, size_t totalSamples) {
    OPolyMeshSchema oSchema;
    init<IPolyMesh, IPolyMeshSchema, OPolyMesh, OPolyMeshSchema>(iObjects, oParentObj, oSchema, iTimeMap, totalSamples);
    OObject outObj = oSchema.getObject();

    OPolyMeshSchema::Sample emptySample(P3fArraySample::emptySample(),
                                        Int32ArraySample::emptySample(), Int32ArraySample::emptySample());

    // stitch the PolySchema
    //
    for (size_t i = 0; i < iObjects.size(); i++) {

        if (!iObjects[i].valid()) {
            continue;
        }

        IPolyMeshSchema iSchema = IPolyMesh(iObjects[i]).getSchema();
        index_t numSamples = iSchema.getNumSamples();

        IN3fGeomParam normals = iSchema.getNormalsParam();
        IV2fGeomParam uvs = iSchema.getUVsParam();
        if (oSchema.getNumSamples() == 0 && uvs) {
            oSchema.setUVSourceName(GetSourceName(uvs.getMetaData()));
        }

        index_t numEmpties = 0;
        //index_t reqIdx = getIndexSample(oSchema.getNumSamples(),
        //    oSchema.getTimeSampling(), numSamples,
        //    iSchema.getTimeSampling(), numEmpties);
        index_t oNumSamples = oSchema.getNumSamples();
        TimeSamplingPtr oTime = oSchema.getTimeSampling();
        TimeSamplingPtr iTime = iSchema.getTimeSampling();
        index_t reqIdx = getIndexSample(oNumSamples, oTime, numSamples, iTime, numEmpties);

        for (index_t emptyIdx = 0; emptyIdx < numEmpties; ++emptyIdx) {
            oSchema.set(emptySample);
        }
        //wxf
        printf("\tobject %lld, mesh numSamples=%lld\n", i, numSamples);

        for (; reqIdx < numSamples; reqIdx++) {
            IPolyMeshSchema::Sample iSamp = iSchema.getValue(reqIdx);
            OPolyMeshSchema::Sample oSamp;

            Abc::P3fArraySamplePtr posPtr = iSamp.getPositions();
            if (posPtr)
                oSamp.setPositions(*posPtr);

            Abc::V3fArraySamplePtr velocPtr = iSamp.getVelocities();
            if (velocPtr)
                oSamp.setVelocities(*velocPtr);

            Abc::Int32ArraySamplePtr faceIndicesPtr = iSamp.getFaceIndices();
            if (faceIndicesPtr)
                oSamp.setFaceIndices(*faceIndicesPtr);

            Abc::Int32ArraySamplePtr faceCntPtr = iSamp.getFaceCounts();
            if (faceCntPtr)
                oSamp.setFaceCounts(*faceCntPtr);

            // set uvs
            IV2fGeomParam::Sample iUVSample;
            OV2fGeomParam::Sample oUVSample;
            if (uvs) {
                getOGeomParamSamp <IV2fGeomParam, IV2fGeomParam::Sample, OV2fGeomParam::Sample>
                (uvs, iUVSample, oUVSample, reqIdx);
                oSamp.setUVs(oUVSample);
            }

            // set normals
            IN3fGeomParam::Sample iNormalsSample;
            ON3fGeomParam::Sample oNormalsSample;
            if (normals) {
                getOGeomParamSamp <IN3fGeomParam, IN3fGeomParam::Sample, ON3fGeomParam::Sample>
                (normals, iNormalsSample, oNormalsSample, reqIdx);
                oSamp.setNormals(oNormalsSample);
            }

            oSchema.set(oSamp);
        }
    }

    for (size_t i = oSchema.getNumSamples(); i < totalSamples; ++i) {
        oSchema.set(emptySample);
    }
    return outObj;
}
OObject joinCamera(OObject & oParentObj, vector<IObject>&iObjects, const TimeAndSamplesMap & iTimeMap, size_t totalSamples) {

    OCameraSchema oSchema;
    init<ICamera, ICameraSchema, OCamera, OCameraSchema>(iObjects, oParentObj, oSchema, iTimeMap, totalSamples);

    OObject outObj = oSchema.getObject();

    // stitch the CameraSchemas
    //
    for (size_t i = 0; i < iObjects.size(); i++) {
        if (!iObjects[i].valid()) {
            continue;
        }

        ICameraSchema iSchema = ICamera(iObjects[i]).getSchema();
        index_t numSamples = iSchema.getNumSamples();
        index_t numEmpties = 0;
        //index_t reqIdx = getIndexSample(oSchema.getNumSamples(),
        //    oSchema.getTimeSampling(), numSamples,
        //    iSchema.getTimeSampling(), numEmpties);
        index_t oNumSamples = oSchema.getNumSamples();
        TimeSamplingPtr oTime = oSchema.getTimeSampling();
        TimeSamplingPtr iTime = iSchema.getTimeSampling();
        index_t reqIdx = getIndexSample(oNumSamples, oTime, numSamples, iTime, numEmpties);

        // write empties only if we are also writing a sample, as the
        // first sample will be repeated over and over again
        for (index_t emptyIdx = 0;
                reqIdx < numSamples && emptyIdx < numEmpties; ++emptyIdx) {
            oSchema.set(iSchema.getValue(reqIdx));
        }

        for (; reqIdx < numSamples; reqIdx++) {
            oSchema.set(iSchema.getValue(reqIdx));
        }
    }

    // for the rest of the samples just set the last one as long as
    // a sample has been already set
    for (size_t i = oSchema.getNumSamples(); i != 0 && i < totalSamples; ++i) {
        oSchema.setFromPrevious();
    }
    return outObj;
}
OObject joinPoints(OObject & oParentObj, vector<IObject>&iObjects, const TimeAndSamplesMap & iTimeMap, size_t totalSamples) {

    OPointsSchema oSchema;
    init<IPoints, IPointsSchema, OPoints, OPointsSchema>(iObjects, oParentObj, oSchema, iTimeMap, totalSamples);
    OObject outObj = oSchema.getObject();
    OPointsSchema::Sample emptySample(P3fArraySample::emptySample(), UInt64ArraySample::emptySample());

    // stitch the PointsSchemas
    //
    for (size_t i = 0; i < iObjects.size(); i++) {
        if (!iObjects[i].valid()) {
            continue;
        }

        IPointsSchema iSchema = IPoints(iObjects[i]).getSchema();
        IFloatGeomParam iWidths = iSchema.getWidthsParam();
        index_t numSamples = iSchema.getNumSamples();
        index_t numEmpty = 0;
        index_t oNumSamples = oSchema.getNumSamples();
        TimeSamplingPtr oTime = oSchema.getTimeSampling();
        TimeSamplingPtr iTime = iSchema.getTimeSampling();
        index_t reqIdx = getIndexSample(oNumSamples, oTime, numSamples, iTime, numEmpty);

        for (index_t emptyIdx = 0; emptyIdx < numEmpty; ++emptyIdx) {
            oSchema.set(emptySample);
        }
        //wxf
        printf("\tobject %lld, point numSamples=%lld\n", i, numSamples);

        for (; reqIdx < numSamples; reqIdx++) {
            IPointsSchema::Sample iSamp = iSchema.getValue(reqIdx);
            OPointsSchema::Sample oSamp;
            Abc::P3fArraySamplePtr posPtr = iSamp.getPositions();
            if (posPtr)
                oSamp.setPositions(*posPtr);
            Abc::UInt64ArraySamplePtr idPtr = iSamp.getIds();
            if (idPtr)
                oSamp.setIds(*idPtr);
            Abc::V3fArraySamplePtr velocPtr = iSamp.getVelocities();
            if (velocPtr) {
                oSamp.setVelocities(*velocPtr);
                //wxf
                Dimensions dim = velocPtr->getDimensions();
                printf("\tdim=%lld\n", dim.rank());
                if (dim.rank() > 0)printf("dim[0]=%lld\n", dim[0]);
            }

            IFloatGeomParam::Sample iWidthSample;
            OFloatGeomParam::Sample oWidthSample;
            if (iWidths) {
                getOGeomParamSamp <IFloatGeomParam, IFloatGeomParam::Sample, OFloatGeomParam::Sample>
                (iWidths, iWidthSample, oWidthSample, reqIdx);
                oSamp.setWidths(oWidthSample);
            }

            oSchema.set(oSamp);
        }
    }

    for (size_t i = oSchema.getNumSamples(); i < totalSamples; ++i) {
        oSchema.set(emptySample);
    }
    return outObj;
}
OObject joinNuPatch(OObject & oParentObj, vector<IObject>&iObjects, const TimeAndSamplesMap & iTimeMap, size_t totalSamples) {
    ONuPatchSchema oSchema;
    init<INuPatch, INuPatchSchema, ONuPatch, ONuPatchSchema>(iObjects, oParentObj, oSchema, iTimeMap, totalSamples);
    OObject outObj = oSchema.getObject();

    Alembic::Util::int32_t zeroVal = 0;
    ONuPatchSchema::Sample emptySample(P3fArraySample::emptySample(),
                                       zeroVal, zeroVal, zeroVal, zeroVal, FloatArraySample::emptySample(), FloatArraySample::emptySample());

    // stitch the NuPatchSchemas
    //
    for (size_t i = 0; i < iObjects.size(); i++) {
        if (!iObjects[i].valid()) {
            continue;
        }

        INuPatchSchema iSchema = INuPatch(iObjects[i]).getSchema();
        index_t numSamples = iSchema.getNumSamples();
        IN3fGeomParam normals = iSchema.getNormalsParam();
        IV2fGeomParam uvs = iSchema.getUVsParam();
        index_t numEmpty = 0;
        index_t oNumSamples = oSchema.getNumSamples();
        TimeSamplingPtr oTime = oSchema.getTimeSampling();
        TimeSamplingPtr iTime = iSchema.getTimeSampling();

        index_t reqIdx = getIndexSample(oNumSamples, oTime, numSamples, iTime, numEmpty);
        for (index_t emptyIdx = 0; emptyIdx < numEmpty; ++emptyIdx) {
            oSchema.set(emptySample);
        }

        for (; reqIdx < numSamples; reqIdx++) {
            INuPatchSchema::Sample iSamp = iSchema.getValue(reqIdx);
            ONuPatchSchema::Sample oSamp;

            Abc::P3fArraySamplePtr posPtr = iSamp.getPositions();
            if (posPtr)
                oSamp.setPositions(*posPtr);

            Abc::V3fArraySamplePtr velocPtr = iSamp.getVelocities();
            if (velocPtr)
                oSamp.setVelocities(*velocPtr);

            oSamp.setNu(iSamp.getNumU());
            oSamp.setNv(iSamp.getNumV());
            oSamp.setUOrder(iSamp.getUOrder());
            oSamp.setVOrder(iSamp.getVOrder());

            Abc::FloatArraySamplePtr uKnotsPtr = iSamp.getUKnot();
            if (uKnotsPtr)
                oSamp.setUKnot(*uKnotsPtr);

            Abc::FloatArraySamplePtr vKnotsPtr = iSamp.getVKnot();
            if (vKnotsPtr)
                oSamp.setVKnot(*vKnotsPtr);

            IV2fGeomParam::Sample iUVSample;
            OV2fGeomParam::Sample oUVSample;
            if (uvs) {
                getOGeomParamSamp <IV2fGeomParam, IV2fGeomParam::Sample, OV2fGeomParam::Sample>
                (uvs, iUVSample, oUVSample, reqIdx);
                oSamp.setUVs(oUVSample);
            }

            IN3fGeomParam::Sample iNormalsSample;
            ON3fGeomParam::Sample oNormalsSample;
            if (normals) {
                getOGeomParamSamp <IN3fGeomParam, IN3fGeomParam::Sample, ON3fGeomParam::Sample>
                (normals, iNormalsSample, oNormalsSample, reqIdx);
                oSamp.setNormals(oNormalsSample);
            }


            if (iSchema.hasTrimCurve()) {
                oSamp.setTrimCurve(iSamp.getTrimNumLoops(),
                                   *(iSamp.getTrimNumCurves()),
                                   *(iSamp.getTrimNumVertices()),
                                   *(iSamp.getTrimOrders()),
                                   *(iSamp.getTrimKnots()),
                                   *(iSamp.getTrimMins()),
                                   *(iSamp.getTrimMaxes()),
                                   *(iSamp.getTrimU()),
                                   *(iSamp.getTrimV()),
                                   *(iSamp.getTrimW()));
            }
            oSchema.set(oSamp);
        }
    }

    for (size_t i = oSchema.getNumSamples(); i < totalSamples; ++i) {
        oSchema.set(emptySample);
    }
    return outObj;
}


OObject joinCurves(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples) {
    OCurvesSchema oSchema;
    init<ICurves, ICurvesSchema, OCurves, OCurvesSchema>(iObjects, oParentObj, oSchema, iTimeMap, totalSamples);
    OObject outObj = oSchema.getObject();
    OCurvesSchema::Sample emptySample(P3fArraySample::emptySample(), Int32ArraySample::emptySample());

    // stitch the CurvesSchemas
    //
    for (size_t i = 0; i < iObjects.size(); i++) {
        if (!iObjects[i].valid()) {
            continue;
        }
        ICurvesSchema iSchema = ICurves(iObjects[i]).getSchema();
        IV2fGeomParam iUVs = iSchema.getUVsParam();
        IN3fGeomParam iNormals = iSchema.getNormalsParam();
        IFloatGeomParam iWidths = iSchema.getWidthsParam();
        IFloatArrayProperty iKnots = iSchema.getKnotsProperty();    //control verts
        IUcharArrayProperty iOrders = iSchema.getOrdersProperty();

        index_t numSamples = iSchema.getNumSamples();
        index_t numEmpty = 0;
        index_t oNumSamples = oSchema.getNumSamples();
        TimeSamplingPtr oTime = oSchema.getTimeSampling();
        TimeSamplingPtr iTime = iSchema.getTimeSampling();
        index_t reqIdx = getIndexSample(oNumSamples, oTime, numSamples, iTime, numEmpty);
        for (index_t emptyIdx = 0; emptyIdx < numEmpty; ++emptyIdx) {
            oSchema.set(emptySample);
        }

        for (; reqIdx < numSamples; reqIdx++) {
            ICurvesSchema::Sample iSamp = iSchema.getValue(reqIdx);

            OCurvesSchema::Sample oSamp;
            Abc::P3fArraySamplePtr posPtr = iSamp.getPositions();
            if (posPtr)
                oSamp.setPositions(*posPtr);

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
            size_t nc = iKnots.getNumSamples();
            long uc = knotsPtr.use_count();
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
                (iWidths, iWidthSample, oWidthSample, reqIdx);
                oSamp.setWidths(oWidthSample);
            }

            IV2fGeomParam::Sample iUVSample;
            OV2fGeomParam::Sample oUVSample;
            if (iUVs) {
                getOGeomParamSamp <IV2fGeomParam, IV2fGeomParam::Sample, OV2fGeomParam::Sample>
                (iUVs, iUVSample, oUVSample, reqIdx);
                oSamp.setUVs(oUVSample);
            }

            IN3fGeomParam::Sample iNormalsSample;
            ON3fGeomParam::Sample oNormalsSample;
            if (iNormals) {
                getOGeomParamSamp <IN3fGeomParam, IN3fGeomParam::Sample, ON3fGeomParam::Sample>
                (iNormals, iNormalsSample, oNormalsSample, reqIdx);
                oSamp.setNormals(oNormalsSample);
            }

            oSchema.set(oSamp); // wxf, create a new frame
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
    } else if (ISubD::matches(header)) {
        outObj = joinSubD(oParentObj, iObjects, iTimeMap, totalSamples);
    } else if (IPolyMesh::matches(header)) {
        outObj = joinPolyMesh(oParentObj, iObjects, iTimeMap, totalSamples);
    } else if (ICamera::matches(header)) {
        outObj = joinCamera(oParentObj, iObjects, iTimeMap, totalSamples);
    } else if (ICurves::matches(header)) {
        outObj = joinCurves(oParentObj, iObjects, iTimeMap, totalSamples);
    } else if (IPoints::matches(header)) {
        outObj = joinPoints(oParentObj, iObjects, iTimeMap, totalSamples);
    } else if (INuPatch::matches(header)) {
        outObj = joinNuPatch(oParentObj, iObjects, iTimeMap, totalSamples);
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

//-*****************************************************************************
//-*****************************************************************************
// DO IT.
//-*****************************************************************************
//-*****************************************************************************
int main(int argc, char* argv[]) {
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
            timeMap.add(archive.getTimeSampling(0),
                        archive.getMaxNumSamplesForTimeSamplingIndex(0));

            if (numSamplings > 1) {
                // timesampling index 0 is special, so it will be skipped
                // use the first time on the next time sampling to determine
                // our archive order the archive order
                min = archive.getTimeSampling(1)->getSampleTime(0);

                for (uint32_t s = 1; s < numSamplings; ++s) {
                    timeMap.add(archive.getTimeSampling(s), archive.getMaxNumSamplesForTimeSamplingIndex(s));
                }
                printf("%s start frame %f\n", argv[i], min / 0.04);
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
