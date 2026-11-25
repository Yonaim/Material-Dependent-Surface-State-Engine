/**
 * @file GeometryExperiments.cpp
 * @brief 메시와 맵 입력의 높이 오차 및 형상 대역별 저장 용량을 비교한다.
 */

#include "Application/Editor/SurfaceSimulation/GeometryExperiments.h"
#include "AssetManager/Loaders/LiquidMaterialLoader.h"
#include "SurfaceState/Geometry/HeightfieldSources.h"
#include "SurfaceState/Simulation/SurfaceSimulationDomain.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <numeric>
#include <stdexcept>

namespace MDSS::SurfaceSimulation
{
    void CheckGeometryImports(const std::filesystem::path &FixtureDirectory, const std::filesystem::path &Output)
    {
        using namespace SurfaceState;
        constexpr float Width = 0.09F;
        constexpr float Length = 0.06F;
        const auto      Load = [&](EHeightfieldSource Source, const char *Name, float Scale)
        { return LoadHeightfield(Source, FixtureDirectory / Name, 52, 36, Width, Length, Scale); };
        const auto         Mesh = Load(EHeightfieldSource::Mesh, "patch.obj", 1.F);
        const auto         Height = Load(EHeightfieldSource::HeightMap, "height.ppm", 0.009F);
        const auto         Displacement = Load(EHeightfieldSource::DisplacementMap, "displacement.ppm", 0.009F);
        const auto         Normals = Load(EHeightfieldSource::NormalMap, "normal.ppm", 1.F);
        const auto         Bands = SplitHeightfield(Mesh, 95, 2);
        const auto        &Base = Bands.Base.Heights;
        std::vector<float> Resolved(Base.size());
        for (std::size_t Index = 0; Index < Base.size(); ++Index)
            Resolved[Index] = Base[Index] + Bands.Meso.Heights[Index];

        auto Error = [](std::span<const float> A, std::span<const float> B)
        {
            std::vector<double> Diff(A.size());
            std::transform(A.begin(),
                           A.end(),
                           B.begin(),
                           Diff.begin(),
                           [](float X, float Y) { return static_cast<double>(X) - Y; });
            const double Count = static_cast<double>(Diff.size());
            const double Mean = std::accumulate(Diff.begin(), Diff.end(), 0.0) / Count;
            for (double &Value : Diff)
                Value -= Mean;
            return std::sqrt(std::inner_product(Diff.begin(), Diff.end(), Diff.begin(), 0.0) / Count);
        };
        const double HeightError = Error(Mesh.Heights, Height.Heights);
        const double DisplacementError = Error(Mesh.Heights, Displacement.Heights);
        const double NormalError = Error(Mesh.Heights, Normals.Heights);
        for (std::size_t Index = 0; Index < Base.size(); ++Index)
            if (std::abs(Resolved[Index] + Bands.Micro.Heights[Index] - Mesh.Heights[Index]) > 1.e-8F)
                throw std::runtime_error("Frequency split did not reconstruct its input.");

        double     CapacityDifference = 0.0;
        const auto X = Liquid::Partition(Mesh.Columns, 4), Y = Liquid::Partition(Mesh.Rows, 3);
        const auto Material = Asset::LoadLiquidMaterial({});
        const auto G0 = Liquid::MakeGeometry(Bands.Base, X, Y, false, Material);
        auto       ResolvedShape = Mesh;
        ResolvedShape.Heights = Resolved;
        const auto G1 = Liquid::MakeGeometry(std::move(ResolvedShape), X, Y, false, Material);
        for (int Cell = 0; Cell < static_cast<int>(G0.Cells.size()); ++Cell)
            CapacityDifference = std::max(
                CapacityDifference,
                std::abs(Liquid::VolumeAtLevel(G0, Cell, {}, 0.0025) - Liquid::VolumeAtLevel(G1, Cell, {}, 0.0025)));

        if (HeightError > 2.e-5 || DisplacementError > 4.e-5 || NormalError > 1.4e-4 ||
            Error(Resolved, Mesh.Heights) < 1.e-5 || Error(Base, Resolved) < 7.e-5 || CapacityDifference < 1.e-9)
            throw std::runtime_error("Geometry imports or frequency split did not meet the analytic fixture bounds.");

        const auto Millimeters = [](std::vector<float> Values)
        {
            for (float &Value : Values)
                Value *= 1000.F;
            return Values;
        };
        const nlohmann::json Report{
            {"scene", "Geometry source comparison"},
            {"purpose", "Aligned 52 x 36 XY samples from a 17,549-vertex OBJ and three maps."},
            {"summary",
             {{"height_rms_mm", HeightError * 1000},
              {"displacement_rms_mm", DisplacementError * 1000},
              {"normal_rms_mm", NormalError * 1000},
              {"cavity_capacity_difference_mL", CapacityDifference * 1.e6}}},
            {"geometry",
             {{"columns", Mesh.Columns},
              {"rows", Mesh.Rows},
              {"height_mm", Millimeters(Mesh.Heights)},
              {"depth_mm", Millimeters(Base)},
              {"absorbed_kg_m2", Millimeters(Bands.Meso.Heights)},
              {"char_fraction", Millimeters(Bands.Micro.Heights)}}},
            {"field_labels",
             {"Full OBJ projection (mm)", "Low-frequency base (mm)", "Meso residual (mm)", "Micro residual (mm)"}}};
        std::cout << "geometry " << Report["summary"].dump() << '\n';
        if (!Output.empty())
        {
            std::filesystem::create_directories(Output);
            std::ofstream File(Output / "geometry.json");
            File << Report.dump() << '\n';
            if (!File)
                throw std::runtime_error("Cannot write the geometry experiment report.");
        }
    }
} // namespace MDSS::SurfaceSimulation
