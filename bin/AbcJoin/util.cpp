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
using namespace Alembic::Abc;
using namespace Alembic::AbcGeom;
using namespace Alembic::AbcCoreAbstract;
using namespace Alembic::AbcCoreFactory;

void TimeAndSamplesMap::add(TimeSamplingPtr iTime, size_t iNumSamples) {
    if (iNumSamples == 0) {
        iNumSamples = 1;
    }
    //printf("iNumSamples=%d\n", iNumSamples);
    for (size_t i = 0; i < mTimeSampling.size(); ++i) {
        if (mTimeSampling[i]->getTimeSamplingType() == iTime->getTimeSamplingType()) {
            chrono_t curLastTime = mTimeSampling[i]->getSampleTime(mExpectedSamples[i]);

            chrono_t lastTime = iTime->getSampleTime(iNumSamples);
            if (lastTime < curLastTime) {
                lastTime = curLastTime;
            }

            if (mTimeSampling[i]->getSampleTime(0) > iTime->getSampleTime(0)) {
                mTimeSampling[i] = iTime;
            }

            mExpectedSamples[i] = mTimeSampling[i]->getNearIndex(lastTime, std::numeric_limits< index_t >::max()).first;
            //TimeSamplingType tst = iTime->getTimeSamplingType();
            //printf("lastTime=%f, mExpectedSamples[%lld]=%lld, %f\n", lastTime, i, mExpectedSamples[i], tst.getTimePerCycle());
            return;
        }
    }

    mTimeSampling.push_back(iTime);
    mExpectedSamples.push_back(iNumSamples);
}

TimeSamplingPtr TimeAndSamplesMap::get(TimeSamplingPtr iTime, std::size_t& oNumSamples) const {
    for (size_t i = 0; i < mTimeSampling.size(); ++i) {
        if (mTimeSampling[i]->getTimeSamplingType() == iTime->getTimeSamplingType()) {
            oNumSamples = mExpectedSamples[i];
            TimeSamplingType tst = iTime->getTimeSamplingType();
            //printf("TimeAndSamplesMap::get getNumStoredTimes%d, tst.getNumSamplesPerCycle=%d, getTimePerCycle=%f oNumSamples=%d\n", 
            //    iTime->getNumStoredTimes(), tst.getNumSamplesPerCycle(), tst.getTimePerCycle(), oNumSamples);
            //oNumSamples = 5;
            return mTimeSampling[i];
        }
    }

    oNumSamples = 0;
    return TimeSamplingPtr();
}

index_t getIndexSample(index_t iCurOutIndex, TimeSamplingPtr iOutTime, index_t iInNumSamples, TimeSamplingPtr iInTime, index_t& oNumEmpty) {
    // see if we are missing any samples for oNumEmpty
    chrono_t curTime = iOutTime->getSampleTime(iCurOutIndex);
    chrono_t inChrono = iInTime->getSampleTime(0);
    if (curTime < inChrono) {
        index_t emptyEnd = iOutTime->getNearIndex(inChrono, std::numeric_limits<index_t>::max()).first;
        if (emptyEnd > iCurOutIndex) {
            oNumEmpty = emptyEnd - iCurOutIndex;
        }
        else {
            oNumEmpty = 0;
        }
    }
    else {
        oNumEmpty = 0;
    }

    for (index_t i = 0; i < iInNumSamples; ++i) {
        inChrono = iInTime->getSampleTime(i);
        if (curTime <= inChrono || Imath::equalWithAbsError(curTime, inChrono, 1e-5)) {
            return i;
        }
    }

    return iInNumSamples;
}

void checkAcyclic(const TimeSamplingType& tsType, const std::string& fullNodeName) {
    if (tsType.isAcyclic()) {
        std::cerr << "No support for stitching acyclic sampling node " << fullNodeName << std::endl;
        exit(1);
    }
}


void stitchArrayProp(const PropertyHeader& propHeader, const ICompoundPropertyVec& iCompoundProps,
    OCompoundProperty& oCompoundProp, const TimeAndSamplesMap& iTimeMap) {

    size_t totalSamples = 0;
    TimeSamplingPtr timePtr = iTimeMap.get(propHeader.getTimeSampling(), totalSamples);

    const DataType& dataType = propHeader.getDataType();
    const MetaData& metaData = propHeader.getMetaData();
    const std::string& propName = propHeader.getName();

    Dimensions emptyDims(0);
    ArraySample emptySample(NULL, dataType, emptyDims);

    OArrayProperty writer(oCompoundProp, propName, dataType, metaData, timePtr);

    size_t numInputs = iCompoundProps.size();
    for (size_t iCpIndex = 0; iCpIndex < numInputs; iCpIndex++) {

        if (!iCompoundProps[iCpIndex].valid()) {
            continue;
        }

        const PropertyHeader* childHeader = iCompoundProps[iCpIndex].getPropertyHeader(propName);

        if (!childHeader || dataType != childHeader->getDataType()) {
            continue;
        }

        IArrayProperty reader(iCompoundProps[iCpIndex], propName);
        index_t numSamples = reader.getNumSamples();

        ArraySamplePtr dataPtr;
        index_t numEmpty;
        index_t k = getIndexSample(writer.getNumSamples(), writer.getTimeSampling(), numSamples, reader.getTimeSampling(), numEmpty);


        for (index_t j = 0; j < numEmpty; ++j) {
            writer.set(emptySample);
        }

        for (; k < numSamples; k++) {
            reader.get(dataPtr, k);
            writer.set(*dataPtr);
        }
    }

    // fill in any other empties
    for (size_t i = writer.getNumSamples(); i < totalSamples; ++i) {
        writer.set(emptySample);
    }
}

// return true if we needed to stitch the geom param
bool stitchArbGeomParam(const PropertyHeader& propHeader, const ICompoundPropertyVec& iCompoundProps,
    OCompoundProperty& oCompoundProp, const TimeAndSamplesMap& iTimeMap) {
    // go through all the inputs to see if all the property types are the same
    size_t numInputs = iCompoundProps.size();
    const std::string& propName = propHeader.getName();
    PropertyType ptype = propHeader.getPropertyType();
    bool diffProp = false;

    for (size_t iCpIndex = 1; iCpIndex < numInputs && diffProp == false; iCpIndex++) {
        if (!iCompoundProps[iCpIndex].valid()) {
            continue;
        }

        const PropertyHeader* childHeader = iCompoundProps[iCpIndex].getPropertyHeader(propName);

        if (childHeader && childHeader->getPropertyType() != ptype) {
            diffProp = true;
        }
    }

    // all of the props are the same, lets stitch them like normal
    if (!diffProp) {
        return false;
    }


    // we have a mismatch of indexed and non-index geom params, lets stitch them
    // together AS indexed
    std::vector< IArrayProperty > valsProp(numInputs);
    std::vector< IArrayProperty > indicesProp(numInputs);

    bool firstVals = true;

    DataType dataType;
    MetaData metaData;
    TimeSamplingPtr timePtr;

    // first we need to get our attrs
    for (size_t iCpIndex = 0; iCpIndex < numInputs; iCpIndex++) {
        if (!iCompoundProps[iCpIndex].valid()) {
            continue;
        }

        const PropertyHeader* childHeader = iCompoundProps[iCpIndex].getPropertyHeader(propName);

        if (childHeader && childHeader->isArray()) {
            valsProp[iCpIndex] = IArrayProperty(iCompoundProps[iCpIndex], propName);

            if (firstVals) {
                firstVals = false;
                dataType = valsProp[iCpIndex].getDataType();
                metaData = valsProp[iCpIndex].getMetaData();
                timePtr = valsProp[iCpIndex].getTimeSampling();
            }
        }
        else if (childHeader && childHeader->isCompound()) {
            ICompoundProperty cprop(iCompoundProps[iCpIndex], propName);
            if (cprop.getPropertyHeader(".vals") != NULL &&
                cprop.getPropertyHeader(".indices") != NULL) {
                valsProp[iCpIndex] = IArrayProperty(cprop, ".vals");
                indicesProp[iCpIndex] = IArrayProperty(cprop, ".indices");

                if (firstVals) {
                    firstVals = false;
                    dataType = valsProp[iCpIndex].getDataType();
                    metaData = valsProp[iCpIndex].getMetaData();
                    timePtr = valsProp[iCpIndex].getTimeSampling();
                }
            }
        }
    }


    size_t totalSamples = 0;
    timePtr = iTimeMap.get(timePtr, totalSamples);

    DataType indicesType(kUint32POD);
    Dimensions emptyDims(0);
    ArraySample emptySample(NULL, dataType, emptyDims);
    ArraySample emptyIndicesSample(NULL, indicesType, emptyDims);

    // we write indices and vals together
    OCompoundProperty ocProp(oCompoundProp, propName, metaData);
    OArrayProperty valsWriter(ocProp, ".vals", dataType, metaData, timePtr);
    OArrayProperty indicesWriter(ocProp, ".indices", indicesType, timePtr);

    for (size_t index = 0; index < numInputs; index++) {

        if (!valsProp[index].valid()) {
            continue;
        }

        index_t numSamples = valsProp[index].getNumSamples();

        ArraySamplePtr dataPtr;
        index_t numEmpty;
        index_t k = getIndexSample(valsWriter.getNumSamples(), valsWriter.getTimeSampling(), numSamples, valsProp[index].getTimeSampling(), numEmpty);

        for (index_t j = 0; j < numEmpty; ++j) {
            valsWriter.set(emptySample);
            indicesWriter.set(emptyIndicesSample);
        }

        for (; k < numSamples; k++) {
            valsProp[index].get(dataPtr, k);
            valsWriter.set(*dataPtr);

            if (indicesProp[index].valid()) {
                indicesProp[index].get(dataPtr, k);
                indicesWriter.set(*dataPtr);
            }
            else {
                // we need to construct our indices manually
                Dimensions dataDims = dataPtr->getDimensions();
                std::vector<uint32_t> indicesVec(
                    dataDims.numPoints());
                for (size_t dataIdx = 0; dataIdx < indicesVec.size(); ++dataIdx) {
                    indicesVec[dataIdx] = (uint32_t)dataIdx;
                }

                // set the empty sample
                if (indicesVec.empty()) {
                    indicesWriter.set(emptyIndicesSample);
                }
                else {
                    ArraySample indicesSamp(&indicesVec.front(), indicesType, dataDims);
                    indicesWriter.set(indicesSamp);
                }
            }
        }
    }

    // fill in any other empties
    for (size_t i = valsWriter.getNumSamples(); i < totalSamples; ++i) {
        valsWriter.set(emptySample);
        indicesWriter.set(emptyIndicesSample);
    }
    return true;
}

template< typename T >
void scalarPropIO(IScalarProperty& reader, uint8_t extent, OScalarProperty& writer) {
    std::vector< T > data(extent);
    std::vector< T > emptyData(extent);
    void* emptyPtr = static_cast<void*>(&emptyData.front());

    index_t numSamples = reader.getNumSamples();
    index_t numEmpty;
    index_t k = getIndexSample(writer.getNumSamples(), writer.getTimeSampling(), numSamples, reader.getTimeSampling(), numEmpty);

    // not really empty, but set to a default 0 or empty string value
    for (index_t i = 0; i < numEmpty; ++i) {
        writer.set(emptyPtr);
    }

    void* vPtr = static_cast<void*>(&data.front());

    for (; k < numSamples; ++k) {
        reader.get(vPtr, k);
        writer.set(vPtr);
    }
}

void stitchScalarProp(const PropertyHeader& propHeader,
    const ICompoundPropertyVec& iCompoundProps,
    OCompoundProperty& oCompoundProp,
    const TimeAndSamplesMap& iTimeMap) {
    size_t totalSamples = 0;
    TimeSamplingPtr timePtr = iTimeMap.get(propHeader.getTimeSampling(), totalSamples);

    const DataType& dataType = propHeader.getDataType();
    const MetaData& metaData = propHeader.getMetaData();
    const std::string& propName = propHeader.getName();
    PlainOldDataType pod = dataType.getPod();

    TimeSamplingType tst = timePtr->getTimeSamplingType();
    //printf("%s timePerCycle=%f\n", propName.c_str(), tst.getTimePerCycle());
    OScalarProperty writer(oCompoundProp, propName, dataType, metaData, timePtr);

    size_t numInputs = iCompoundProps.size();
    for (size_t iCpIndex = 0; iCpIndex < numInputs; iCpIndex++) {
        if (!iCompoundProps[iCpIndex].valid()) {
            continue;
        }

        const PropertyHeader* childHeader = iCompoundProps[iCpIndex].getPropertyHeader(propName);

        if (!childHeader || dataType != childHeader->getDataType()) {
            continue;
        }

        IScalarProperty reader(iCompoundProps[iCpIndex], propName);
        uint8_t extent = dataType.getExtent();
        switch (pod) {
        case kBooleanPOD:
            scalarPropIO< bool_t >(reader, extent, writer);
            break;
        case kUint8POD:
            scalarPropIO< uint8_t >(reader, extent, writer);
            break;
        case kInt8POD:
            scalarPropIO< int8_t >(reader, extent, writer);
            break;
        case kUint16POD:
            scalarPropIO< uint16_t >(reader, extent, writer);
            break;
        case kInt16POD:
            scalarPropIO< int16_t >(reader, extent, writer);
            break;
        case kUint32POD:
            scalarPropIO< uint32_t >(reader, extent, writer);
            break;
        case kInt32POD:
            scalarPropIO< int32_t >(reader, extent, writer);
            break;
        case kUint64POD:
            scalarPropIO< uint64_t >(reader, extent, writer);
            break;
        case kInt64POD:
            scalarPropIO< int64_t >(reader, extent, writer);
            break;
        case kFloat16POD:
            scalarPropIO< float16_t >(reader, extent, writer);
            break;
        case kFloat32POD:
            scalarPropIO< float32_t >(reader, extent, writer);
            break;
        case kFloat64POD:
            scalarPropIO< float64_t >(reader, extent, writer);
            break;
        case kStringPOD:
            scalarPropIO< string >(reader, extent, writer);
            break;
        case kWstringPOD:
            scalarPropIO< wstring >(reader, extent, writer);
            break;
        default:
            break;
        }
    }

    // set any extra empties
    std::vector< string > emptyStr(dataType.getExtent());
    std::vector< wstring > emptyWstr(dataType.getExtent());
    std::vector< uint8_t > emptyBuffer(dataType.getNumBytes());
    for (size_t i = writer.getNumSamples(); i < totalSamples; ++i) {
        if (pod == kStringPOD) {
            writer.set(&emptyStr.front());
        }
        else if (pod == kWstringPOD) {
            writer.set(&emptyWstr.front());
        }
        else {
            writer.set(&emptyBuffer.front());
        }
    }
}

void stitchCompoundProp(ICompoundPropertyVec& iCompoundProps, OCompoundProperty& oCompoundProp, const TimeAndSamplesMap& iTimeMap) {
    size_t numCompounds = iCompoundProps.size();
    for (size_t i = 0; i < numCompounds; ++i) {
        if (!iCompoundProps[i].valid()) {
            continue;
        }

        size_t numProps = iCompoundProps[i].getNumProperties();
        for (size_t propIndex = 0; propIndex < numProps; propIndex++) {
            const PropertyHeader& propHeader = iCompoundProps[i].getPropertyHeader(propIndex);
            string propName = propHeader.getName();
            if (oCompoundProp.getPropertyHeader(propName) != NULL) {
                continue;
            }

            if (propHeader.getMetaData().get("isGeomParam") == "true"){
                if (stitchArbGeomParam(propHeader, iCompoundProps, oCompoundProp, iTimeMap)) {
                    continue;
                }
            }
            else if (propHeader.isCompound()) {
                ICompoundPropertyVec childProps;
                for (size_t j = i; j < numCompounds; ++j) {
                    if (!iCompoundProps[j].valid() || iCompoundProps[j].getPropertyHeader(propHeader.getName()) == NULL) {
                        continue;
                    }

                    childProps.push_back(ICompoundProperty(iCompoundProps[j], propHeader.getName()));
                }
                OCompoundProperty child(oCompoundProp, propHeader.getName(), propHeader.getMetaData());
                stitchCompoundProp(childProps, child, iTimeMap);
            }
            else if (propHeader.isScalar()) {
                stitchScalarProp(propHeader, iCompoundProps, oCompoundProp, iTimeMap);
            }
            else if (propHeader.isArray()) {
                stitchArrayProp(propHeader, iCompoundProps, oCompoundProp, iTimeMap);
            }
        }
    }
}
//*******************************

OObject joinXform(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples) {
    OXformSchema oSchema = ceateDataSchema<IXform, IXformSchema, OXform, OXformSchema>(iObjects, oParentObj, iTimeMap, totalSamples);

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
#if 0
OObject joinSubD(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples) {
    OSubDSchema oSchema = ceateDataSchema<ISubD, ISubDSchema, OSubD, OSubDSchema>(iObjects, oParentObj, iTimeMap, totalSamples);
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
OObject joinPolyMesh(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples) {
    OPolyMeshSchema oSchema = ceateDataSchema<IPolyMesh, IPolyMeshSchema, OPolyMesh, OPolyMeshSchema>(iObjects, oParentObj, iTimeMap, totalSamples);
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
OObject joinCamera(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples) {

    OCameraSchema oSchema = ceateDataSchema<ICamera, ICameraSchema, OCamera, OCameraSchema>(iObjects, oParentObj, iTimeMap, totalSamples);

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
OObject joinPoints(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples) {

    OPointsSchema oSchema = ceateDataSchema<IPoints, IPointsSchema, OPoints, OPointsSchema>(iObjects, oParentObj, iTimeMap, totalSamples);
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
OObject joinNuPatch(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples) {
    ONuPatchSchema oSchema = ceateDataSchema<INuPatch, INuPatchSchema, ONuPatch, ONuPatchSchema>(iObjects, oParentObj, iTimeMap, totalSamples);
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
#endif