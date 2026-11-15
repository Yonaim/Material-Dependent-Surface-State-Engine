/**
 * @file MesoStudy.h
 * @brief 연구 입력의 Meso 형상을 실제 전처리 경로에 연결하고 형상 진단을 저장한다.
 */

// Research input/diagnostic adapter. Production solvers and geometry equations are unchanged.
#pragma once
#include "AssetManager/Loaders/TextureLoader.h"
#include "SurfaceState/Mapping/NormalMapTransferNormalBuilder.h"

namespace MDSS::SurfaceState
{
    void RebuildMesoStudyGeometry(FSharedSurfaceGeometryData& Geometry);
}

FJson ConfigureMesoStudy(FSharedSurfaceGeometryData&                          Geometry,
                         const std::vector<MDSS::Asset::FVertex>&             Vertices,
                         const std::vector<MDSS::Asset::FMeshTriangleSource>& Triangles,
                         const FJson&                                         Plan,
                         const fs::path&                                      Out)
{
    const auto        Config = Plan.value("meso", FJson::object());
    const std::string Mode = Config.value("mode", std::string("flat"));

    if (Mode != "flat" && Mode != "analytic" && Mode != "normal_map")
        throw std::runtime_error("Unknown meso input mode");
    MDSS::Asset::FTextureData Texture;

    if (Mode == "normal_map")
        Texture = MDSS::Asset::FTextureLoader::LoadRGBA8(Config.at("path").get<std::string>());
    const float Strength = Config.value("strength", 1.0F), Amplitude = Config.value("amplitude", .015F);

    if (!std::isfinite(Strength) || Strength <= 0)
        throw std::runtime_error("Invalid meso strength");
    auto&              Texels = Geometry.GetTexels();
    const auto         N = Texels.size();
    std::vector<float> Position(N * 3), MacroNormal(N * 3), Transfer(N * 3), Derived(N * 3), Height(N), Truth(N),
        Valid(N), Neighbors(N * 8), Charts(N);
    std::size_t Samples = 0;

    for (std::size_t I = 0; I < N; ++I)
    {
        auto& T = Texels[I];
        Valid[I] = T.IsValid() ? 1.0F : 0.0F;
        Charts[I] = float(T.Chart);

        for (unsigned D = 0; D < 8; ++D)
            Neighbors[I * 8 + D] = T.NeighborIndices[D] == InvalidTexelIndex ? -1.0F : float(T.NeighborIndices[D]);
        if (!T.IsValid())
            continue;
        T.MesoNormal = T.Normal;
        T.bHasMesoNormal = true;
        glm::vec3 Normal = T.Normal;

        if (Mode == "analytic")
        {
            if (glm::length(T.Normal - glm::vec3(0, 0, 1)) > 1e-5F)
                throw std::runtime_error("Analytic meso requires a +Z plane");
            constexpr float Omega = 12.566370614359172F;
            const float     X = T.Position.x, Y = T.Position.y;
            const float     HX = -Amplitude * Omega * std::sin(Omega * X) * std::cos(Omega * Y);
            const float     HY = -Amplitude * Omega * std::cos(Omega * X) * std::sin(Omega * Y);
            Normal = glm::normalize(glm::vec3(-HX, -HY, 1));
        }

        if (Mode == "normal_map" &&
            !BuildNormalMapTransferNormal(T, Vertices, Triangles, Texture, Normal, Config.value("flip_y", true)))
            throw std::runtime_error("Normal map sampling failed for a valid texel");
        if (Mode != "flat")
        {
            const float Z = glm::dot(Normal, T.Normal);

            if (Z <= 0)
                throw std::runtime_error("Study normal points below the macro tangent plane");
            Normal = glm::normalize(T.Normal + (Normal - T.Normal * Z) * (Strength / Z));
            T.TransferNormal = Normal;
            T.bHasTransferNormal = true;
            ++Samples;
        }

        Truth[I] =
            Amplitude * std::cos(12.566370614359172F * T.Position.x) * std::cos(12.566370614359172F * T.Position.y);
        for (int J = 0; J < 3; ++J)
        {
            Position[I * 3 + J] = T.Position[J];
            MacroNormal[I * 3 + J] = T.Normal[J];
            Transfer[I * 3 + J] = Normal[J];
        }
    }

    FMesoGeometryBuildReport Report;

    if (Mode != "flat")
        Report = BuildMesoGeometry(Geometry);
    for (std::size_t I = 0; I < N; ++I)
        Height[I] = Texels[I].Geometry.MesoVirtualHeight;
    Save(Out / "meso-production-height.f32", Height);

    if (Config.contains("height_file"))
    {
        std::ifstream F(Config.at("height_file").get<std::string>(), std::ios::binary | std::ios::ate);

        if (!F || F.tellg() != std::streamoff(N * sizeof(float)))
            throw std::runtime_error("Meso reference height size mismatch");
        F.seekg(0);
        F.read(reinterpret_cast<char*>(Height.data()), std::streamsize(N * sizeof(float)));

        for (std::size_t I = 0; I < N; ++I)
        {
            if (!std::isfinite(Height[I]))
                throw std::runtime_error("Nonfinite reference height");
            Texels[I].Geometry.MesoVirtualHeight = Height[I];
        }

        RebuildMesoStudyGeometry(Geometry);
    }

    for (std::size_t I = 0; I < N; ++I)
        for (int J = 0; J < 3; ++J)
            Derived[I * 3 + J] = Texels[I].MesoNormal[J];
    Save(Out / "meso-height.f32", Height);
    Save(Out / "meso-truth.f32", Truth);
    Save(Out / "meso-positions.f32", Position);
    Save(Out / "meso-macro-normals.f32", MacroNormal);
    Save(Out / "meso-transfer-normals.f32", Transfer);
    Save(Out / "meso-derived-normals.f32", Derived);
    Save(Out / "meso-valid.f32", Valid);
    Save(Out / "meso-neighbors.f32", Neighbors);
    Save(Out / "meso-charts.f32", Charts);
    FJson Result = {
        {"mode", Mode},
        {"samples", Samples},
        {"strength", Strength},
        {"flip_y", Config.value("flip_y", true)},
        {"production_active", Report.ActiveTexelCount},
        {"production_components", Report.ComponentCount},
        {"production_iterations", Report.IterationCount},
        {"production_relative_edge_residual", Report.RelativeEdgeResidual},
        {"height_override", Config.value("height_file", std::string())},
        {"texels", N},
        {"warnings", "Single flat quad, fixed graph/area; not a general mesh or physical height validation"}};
    std::ofstream(Out / "meso.json") << Result.dump(2);

    return Result;
}
