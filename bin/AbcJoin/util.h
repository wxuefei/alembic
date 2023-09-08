//-*****************************************************************************
//
// Copyright(c) 2023-2024, PASSION PAINT ANIMATION
//
//-*****************************************************************************

#ifndef ABC_STITCHER_UTIL_H
#define ABC_STITCHER_UTIL_H

#include <string>
#include <vector>
#include <Alembic/Abc/ICompoundProperty.h>
#include <Alembic/Abc/OCompoundProperty.h>

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


#endif // ABC_STITCHER_UTIL_H
