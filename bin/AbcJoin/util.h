//-*****************************************************************************
//
// Copyright(c) 2023-2024, PASSION PAINT ANIMATION
//
//-*****************************************************************************

#ifndef ABC_STITCHER_UTIL_H
#define ABC_STITCHER_UTIL_H

#include <string>
#include <vector>
#include <Alembic/AbcGeom/All.h>
#ifdef ALEMBIC_WITH_HDF5
#include <Alembic/AbcCoreHDF5/All.h>
#endif
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/Abc/ICompoundProperty.h>
#include <Alembic/Abc/OCompoundProperty.h>

using namespace std;
using namespace Alembic::Abc;
using namespace Alembic::AbcGeom;
using namespace Alembic::AbcCoreAbstract;
using namespace Alembic::AbcCoreFactory;

typedef std::vector< Alembic::Abc::ICompoundProperty > ICompoundPropertyVec;

class TimeAndSamplesMap
{
public:
    TimeAndSamplesMap() {m_isVerbose = false;};

    void add(Alembic::AbcCoreAbstract::TimeSamplingPtr iTime,
             std::size_t iNumSamples);

    Alembic::AbcCoreAbstract::TimeSamplingPtr get(
        Alembic::AbcCoreAbstract::TimeSamplingPtr iTime,
        std::size_t & oNumSamples) const;

    void setVerbose(bool isVerbose){m_isVerbose = isVerbose;};
    bool isVerbose() const {return m_isVerbose;};

private:
    std::vector< Alembic::AbcCoreAbstract::TimeSamplingPtr > mTimeSampling;
    std::vector< std::size_t > mExpectedSamples;
    bool m_isVerbose;
};

Alembic::AbcCoreAbstract::index_t
getIndexSample(Alembic::AbcCoreAbstract::index_t iCurOutIndex,
    Alembic::AbcCoreAbstract::TimeSamplingPtr iOutTime,
    Alembic::AbcCoreAbstract::index_t iInNumSamples,
    Alembic::AbcCoreAbstract::TimeSamplingPtr iInTime,
    Alembic::AbcCoreAbstract::index_t & oNumEmpty);

void checkAcyclic(const Alembic::AbcCoreAbstract::TimeSamplingType & tsType,
                  const std::string & fullNodeName);

void stitchArrayProp(const Alembic::AbcCoreAbstract::PropertyHeader & propHeader,
                     const ICompoundPropertyVec & iCompoundProps,
                     Alembic::Abc::OCompoundProperty & oCompoundProp,
                     const TimeAndSamplesMap & iTimeMap);

void stitchScalarProp(const Alembic::AbcCoreAbstract::PropertyHeader & propHeader,
                      const ICompoundPropertyVec & iCompoundProps,
                      Alembic::Abc::OCompoundProperty & oCompoundProp,
                      const TimeAndSamplesMap & iTimeMap);

void stitchCompoundProp(ICompoundPropertyVec & iCompoundProps,
                        Alembic::Abc::OCompoundProperty & oCompoundProp,
                        const TimeAndSamplesMap & iTimeMap);


OObject joinCamera(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples);
OObject joinNuPatch(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples);
OObject joinPoints(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples);
OObject joinPolyMesh(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples);
OObject joinSubD(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples);
OObject joinXform(OObject& oParentObj, vector<IObject>& iObjects, const TimeAndSamplesMap& iTimeMap, size_t totalSamples);

template< class IPARAM, class IPARAMSAMP, class OPARAMSAMP >
void getOGeomParamSamp(IPARAM& iGeomParam, IPARAMSAMP& iGeomSamp, OPARAMSAMP& oGeomSamp, index_t iIndex) {
    if (iGeomParam.isIndexed()) {
        iGeomParam.getIndexed(iGeomSamp, iIndex);
        oGeomSamp.setVals(*(iGeomSamp.getVals()));
        oGeomSamp.setScope(iGeomSamp.getScope());
        oGeomSamp.setIndices(*(iGeomSamp.getIndices()));
    }
    else {
        iGeomParam.getExpanded(iGeomSamp, iIndex);
        oGeomSamp.setVals(*(iGeomSamp.getVals()));
        oGeomSamp.setScope(iGeomSamp.getScope());
    }
}
TimeSamplingPtr getFrame1Tsp();
static TimeSampling* makeTimeSampling() {
    TimeSampling* ts = new TimeSampling(0.04, 1.68 + 1 * 0.04);
    return ts;
}
template<class IData, class IDataSchema, class OData, class ODataSchema>
ODataSchema ceateDataSchema(vector< IObject >& iObjects, OObject& oParentObj,
    const TimeAndSamplesMap& iTimeMap, size_t& oTotalSamples) {
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
    //TimeSamplingPtr tsp = iSchema0.getTimeSampling();
    //TimeSamplingType tst = tsp->getTimeSamplingType();
//    printf("%s oTotalSamples=%d, TimePerCycle=%f\n", fullNodeName.c_str(), oTotalSamples, tst.getTimePerCycle());
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
        TimeSamplingType tsType2 = tsPtr->getTimeSamplingType();
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

    //tsPtr0 = TimeSamplingPtr(makeTimeSampling());
    //oTotalSamples = iObjects.size();//wxf, just for only one frame in one abc file.
    tsPtr0 = getFrame1Tsp();

    OData oData(oParentObj, inObjName, tsPtr0);
    ODataSchema oSchema = oData.getSchema();

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
    return oSchema;
}

#endif // ABC_STITCHER_UTIL_H
