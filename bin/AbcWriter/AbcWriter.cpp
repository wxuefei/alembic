//-*****************************************************************************
//
// Copyright(c) 2023-2024, PASSION PAINT ANIMATION
//
//-*****************************************************************************

#include <iostream>
#include <sstream>
#include <algorithm>
//#include "util.h"
//#include "MayaUtility.h"

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
// DO IT.
//-*****************************************************************************
//-*****************************************************************************
int main(int argc, char* argv[]) {
    if (argc < 0) {
        cerr << "Usage: abcwriter outFile.abc \n\n"
                "Options:\n"
                "  -v      Verbose for the IObject.\n" ;
        return -1;
    }

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

    int numPoints = 100;
    std::vector<Alembic::Abc::V3f> points;
    points.reserve(numPoints);

    for (int i = 0; i < numPoints; ++i)
    {
        double t = static_cast<double>(i) / (numPoints - 1);
        points.push_back(Alembic::Abc::V3f(static_cast<float>(t), 0.0f, 0.0f));
    }

    try {
        std::vector < chrono_t > samples;
        samples.push_back( 0.04);
        //TimeSampling iTs(AbcA::TimeSamplingType(AbcA::TimeSamplingType::kAcyclic), samples);
        TimeSampling iTs(0.04, 1.68);
        TimeSamplingType tst =  iTs.getTimeSamplingType();
        chrono_t cycle = tst.getTimePerCycle();
        printf("frame cycle=%f, fps=%.1f\n", cycle, fps);

        oArchive.addTimeSampling(iTs);


        TimeSamplingPtr tsp(new TimeSampling(iTs));
        Alembic::AbcGeom::OXform xform(oRoot, "xform_node",tsp);

        // 创建曲线对象
        //Alembic::AbcGeom::OArchive abcArchive(oArchive, Alembic::Abc::ErrorHandler::kThrowPolicy);
        Alembic::AbcGeom::OCurves curves(xform, "my_curve", tsp);
        Alembic::AbcGeom::OCurvesSchema& oSchema = curves.getSchema();

        // 添加曲线数据
        Alembic::AbcGeom::OCurvesSchema::Sample oSamp;
        int32_t numCurves = 1;
        std::vector<Alembic::Util::int32_t> nVertices(numCurves);
        nVertices[0] = 100;
        oSamp.setCurvesNumVertices(Alembic::Abc::Int32ArraySample(nVertices));
        oSamp.setBasis(Alembic::AbcGeom::kBsplineBasis);
        std::vector<Alembic::Util::uint8_t> orders(numCurves);
        int degree = 2; // xgen always is 2
        orders[0] = degree+1; //degree + 1;
        oSamp.setOrders(Alembic::Abc::UcharArraySample(orders));
        oSamp.setType(kVariableOrder);

        oSamp.setPositions(Alembic::Abc::P3fArraySample(reinterpret_cast<const Imath::V3f*>(points.data()), numPoints));
        oSamp.setSelfBounds(Alembic::Abc::Box3d());
        //oSamp.setInvisible(Alembic::Abc::UIntArraySample());
        //oSamp.setTopologyVisibility(Alembic::Abc::UIntArraySample());

        //Alembic::AbcGeom::SampleSelector sampleSelector(static_cast<float>(startTime));
        oSchema.set(oSamp); // , sampleSelector);
    }
    catch (const std::exception& e) {
        // 捕获异常并输出错误信息
        std::cerr << "Error creating curves object: " << e.what() << std::endl;
    }

    return 0;
}
#if 0
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcCoreFactory/All.h>
//#include <Alembic/AbcCoreHDF5/All.h>

int main()
{
    // 创建一个ABC档案文件
    Alembic::AbcCoreFactory::IFactory factory;
    Alembic::AbcCoreFactory::IFactory::CoreType coreType = Alembic::AbcCoreFactory::IFactory::kOgawa;
    Alembic::AbcCoreFactory::IFactory::setCoreType(coreType);
    Alembic::AbcCoreFactory::IFactory::CoreType currentType = Alembic::AbcCoreFactory::IFactory::getCoreType();

    Alembic::AbcCoreFactory::IFactory::CoreType coreTypes[2] = { Alembic::AbcCoreFactory::IFactory::kHDF5,
                                                                 Alembic::AbcCoreFactory::IFactory::kOgawa };

    Alembic::AbcCoreFactory::IFactory::ObjectHeader objectHeader;
    objectHeader.setMetaData(Alembic::AbcCoreFactory::MetaData());
    Alembic::AbcCoreFactory::IFactory::setCoreType(currentType);

    Alembic::AbcCoreFactory::IFactory::setCoreType(coreTypes[coreType]);
    Alembic::AbcCoreFactory::IFactory::setOgawaNumStreamsToDisk(2);

    Alembic::AbcCoreFactory::IFactory::setHDF5NumThreads(2);
    Alembic::AbcCoreFactory::IFactory::setHDF5NumGroupsPerHierarchy(2);
    Alembic::AbcCoreFactory::IFactory::setHDF5UseSingleFile(false);

    Alembic::Abc::OArchive archive(Alembic::AbcCoreFactory::IFactory::getCoreType(), "curve_example.abc", objectHeader, Alembic::Abc::ErrorHandler::kThrowPolicy);

    // 创建一个时间范围
    double startTime = 1.0;
    double endTime = 100.0;

    // 创建曲线的示例数据
    int numPoints = 100;
    std::vector<Alembic::Abc::V3f> points;
    points.reserve(numPoints);

    for (int i = 0; i < numPoints; ++i)
    {
        double t = static_cast<double>(i) / (numPoints - 1);
        points.push_back(Alembic::Abc::V3f(static_cast<float>(t), 0.0f, 0.0f));
    }

    // 创建曲线对象
    Alembic::AbcGeom::OCurves curves(archive.getTop(), "my_curve", 1);
    Alembic::AbcGeom::OCurvesSchema& curvesSchema = curves.getSchema();

    // 添加曲线数据
    Alembic::AbcGeom::OCurvesSchema::Sample curvesSample;
    curvesSample.setPositions(Alembic::Abc::P3fArraySample(reinterpret_cast<const Imath::V3f*>(points.data()), numPoints));
    curvesSample.setSelfBounds(Alembic::Abc::Box3d());
    curvesSample.setInvisible(Alembic::Abc::UIntArraySample());
    curvesSample.setTopologyVisibility(Alembic::Abc::UIntArraySample());

    Alembic::AbcGeom::SampleSelector sampleSelector(static_cast<float>(startTime));
    curvesSchema.set(curvesSample, sampleSelector);

    // 关闭存档
    archive.getArchive().getErrorHandler().cleanup();

    return 0;
}
#elif 0
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreOgawa/All.h>

int main()
{
    // 创建一个 ABC 存档文件（使用 Ogawa 文档类型）
    Alembic::AbcCoreOgawa::WriteArchive archive = Alembic::AbcCoreOgawa::WriteArchive(); // ("curve_example.abc");

    // 创建一个时间范围
    double startTime = 1.0;
    double endTime = 100.0;

    // 创建曲线的示例数据
    int numPoints = 100;
    std::vector<Alembic::Abc::V3f> points;
    points.reserve(numPoints);

    for (int i = 0; i < numPoints; ++i)
    {
        double t = static_cast<double>(i) / (numPoints - 1);
        points.push_back(Alembic::Abc::V3f(static_cast<float>(t), 0.0f, 0.0f));
    }

    // 创建曲线对象
    Alembic::AbcGeom::OArchive abcArchive(archive, Alembic::Abc::ErrorHandler::kThrowPolicy);
    Alembic::AbcGeom::OCurves curves(abcArchive.getTop(), "my_curve", 1);
    Alembic::AbcGeom::OCurvesSchema& curvesSchema = curves.getSchema();

    // 添加曲线数据
    Alembic::AbcGeom::OCurvesSchema::Sample curvesSample;
    curvesSample.setPositions(Alembic::Abc::P3fArraySample(reinterpret_cast<const Imath::V3f*>(points.data()), numPoints));
    curvesSample.setSelfBounds(Alembic::Abc::Box3d());
    //curvesSample.setInvisible(Alembic::Abc::UIntArraySample());
    //curvesSample.setTopologyVisibility(Alembic::Abc::UIntArraySample());

    //Alembic::AbcGeom::SampleSelector sampleSelector(static_cast<float>(startTime));
    curvesSchema.set(curvesSample); // , sampleSelector);

    return 0;
}

#endif