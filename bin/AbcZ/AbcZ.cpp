//-*****************************************************************************
// Copyright (c) 2023-2023
//-*****************************************************************************

#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreAbstract/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/Util/All.h>
#include <Alembic/Abc/TypedPropertyTraits.h>

#include <iostream>
#include <sstream>

//-*****************************************************************************
using namespace ::Alembic::AbcGeom;

static const std::string g_sep( ";" );

//-*****************************************************************************
void visitProperties( ICompoundProperty, std::string & );

//-*****************************************************************************
void visitSimpleArrayProperty(IArrayProperty iProp, const std::string &iIndent ){
    std::string ptype = "ArrayProperty ";
    size_t asize = 0;

    AbcA::ArraySamplePtr samp;
    index_t maxSamples = iProp.getNumSamples();
    for ( index_t i = 0 ; i < maxSamples; ++i ){
        iProp.get( samp, ISampleSelector( i ) );
        asize = samp->size();
    }

    std::stringstream dtype;
    dtype << "datatype=";
    dtype << iProp.getDataType();

    std::stringstream asizestr;
    asizestr << ";arraysize=";
    asizestr << asize;

    std::string mdstring = "interpretation=";
    mdstring += iProp.getMetaData().get( "interpretation" );
    mdstring += g_sep;
    mdstring += dtype.str();
    mdstring += asizestr.str();

    std::cout << iIndent << "  " << ptype << "name=" << iProp.getName()
              << g_sep << mdstring << g_sep << "numsamps="
              << iProp.getNumSamples() << std::endl;
}

//-*****************************************************************************
void visitSimpleScalarProperty(IScalarProperty iProp, const std::string &iIndent )
{
    std::string ptype = "ScalarProperty ";
    size_t asize = 0;

    const AbcA::DataType &dt = iProp.getDataType();
    const Alembic::Util ::uint8_t extent = dt.getExtent();
    Alembic::Util::Dimensions dims( extent );
    AbcA::ArraySamplePtr samp = AbcA::AllocateArraySample( dt, dims );
    index_t maxSamples = iProp.getNumSamples();
    for ( index_t i = 0 ; i < maxSamples; ++i ){
        iProp.get( const_cast<void*>( samp->getData() ), ISampleSelector( i ) );
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
void visitCompoundProperty( ICompoundProperty iProp, std::string &ioIndent ){
    std::string ptype = "CompoundProperty ";
    std::string oldIndent = ioIndent;
    ioIndent += "  ";

    std::string interp = "schema=";
    interp += iProp.getMetaData().get( "schema" );

    std::cout << ioIndent << ptype << "name=" << iProp.getName()
              << g_sep << interp << std::endl;

    visitProperties( iProp, ioIndent );

    ioIndent = oldIndent;
}

//-*****************************************************************************
void visitProperties( ICompoundProperty iParent, std::string &ioIndent )
{
    std::string oldIndent = ioIndent;
    for ( size_t i = 0 ; i < iParent.getNumProperties() ; i++ )
    {
        PropertyHeader header = iParent.getPropertyHeader( i );
        std::string name = header.getName();
        if ( header.isCompound()){
            visitCompoundProperty( ICompoundProperty( iParent, name), ioIndent );
        }
        else if ( header.isScalar()){
            visitSimpleScalarProperty( IScalarProperty( iParent, name), ioIndent );
        }
        else if (header.isArray()) {
            //assert( header.isArray() );
            visitSimpleArrayProperty( IArrayProperty( iParent, name), ioIndent );
        }
        else {
            //wxf
        }
        fflush(stdout);
    }

    ioIndent = oldIndent;
}

//-*****************************************************************************
void visitObject( IObject iObj, std::string iIndent )
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
        IObject child(iObj, iObj.getChildHeader(i).getName());
        visitObject( child, iIndent );
    }
}

//-*****************************************************************************
int abcz(int argc, char* argv[]) {
    if (argc != 2) {
        char* exe = argv[0];
        char*p = strrchr(exe, '\\');
        if (p)exe = p + 1;
        std::cerr << "USAGE: " << exe << " <AlembicArchive.abc>" << std::endl;
        exit(-1);
    }
    char* filename = argv[1];
    //filename = "e:\\Projects\\maya\\abc\\chr_DaJiaZhang.chr_DaJiaZhang_rig.render_mesh.abc";
    filename = "a1_nonormals.abc";
    //filename = "1plane.abc";
//    filename = "1plane_tri.abc";
    filename = "a1_1.abc";
    filename = "chr_DaJiaZhang2.abc";
    // Scoped.

    Alembic::AbcCoreFactory::IFactory factory;
    factory.setPolicy(ErrorHandler::kQuietNoopPolicy);
    IArchive archive = factory.getArchive(filename);

    std::cout << "AbcZ for " << Alembic::AbcCoreAbstract::GetLibraryVersion() << std::endl;;
    if (archive){
        std::string appName;
        std::string libraryVersionString;
        Alembic::Util::uint32_t libraryVersion;
        std::string whenWritten;
        std::string userDescription;
        GetArchiveInfo(archive,
            appName,
            libraryVersionString,
            libraryVersion,
            whenWritten,
            userDescription);

        if (appName != "")
        {
            std::cout << "  file written by: " << appName << std::endl;
            std::cout << "  using Alembic : " << libraryVersionString << std::endl;
            std::cout << "  written on : " << whenWritten << std::endl;
            std::cout << "  user description : " << userDescription << std::endl;
            std::cout << std::endl;
        }
        else
        {
            std::cout << filename << std::endl;
            std::cout << "  (file doesn't have any ArchiveInfo)"
                << std::endl;
            std::cout << std::endl;
        }
        visitObject(archive.getTop(), "");
    }
    exit(0);
}
//-*****************************************************************************
//-*****************************************************************************
// DO IT.
//-*****************************************************************************
//-*****************************************************************************
void printDumpStat(char* p);

int main( int argc, char *argv[] ){
    //half f1;
    //f1.setBits(11);
    //printf("f1=%f\n", (float)f1);
    //return 0;
    //printDumpStat(NULL);
    abcz(argc, argv);

    return 0;
}
