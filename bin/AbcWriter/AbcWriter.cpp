//-*****************************************************************************
//
// Copyright(c) 2023-2024, PASSION PAINT ANIMATION
//
//-*****************************************************************************
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


//-*****************************************************************************
//-*****************************************************************************
std::map<int, OCurvesSchema*> oSchemas;
std::vector<OCurves*> oCurvess;
OXform *xform = NULL;
TimeSamplingPtr tsp;
int writeFrame(int no, std::vector<Alembic::Abc::V3f> points) {
    // 创建曲线对象
    OCurvesSchema* oSchema = NULL;
    if(oSchemas.count(no) > 0)
        oSchema = oSchemas[no];
    if (!oSchema) {
        char name[128];
        sprintf(name, "curve%d", no);
        OCurves* curves = new OCurves(*xform, name, tsp);
        oSchema = &curves->getSchema();        
        oSchemas[no] = oSchema;
        oCurvess.push_back(curves);
    }
    // 添加曲线数据
    OCurvesSchema::Sample oSamp;
    int32_t numCurves = 1;
    std::vector<Alembic::Util::int32_t> nVertices(numCurves);
    nVertices[0] = 100;
    oSamp.setCurvesNumVertices(Alembic::Abc::Int32ArraySample(nVertices));
    oSamp.setBasis(Alembic::AbcGeom::kBsplineBasis);
    std::vector<Alembic::Util::uint8_t> orders(numCurves);
    int degree = 2; // xgen always is 2
    orders[0] = degree + 1; //degree + 1;
    oSamp.setOrders(Alembic::Abc::UcharArraySample(orders));
    oSamp.setType(kVariableOrder);
    int numPoints = points.size();
    oSamp.setPositions(Alembic::Abc::P3fArraySample(reinterpret_cast<const Imath::V3f*>(points.data()), numPoints));
    oSamp.setSelfBounds(Alembic::Abc::Box3d());
    //oSamp.setInvisible(Alembic::Abc::UIntArraySample());
    //oSamp.setTopologyVisibility(Alembic::Abc::UIntArraySample());

    //Alembic::AbcGeom::SampleSelector sampleSelector(static_cast<float>(startTime));
    oSchema->set(oSamp); // , sampleSelector);
    return 0;
}

int abcCreate(){
    string fileName = "out.abc"; // argv[1];


    IFactory::CoreType coreType = IFactory::kOgawa;
    string appWriter = "AbcWriter";
    string userStr = "AbcWriter exported from out.abc" ;
    MetaData md;
    double fps(0.04);

    OArchive oArchive;
    oArchive = CreateArchiveWithInfo(Alembic::AbcCoreOgawa::WriteArchive(), fileName, appWriter, userStr, md, ErrorHandler::kThrowPolicy);

    OObject oRoot = oArchive.getTop();
    if (!oRoot.valid()) {
        return -1;
    }
    double startTime = 1.0;
    double endTime = 100.0;


    try {
        //std::vector < chrono_t > samples;
        //samples.push_back( 0.04);
        //TimeSampling iTs(AbcA::TimeSamplingType(AbcA::TimeSamplingType::kAcyclic), samples);
        TimeSampling iTs(0.04, 1.68);
        TimeSamplingType tst =  iTs.getTimeSamplingType();
        chrono_t cycle = tst.getTimePerCycle();
        printf("frame cycle=%f, fps=%.1f\n", cycle, fps);

        oArchive.addTimeSampling(iTs);

        tsp = TimeSamplingPtr(new TimeSampling(iTs));
        xform = new OXform(oRoot, "xform_node",tsp);
    }
    catch (const std::exception& e) {
        // 捕获异常并输出错误信息
        std::cerr << "Error creating curves object: " << e.what() << std::endl;
    }

    return 0;
}
void test() {
    abcCreate();
    int numPoints = 100;
    std::vector<Alembic::Abc::V3f> points;
    points.reserve(numPoints);

    for (int i = 0; i < numPoints; ++i) {
        double x = (double)i / (numPoints);
        points.push_back(Alembic::Abc::V3f(x, 0.0f, 0.0f));
    }
    writeFrame(1, points);
    for (int i = 0; i < numPoints; ++i) {
        points[i][1] += 0.1;
    }
    writeFrame(1, points);
    for (int i = 0; i < numPoints; ++i) {
        points[i][1] += 0.1;
    }
    writeFrame(2, points);

    for (int i = 0; i < oCurvess.size(); i++)
        if (oCurvess[i])
            delete oCurvess[i];
    if (xform)
        delete xform;

}

//-*****************************************************************************
// main
//-*****************************************************************************
int main(int argc, char* argv[]) {
    if (argc < 0) {
        cerr << "Usage: abcwriter outFile.abc \n\n"
            "Options:\n"
            "  -v      Verbose for the IObject.\n";
        return -1;
    }
    test();

    printf("done\n");
}
