//-*****************************************************************************
//
// Copyright (c) 2009-2013,
//  Sony Pictures Imageworks, Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreAbstract/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/Util/All.h>
#include <Alembic/Abc/TypedPropertyTraits.h>

#include <iostream>
#include <sstream>
#include <corecrt_io.h>
#include <excpt.h>
//-*****************************************************************************
using namespace ::Alembic::AbcGeom;

static const std::string g_sep( ";" );

//-*****************************************************************************
// FORWARD
void visitProperties( ICompoundProperty, std::string & );
uint8_t* archiveAddr = NULL;
//-*****************************************************************************
template <class PROP>
void visitSimpleArrayProperty( PROP iProp, const std::string &iIndent )
{
    std::string ptype = "ArrayProperty ";
    size_t asize = 0;

    AbcA::ArraySamplePtr samp;
    index_t maxSamples = iProp.getNumSamples();
    //printf("wxf:%s maxSamples=%d\n", __func__, maxSamples);
    for ( index_t i = 0 ; i < maxSamples; ++i )
    {
        ISampleSelector ss = ISampleSelector(i);
        iProp.get( samp, ss );
        asize = samp->size();
        //if (iProp.getName() == "P") {
        //    const void* p = samp->getData();
        //    float* pf = (float*)(p);
        //    printf("cv data: frame %lld ", i);
        //    for (int j = 0; j < 6; j += 3) {
        //        printf("[%.2f, %.2f, %.2f],", pf[j], pf[j + 1], pf[j + 2]);
        //    }
        //    printf("\n");
        //}
    };

    std::string mdstring = "interpretation=";
    mdstring += iProp.getMetaData().get( "interpretation" );

    std::stringstream dtype;
    dtype << "datatype=";
    dtype << iProp.getDataType();

    std::stringstream asizestr;
    asizestr << ";arraysize=";
    asizestr << asize;

    mdstring += g_sep;

    mdstring += dtype.str();

    mdstring += asizestr.str();

    std::cout << iIndent << "  " << ptype << "name=" << iProp.getName()
              << g_sep << mdstring << g_sep << "numsamps="
              << iProp.getNumSamples() << std::endl;

    if (iProp.getName() == "P") {
        for (index_t i = 0; i < maxSamples; ++i){
            ISampleSelector ss = ISampleSelector(i);
            index_t fi = ss.getIndex(iProp.getTimeSampling(), iProp.getNumSamples());

            uint64_t oPos = 0, oSize = 0;
            iProp.getPtr()->getSamplePos(i, oPos, oSize);
            float* pf = (float*)(archiveAddr + oPos);
            printf("data: frame %lld ", fi);
            for (int j = 0; j < 6; j+=3) {
                printf("[%.2f, %.2f, %.2f],", pf[j], pf[j + 1], pf[j + 2]);
            }
            printf("\n");


            //iProp.get(samp, ss);
            //const void* p = samp->getData();
            //pf = (float*)(p);
            //printf("samp: frame %lld ", fi);
            //for (int j = 0; j < 6; j += 3) {
            //    printf("[%.2f, %.2f, %.2f],", pf[j], pf[j + 1], pf[j + 2]);
            //}
            //printf("\n");
        }
    };
}

//-*****************************************************************************
template <class PROP>
void visitSimpleScalarProperty( PROP iProp, const std::string &iIndent )
{
    std::string ptype = "ScalarProperty ";
    size_t asize = 0;

    const AbcA::DataType &dt = iProp.getDataType();
    const Alembic::Util ::uint8_t extent = dt.getExtent();
    Alembic::Util::Dimensions dims( extent );
    AbcA::ArraySamplePtr samp =
        AbcA::AllocateArraySample( dt, dims );
    index_t maxSamples = iProp.getNumSamples();
    //printf("wxf:%s maxSamples=%d\n", __func__, maxSamples);
    for ( index_t i = 0 ; i < maxSamples; ++i )
    {
        iProp.get( const_cast<void*>( samp->getData() ),
                                      ISampleSelector( i ) );
        asize = samp->size();
    };

    std::string mdstring = "interpretation=";
    mdstring += iProp.getMetaData().get( "interpretation" );

    std::stringstream dtype;
    dtype << "datatype=";
    dtype << dt;

    std::stringstream asizestr;
    asizestr << ";arraysize=";
    asizestr << asize;

    mdstring += g_sep;

    mdstring += dtype.str();

    mdstring += asizestr.str();

    std::cout << iIndent << "  " << ptype << "name=" << iProp.getName()
              << g_sep << mdstring << g_sep << "numsamps="
              << iProp.getNumSamples() << std::endl;
}

//-*****************************************************************************
void visitCompoundProperty( ICompoundProperty iProp, std::string &ioIndent )
{
    std::string oldIndent = ioIndent;
    ioIndent += "  ";

    std::string interp = "schema=";
    interp += iProp.getMetaData().get( "schema" );

    std::cout << ioIndent << "CompoundProperty " << "name=" << iProp.getName()
              << g_sep << interp << std::endl;

    visitProperties( iProp, ioIndent );

    ioIndent = oldIndent;
}

//-*****************************************************************************
void visitProperties( ICompoundProperty iParent,
                      std::string &ioIndent )
{
    std::string oldIndent = ioIndent;
    size_t numProp = iParent.getNumProperties();
    for ( size_t i = 0 ; i < numProp ; i++ )
    {
        PropertyHeader header = iParent.getPropertyHeader( i );
        const TimeSamplingPtr ts = header.getTimeSampling();
        if (ts) {
            TimeSamplingType tst = ts->getTimeSamplingType(); 
            printf("\n%s: num samples per cycle: %d, time per cycle: %f, tsp=%p\n", header.getName().c_str(),
                tst.getNumSamplesPerCycle(), tst.getTimePerCycle(),ts.get());
        }
        if ( header.isCompound() )
        {
            visitCompoundProperty( ICompoundProperty( iParent,
                                                      header.getName() ),
                                   ioIndent );
        }
        else if ( header.isScalar() )
        {
            visitSimpleScalarProperty( IScalarProperty( iParent,
                                                        header.getName() ),
                                 ioIndent );
        }
        else
        {
            assert( header.isArray() );
            visitSimpleArrayProperty( IArrayProperty( iParent,
                                                      header.getName() ),
                                 ioIndent );
        }
    }

    ioIndent = oldIndent;
}

//-*****************************************************************************
void visitObject( IObject iObj,
                  std::string iIndent )
{
    // Object has a name, a full name, some meta data,
    // and then it has a compound property full of properties.
    std::string path = iObj.getFullName();

    if ( path != "/" )
    {
        std::cout << "Object " << "name=" << path << std::endl;
    }

    // Get the properties.
    ICompoundProperty props = iObj.getProperties();
    visitProperties( props, iIndent );

    // now the child objects
    for ( size_t i = 0 ; i < iObj.getNumChildren() ; i++ )
    {
        visitObject( IObject( iObj, iObj.getChildHeader( i ).getName() ),
                     iIndent );
    }
}

//-*****************************************************************************
//-*****************************************************************************
// DO IT.
//-*****************************************************************************
//-*****************************************************************************
int main( int argc, char *argv[] )
{
    if ( argc != 2 )
    {
        std::cerr << "USAGE: " << argv[0] << " <AlembicArchive.abc>"
                  << std::endl;
        exit( -1 );
    }
    std::cout << "AbcEcho for " << Alembic::AbcCoreAbstract::GetLibraryVersion() << std::endl << std::endl;;
    if (_access(argv[1], 0) != 0) {
        std::cerr << "Not found " << argv[1] << std::endl;
        exit( -1 );
    }
    __try {
        Alembic::AbcCoreFactory::IFactory factory;
        factory.setPolicy(ErrorHandler::kQuietNoopPolicy);
        IArchive archive = factory.getArchive( argv[1] );

        if (archive)
        {
            if (!archive.valid()) {
                std::cerr << argv[1] << " is invalid ABC file.\n";
                exit( -1 );
            }

            std::string appName;
            std::string libraryVersionString;
            Alembic::Util::uint32_t libraryVersion;
            std::string whenWritten;
            std::string userDescription;
            GetArchiveInfo (archive, appName, libraryVersionString, libraryVersion, whenWritten, userDescription);

            if (appName != "")
            {
                std::cout << "  file written by: " << appName << std::endl;
                std::cout << "  using Alembic : " << libraryVersionString << std::endl;
                std::cout << "  written on : " << whenWritten << std::endl;
                std::cout << "  user description : " << userDescription << std::endl;
                std::cout << std::endl;

                archiveAddr = (uint8_t*)archive.getPtr()->getMemoryMapPtr();
                uint32_t NumTimeSamplings = archive.getNumTimeSamplings();
                printf("Time sampling: NumTimeSamplings=%u\n", NumTimeSamplings);
                const int TimeSamplingIndex = NumTimeSamplings > 1 ? 1 : 0;

                AbcA::TimeSamplingPtr tsp = archive.getTimeSampling(TimeSamplingIndex);
                printf("TimeSamplingPtr[%d]: %p\n", TimeSamplingIndex, tsp);
                printf("Time sampling: NumStoredTimes=%llu\n", tsp->getNumStoredTimes());
                printf("Time sampling: SampleTime=%f\n", tsp->getSampleTime(0));
                const std::vector < chrono_t >& st = tsp->getStoredTimes();
                printf("Stored times: getStoredTimes=%lld, startFrame=%f\n", st.size(), st[0]);
                TimeSamplingType tst = tsp->getTimeSamplingType();
                chrono_t cycle = tst.getTimePerCycle();
                chrono_t fps = 1 / cycle;
                float startFrame = st[0] / cycle;
                index_t maxSample = archive.getMaxNumSamplesForTimeSamplingIndex(TimeSamplingIndex);
                printf("frame cycle=%f, fps=%.1f\n", cycle, fps);
                printf("frame start num=%f, frame count=%lld\n\n", startFrame, maxSample);
                if (TimeSamplingIndex == 1) {
                    AbcA::TimeSamplingPtr tsp = archive.getTimeSampling(0);
                    printf("TimeSamplingPtr[0]: %p\n", tsp);
                    TimeSamplingType tst = tsp->getTimeSamplingType();
                    chrono_t cycle = tst.getTimePerCycle();
                    printf("TimePerCycle=%f\n", cycle);
                }
            }
            else
            {
                std::cout << argv[1] << std::endl;
                std::cout << "  (file doesn't have any ArchiveInfo)"
                          << std::endl;
                std::cout << std::endl;
            }
        }
        visitObject( archive.getTop(), "" );
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        std::cerr << "Fatal exception!! " << argv[1] << " is invalid ABC file.\n";
    }
    return 0;
}
