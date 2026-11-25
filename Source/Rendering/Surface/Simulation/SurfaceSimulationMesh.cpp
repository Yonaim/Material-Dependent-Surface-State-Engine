/**
 * @file SurfaceSimulationMesh.cpp
 * @brief 계산 셀과 형상 샘플에서 표시 메시 및 셰이더용 필드를 생성한다.
 */

#include "Rendering/Surface/Simulation/SurfaceSimulationMesh.h"
#include "SurfaceState/Simulation/SurfaceLocalUpdates.h"
#include "SurfaceState/Simulation/SurfaceSimulationSnapshot.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <limits>
#include <numbers>

namespace MDSS::SurfaceSimulation
{
    // ============================================================
    // 00. Mesh Construction Helpers
    // ============================================================
    #pragma region 00_Mesh_Construction_Helpers
        // 바닥 측벽과 효과의 표시 영역에 필요한 정점 및 인덱스를 생성한다.

        namespace
        {
            void AppendQuad(FSurfaceSimulationMesh         &Mesh,
                            const std::array<glm::vec3, 4> &Points,
                            const std::array<glm::vec3, 4> &Normals,
                            std::uint32_t                   Cell,
                            std::uint32_t                   Region = 0)
            {
                const auto Base = static_cast<std::uint32_t>(Mesh.Vertices.size());
                for (std::size_t Index = 0; Index < Points.size(); ++Index)
                    Mesh.Vertices.push_back({Points[Index], Normals[Index], Cell, Region});
                Mesh.Indices.insert(Mesh.Indices.end(), {Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
            }

            void AppendWall(FSurfaceSimulationMesh         &Mesh,
                            const std::array<glm::vec3, 4> &Points,
                            glm::vec3                       Normal,
                            std::uint32_t                   Cell)
            {
                AppendQuad(Mesh, Points, {Normal, Normal, Normal, Normal}, Cell);
            }

            void AppendGlassVessel(FSurfaceSimulationMesh &Mesh, const FSurfaceRenderField &Field)
            {
                const float                    Thickness = Field.SectionWindow.w;
                const float                    Top = Field.BedMetrics.x + Field.SectionWindow.y;
                const float                    Base = Field.BedMetrics.x - Thickness;
                const glm::vec2                InnerMin{Field.VesselBounds.x, Field.VesselBounds.y};
                const glm::vec2                InnerMax{Field.VesselBounds.z, Field.VesselBounds.w};
                const glm::vec2                OuterMin = InnerMin - Thickness, OuterMax = InnerMax + Thickness;
                const std::array<glm::vec2, 4> Inner{
                    {InnerMin, {InnerMax.x, InnerMin.y}, InnerMax, {InnerMin.x, InnerMax.y}}};
                const std::array<glm::vec2, 4> Outer{
                    {OuterMin, {OuterMax.x, OuterMin.y}, OuterMax, {OuterMin.x, OuterMax.y}}};
                constexpr std::array<glm::vec3, 4> Normals{
                    {{0.F, -1.F, 0.F}, {1.F, 0.F, 0.F}, {0.F, 1.F, 0.F}, {-1.F, 0.F, 0.F}}};
                for (std::size_t Edge = 0; Edge < Outer.size(); ++Edge)
                {
                    const auto Next = (Edge + 1) % Outer.size();
                    AppendWall(Mesh,
                               {{glm::vec3(Outer[Edge], Base),
                                 glm::vec3(Outer[Next], Base),
                                 glm::vec3(Outer[Next], Top),
                                 glm::vec3(Outer[Edge], Top)}},
                               Normals[Edge],
                               0);
                    AppendWall(Mesh,
                               {{glm::vec3(Outer[Edge], Top),
                                 glm::vec3(Outer[Next], Top),
                                 glm::vec3(Inner[Next], Top),
                                 glm::vec3(Inner[Edge], Top)}},
                               {0.F, 0.F, 1.F},
                               0);
                }
                AppendWall(Mesh,
                           {{glm::vec3(Outer[0], Base),
                             glm::vec3(Outer[1], Base),
                             glm::vec3(Outer[2], Base),
                             glm::vec3(Outer[3], Base)}},
                           {0.F, 0.F, -1.F},
                           0);
            }

            void AppendEffectBounds(FSurfaceSimulationMesh &Mesh, const FSurfaceRenderField &Field)
            {
                if (Field.BoundsMin.x >= Field.BoundsMax.x)
                    return;
                const auto Base = static_cast<std::uint32_t>(Mesh.Vertices.size());
                for (std::uint32_t Corner = 0; Corner < 8; ++Corner)
                {
                    const glm::vec3 Position{(Corner & 1U) ? Field.BoundsMax.x : Field.BoundsMin.x,
                                             (Corner & 2U) ? Field.BoundsMax.y : Field.BoundsMin.y,
                                             (Corner & 4U) ? Field.BoundsMax.z : Field.BoundsMin.z};
                    Mesh.Vertices.push_back({Position, {}, 0U, 0U});
                }
                constexpr std::array<std::uint32_t, 36> BoxIndices{0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6, 0, 1, 5, 0, 5, 4,
                                                                   2, 6, 7, 2, 7, 3, 0, 4, 6, 0, 6, 2, 1, 3, 7, 1, 7, 5};
                for (const auto Index : BoxIndices)
                    Mesh.Indices.push_back(Base + Index);
                Mesh.EffectIndexCount = static_cast<std::uint32_t>(BoxIndices.size());
            }
        } // namespace

    #pragma endregion

    // ============================================================
    // 01. Shader Field Packing
    // ============================================================
    #pragma region 01_Shader_Field_Packing
        // 샘플 격자와 계산 셀의 간격 및 경계를 셰이더가 사용하는 배열에 채운다.

        FSurfaceRenderField MakeSurfaceRenderField(const Liquid::FSurfaceSimulationDomain        &Geometry,
                                                   const Rendering::FSurfaceSimulationAppearance &Appearance)
        {
            FSurfaceRenderField Field;
            const auto         &Shape = Geometry.Shape;
            Field.RegionCount = static_cast<int>(Geometry.Regions.size());
            const Liquid::FSurfaceSnapshotLayout Layout(Geometry.Cells.size(),
                                                        Geometry.Regions.size(),
                                                        Geometry.Contacts.size(),
                                                        Geometry.FilmLayers.size() > 1 ? Geometry.FilmLayers.size() : 0);
            if (Geometry.FilmLayers.size() > 1)
                Field.FilmDisplayWord = static_cast<int>(Layout.FilmOffset / sizeof(float));
            Field.ReactionContactWord = static_cast<int>(Layout.ContactOffset / sizeof(float));
            for (const auto &Material : Geometry.Materials)
                Field.AbsorbCapacities.push_back(Material.CapacityKgM2);
            Field.FilmIceLayer = Geometry.FilmIceLayer;
            for (const auto &Layer : Geometry.FilmLayers)
                Field.FilmDensities.push_back(Layer.DensityKgM3);
            Field.CellRegionCounts.resize(Geometry.Cells.size());
            Field.CellMaxBedHeights.resize(Geometry.Cells.size());
            Field.RegionInverseAreas.resize(Geometry.Regions.size());
            Field.LiquidOptics = {Appearance.LiquidOpticalDepthScale,
                                  Appearance.LiquidCoverageDepthMeters,
                                  Geometry.Liquid.RefractiveIndex,
                                  Geometry.Liquid.Roughness};
            Field.LiquidTint = Appearance.LiquidTint;
            const auto &Window = Appearance.SectionWindow;
            Field.SectionWindow = {(Window.FrontV - 0.5F) * Shape.Length,
                                   Window.HeightMeters,
                                   Window.GraduationMeters,
                                   Window.GlassThicknessMeters};
            Field.VesselBounds = (Window.VesselInteriorUV - glm::vec4(0.5F)) *
                                 glm::vec4(Shape.Width, Shape.Length, Shape.Width, Shape.Length);
            const glm::vec2 SolidBed = (Window.SolidBedRangeU - glm::vec2(0.5F)) * Shape.Width;
            Field.SectionSolidBed = {SolidBed.x, SolidBed.y, 0.F, 0.F};
            const auto &Optics = Appearance.Surface.Optics;
            const float Amplitude = (Optics.RefractiveIndex - 1.F) / (Optics.RefractiveIndex + 1.F);
            const float SpecularF0 = std::clamp(Optics.SpecularScale * Amplitude * Amplitude, 0.F, 1.F);
            Field.SubstrateOptics = {
                Optics.DryRoughness,
                Optics.WetRoughness,
                SpecularF0,
                Appearance.Specimens.empty() && Appearance.Surface.NormalTexturePath.empty() ? 0.F : Optics.NormalStrength};
            Field.IceOptics = {Geometry.Liquid.IceRoughness, Geometry.Liquid.FrostRoughness};
            Field.SubstrateReliefMeters = Appearance.bChartAlbedo ? 0.F : Optics.ReliefDepthMeters;
            const auto &Wetness = Appearance.AbsorbedWetnessDepth;
            if (Wetness.DiffusivityM2S > 0.F)
            {
                constexpr float Pi = std::numbers::pi_v<float>;
                const float     Thickness = Appearance.DisplayThicknessMeters;
                const float     InverseTime = Pi * Pi * Wetness.DiffusivityM2S / (Thickness * Thickness);
                Field.AbsorbedWetnessDepth = {Wetness.InitialContrast,
                                              InverseTime,
                                              Wetness.RedistributionStartSeconds,
                                              0.F};
            }
            Field.CharContrast = Appearance.CharContrast;
            Field.DisplayGapMeters = Appearance.DisplayGapMeters;
            Field.EnvironmentOptics = {Appearance.Environment.Exposure, Appearance.Environment.YawRadians};
            Field.SpecimenCount = static_cast<std::uint32_t>(Appearance.SpecimenCount);
            for (std::size_t Index = 0; Index < Appearance.Specimens.size(); ++Index)
                if (Appearance.Specimens[Index].bMetallic)
                    Field.MetallicSpecimens |= 1U << Index;
            Field.SampleLayout = {Shape.Width, Shape.Length, Shape.PitchX(), Shape.PitchY()};
            Field.Counts[0] = Shape.Columns;
            Field.Counts[1] = Shape.Rows;
            Field.ContactIndices.assign(2 * Shape.Heights.size(), -1);
            for (std::size_t Index = 0; Index < Geometry.Contacts.size(); ++Index)
                Field.ContactIndices[Geometry.Contacts[Index]] = static_cast<int>(Index);
            // 표시용 연결 면은 같은 배열 뒤의 접촉 목록을 직접 순회한다.
            Field.ContactIndices.insert(Field.ContactIndices.end(), Geometry.Contacts.begin(), Geometry.Contacts.end());
            Field.Heights = Shape.Heights;
            Field.SampleRegions = Geometry.SampleRegions;
            for (std::size_t Region = 0; Region < Geometry.Regions.size(); ++Region)
                Field.RegionInverseAreas[Region] =
                    1.F / (Shape.PitchX() * Shape.PitchY() * Geometry.Regions[Region].SampleCount);
            glm::vec2  Lower(std::numeric_limits<float>::max());
            glm::vec2  Upper(std::numeric_limits<float>::lowest());
            const auto Heights = std::ranges::minmax_element(Shape.Heights);
            Field.BedMetrics = {*Heights.min,
                                *Heights.max,
                                *Heights.min - Appearance.DisplayThicknessMeters,
                                Appearance.bIceSubstrate ? 1.F : 0.F};
            Field.IceProperties = {Geometry.Liquid.IceRefractiveIndex,
                                   Geometry.Liquid.IceDensityKgM3,
                                   Appearance.Snow.BulkDensityKgM3,
                                   Appearance.WetClothReferenceKgM2};
            if (Geometry.InitialLiquidKg > 0.F && Geometry.Materials[Geometry.SourceCell()].FreezeKgM2SecondK > 0.F)
            {
                const int Cell = Geometry.SourceCell();
                Field.PhaseLevels = {Liquid::RestoreLevel(Geometry, Cell, {}, Geometry.InitialLiquidKg),
                                     Liquid::RestoreLevel(Geometry,
                                                          Cell,
                                                          {},
                                                          Geometry.InitialLiquidKg * (Geometry.Liquid.DensityKgM3 /
                                                                                      Geometry.Liquid.IceDensityKgM3))};
            }
            bool bHasFlame = false;
            bool bHasSteam = false;
            for (std::size_t Index = 0; Index < Geometry.Cells.size(); ++Index)
            {
                Field.CellRegionCounts[Index] = Geometry.CellRegions[Index].Count;
                const auto &Cell = Geometry.Cells[Index];
                float       MaxBedHeight = std::numeric_limits<float>::lowest();
                for (int Y = Cell.FirstRow; Y < Cell.EndRow; ++Y)
                    for (int X = Cell.FirstColumn; X < Cell.EndColumn; ++X)
                        MaxBedHeight = std::max(MaxBedHeight, Shape.Height(X, Y));
                Field.CellMaxBedHeights[Index] = MaxBedHeight;
                const float X0 = Shape.EdgeX(Cell.FirstColumn), X1 = Shape.EdgeX(Cell.EndColumn);
                const float Y0 = Shape.EdgeY(Cell.FirstRow), Y1 = Shape.EdgeY(Cell.EndRow);
                if (Cell.FirstRow == 0)
                {
                    const int Column = Field.Counts[2]++;
                    Field.CenterX.push_back((X0 + X1) * 0.5F);
                    if (Column == 0)
                        Field.EdgeX.push_back(X0);
                    Field.EdgeX.push_back(X1);
                }
                if (Cell.FirstColumn == 0)
                {
                    const int Row = Field.Counts[3]++;
                    Field.CenterY.push_back((Y0 + Y1) * 0.5F);
                    if (Row == 0)
                        Field.EdgeY.push_back(Y0);
                    Field.EdgeY.push_back(Y1);
                }
                const auto &Material = Geometry.Materials[Index];
                const bool  bFlame = Material.CharPerSecond > 0.F;
                const bool  bSteam =
                    Material.EvaporateKgM2SecondK > 0.F || Geometry.SubstrateQuench.ContactConductanceWm2K > 0.F;
                if (!bFlame && !bSteam)
                    continue;
                Lower = glm::min(Lower, glm::vec2(X0, Y0));
                Upper = glm::max(Upper, glm::vec2(X1, Y1));
                bHasFlame |= bFlame;
                bHasSteam |= bSteam;
            }
            if (bHasFlame || bHasSteam)
            {
                const float Margin = bHasSteam ? 0.034F : 0.009F;
                const float Height = bHasSteam ? 0.083F : 0.057F;
                Field.BoundsMin = glm::vec4(Lower - glm::vec2(Margin), *Heights.min - 0.001F, bHasSteam ? 1.F : 0.F);
                Field.BoundsMax = glm::vec4(Upper + glm::vec2(Margin), *Heights.max + Height, bHasFlame ? 1.F : 0.F);
            }
            return Field;
        }

        std::vector<std::byte> PackSurfaceRenderField(const FSurfaceRenderField &Field)
        {
            // SurfaceDisplayField.glsl의 고정 헤더와 배열 오프셋 순서를 유지한다.
            static_assert(sizeof(FSurfaceRenderMetadata) == 560 && offsetof(FSurfaceRenderMetadata, RegionCount) == 460 &&
                          offsetof(FSurfaceRenderMetadata, EnvironmentOptics) == 464 &&
                          offsetof(FSurfaceRenderMetadata, SpecimenCount) == 472 &&
                          offsetof(FSurfaceRenderMetadata, MetallicSpecimens) == 476);
            static_assert(offsetof(FSurfaceRenderMetadata, StateVisuals) == 160 &&
                          offsetof(FSurfaceRenderMetadata, PhaseLevels) == 448 &&
                          offsetof(FSurfaceRenderMetadata, SectionWindow) == 496 &&
                          offsetof(FSurfaceRenderMetadata, VesselBounds) == 512 &&
                          offsetof(FSurfaceRenderMetadata, SectionSolidBed) == 528 &&
                          offsetof(FSurfaceRenderMetadata, AbsorbedWetnessDepth) == 544);
            std::array<int, 14>    Offsets{};
            const std::size_t      HeaderBytes = sizeof(FSurfaceRenderMetadata) + sizeof(Offsets);
            std::vector<std::byte> Bytes(HeaderBytes);
            std::size_t            NextOffset = 0;
            const auto             Append = [&](const auto &Values)
            {
                const auto Start = Bytes.size();
                const auto Span = std::as_bytes(std::span(Values));
                Offsets[NextOffset++] = static_cast<int>((Start - HeaderBytes) / sizeof(std::uint32_t));
                Bytes.insert(Bytes.end(), Span.begin(), Span.end());
            };
            Append(Field.CenterX);
            Append(Field.CenterY);
            Append(Field.Heights);
            Append(Field.EdgeX);
            Append(Field.EdgeY);
            Append(Field.CellRegionCounts);
            Append(Field.SampleRegions);
            Append(Field.RegionInverseAreas);
            // Keep the offset table stable. Slosh reads one bed bound per cell immediately before contact indices.
            const auto BedBounds = std::as_bytes(std::span(Field.CellMaxBedHeights));
            Bytes.insert(Bytes.end(), BedBounds.begin(), BedBounds.end());
            Append(Field.ContactIndices);
            Offsets[NextOffset++] = Field.FilmDisplayWord;
            Append(Field.FilmDensities);
            Offsets[NextOffset++] = Field.FilmIceLayer;
            Offsets[NextOffset++] = Field.ReactionContactWord;
            Append(Field.AbsorbCapacities);
            const auto &Metadata = static_cast<const FSurfaceRenderMetadata &>(Field);
            std::memcpy(Bytes.data(), &Metadata, sizeof(Metadata));
            std::memcpy(Bytes.data() + sizeof(Metadata), Offsets.data(), sizeof(Offsets));
            return Bytes;
        }

    #pragma endregion

    // ============================================================
    // 02. Display Mesh Generation
    // ============================================================
    #pragma region 02_Display_Mesh_Generation
        // 공통 바닥 위에 개별 드로우가 참조할 물, 수증기 및 불꽃의 인덱스 범위를 추가한다.

        FSurfaceSimulationMesh MakeSurfaceSimulationMesh(const Liquid::FSurfaceSimulationDomain &Geometry,
                                                         const FSurfaceRenderField              &Field,
                                                         bool                                    bSmoothDisplay,
                                                         float                                   ThicknessMeters)
        {
            FSurfaceSimulationMesh Mesh;
            const auto            &Shape = Geometry.Shape;
            const int              Columns = Shape.Columns, Rows = Shape.Rows;
            const float            Lower = *std::ranges::min_element(Shape.Heights) - ThicknessMeters;
            const int              Specimens = static_cast<int>(Field.SpecimenCount);
            const int              SpecimenWidth = Columns / Specimens;
            const bool             bSplit = Field.DisplayGapMeters > 0.F;
            const auto             CellAt = [&](int X, int Y)
            { return Geometry.SampleCells[std::clamp(Y, 0, Rows - 1) * Columns + std::clamp(X, 0, Columns - 1)]; };
            const auto Height = [&](int X, int Y)
            { return X >= 0 && X < Columns && Y >= 0 && Y < Rows ? Shape.Height(X, Y) : Lower; };
            const auto SmoothHeight = [&](int X, int Y, int SampleColumn)
            {
                const int First = (SampleColumn / SpecimenWidth) * SpecimenWidth;
                const int Last = First + SpecimenWidth - 1;
                const int Left = std::clamp(X - 1, First, Last), Right = std::clamp(X, First, Last);
                const int Bottom = std::clamp(Y - 1, 0, Rows - 1), Top = std::clamp(Y, 0, Rows - 1);
                return 0.25F * (Height(Left, Bottom) + Height(Left, Top) + Height(Right, Bottom) + Height(Right, Top));
            };
            const auto SmoothNormal = [&](int X, int Y, int SampleColumn)
            {
                const float DX =
                    (SmoothHeight(X + 1, Y, SampleColumn) - SmoothHeight(X - 1, Y, SampleColumn)) / (2.F * Shape.PitchX());
                const float DY =
                    (SmoothHeight(X, Y + 1, SampleColumn) - SmoothHeight(X, Y - 1, SampleColumn)) / (2.F * Shape.PitchY());
                return glm::normalize(glm::vec3{-DX, -DY, 1.F});
            };
            for (int Y = 0; Y < Rows; ++Y)
                for (int X = 0; X < Columns; ++X)
                {
                    std::array<glm::vec3, 4>                    Points, Normals;
                    constexpr std::array<std::array<int, 2>, 4> Corners{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
                    for (std::size_t Corner = 0; Corner < Corners.size(); ++Corner)
                    {
                        const int   I = X + Corners[Corner][0], J = Y + Corners[Corner][1];
                        const float Z = bSmoothDisplay ? SmoothHeight(I, J, X) : Height(X, Y);
                        Points[Corner] = {Shape.EdgeX(I), Shape.EdgeY(J), Z};
                        Normals[Corner] = bSmoothDisplay ? SmoothNormal(I, J, X) : glm::vec3{0.F, 0.F, 1.F};
                    }
                    AppendQuad(Mesh, Points, Normals, CellAt(X, Y), Geometry.SampleRegions[Y * Columns + X]);
                }
            Mesh.TopIndexCount = static_cast<std::uint32_t>(Mesh.Indices.size());

            for (int Y = 0; Y < Rows; ++Y)
                for (int X = 0; X <= Columns; ++X)
                {
                    const float Edge = Shape.EdgeX(X), Y0 = Shape.EdgeY(Y), Y1 = Shape.EdgeY(Y + 1);
                    const float Left = Height(X - 1, Y), Right = Height(X, Y);
                    const auto  CloseSide = [&](int SampleColumn, float NormalX)
                    {
                        const float Top0 = bSmoothDisplay ? SmoothHeight(X, Y, SampleColumn) : Height(SampleColumn, Y);
                        const float Top1 = bSmoothDisplay ? SmoothHeight(X, Y + 1, SampleColumn) : Top0;
                        AppendWall(Mesh,
                                   {{{Edge, Y0, Lower}, {Edge, Y1, Lower}, {Edge, Y1, Top1}, {Edge, Y0, Top0}}},
                                   {NormalX, 0.F, 0.F},
                                   CellAt(SampleColumn, Y));
                    };
                    if (bSplit && X > 0 && X < Columns && X % SpecimenWidth == 0)
                    {
                        // 시편의 경계는 각자 화면에 노출되는 외벽을 가진다.
                        CloseSide(X - 1, 1.F);
                        CloseSide(X, -1.F);
                    }
                    else if (bSmoothDisplay && (X == 0 || X == Columns))
                        CloseSide(X == 0 ? 0 : Columns - 1, X == 0 ? -1.F : 1.F);
                    else if (!bSmoothDisplay && Left != Right)
                        AppendWall(Mesh,
                                   {{{Edge, Y0, Left}, {Edge, Y1, Left}, {Edge, Y1, Right}, {Edge, Y0, Right}}},
                                   {Left > Right ? 1.F : -1.F, 0.F, 0.F},
                                   CellAt(Left > Right ? X - 1 : X, Y));
                }

            for (int Y = 0; Y <= Rows; ++Y)
                for (int X = 0; X < Columns; ++X)
                {
                    const float Edge = Shape.EdgeY(Y), X0 = Shape.EdgeX(X), X1 = Shape.EdgeX(X + 1);
                    const float LowerSide = Height(X, Y - 1), UpperSide = Height(X, Y);
                    if (bSmoothDisplay && (Y == 0 || Y == Rows))
                        AppendWall(Mesh,
                                   {{{X0, Edge, Lower},
                                     {X1, Edge, Lower},
                                     {X1, Edge, SmoothHeight(X + 1, Y, X)},
                                     {X0, Edge, SmoothHeight(X, Y, X)}}},
                                   {0.F, Y == 0 ? -1.F : 1.F, 0.F},
                                   CellAt(X, Y));
                    else if (!bSmoothDisplay && LowerSide != UpperSide)
                        AppendWall(
                            Mesh,
                            {{{X0, Edge, LowerSide}, {X1, Edge, LowerSide}, {X1, Edge, UpperSide}, {X0, Edge, UpperSide}}},
                            {0.F, LowerSide > UpperSide ? 1.F : -1.F, 0.F},
                            CellAt(X, LowerSide > UpperSide ? Y - 1 : Y));
                }
            if (ThicknessMeters > 0.F || bSplit)
            {
                const int BottomWidth = bSplit ? SpecimenWidth : Columns;
                for (int Start = 0; Start < Columns; Start += BottomWidth)
                {
                    const float X0 = Shape.EdgeX(Start), X1 = Shape.EdgeX(Start + BottomWidth);
                    const float Y0 = Shape.EdgeY(0), Y1 = Shape.EdgeY(Rows);
                    AppendWall(Mesh,
                               {{{X0, Y0, Lower}, {X0, Y1, Lower}, {X1, Y1, Lower}, {X1, Y0, Lower}}},
                               {0.F, 0.F, -1.F},
                               CellAt(Start, 0));
                }
            }
            Mesh.SolidIndexCount = static_cast<std::uint32_t>(Mesh.Indices.size());
            AppendEffectBounds(Mesh, Field);
            if (Field.BedMetrics.w > 0.F)
            {
                const auto  First = Mesh.Indices.size();
                const float X = Shape.Width * 2.F, Y = Shape.Length * 2.5F;
                const float Z = Lower - 0.003F;
                AppendWall(Mesh, {{{-X, -Y, Z}, {X, -Y, Z}, {X, Y, Z}, {-X, Y, Z}}}, {0.F, 0.F, 1.F}, 0);
                Mesh.BackdropIndexCount = static_cast<std::uint32_t>(Mesh.Indices.size() - First);
            }
            if (Field.SectionWindow.y > 0.F)
            {
                const auto First = Mesh.Indices.size();
                if (Field.SectionWindow.w > 0.F)
                    AppendGlassVessel(Mesh, Field);
                else
                {
                    const float X = 0.5F * Shape.Width;
                    const float Y = Field.SectionWindow.x - 1.e-5F;
                    const float Top = Field.BedMetrics.x + Field.SectionWindow.y;
                    AppendWall(Mesh, {{{-X, Y, Lower}, {X, Y, Lower}, {X, Y, Top}, {-X, Y, Top}}}, {0.F, -1.F, 0.F}, 0);
                }
                Mesh.SectionIndexCount = static_cast<std::uint32_t>(Mesh.Indices.size() - First);
            }
            return Mesh;
        }

    #pragma endregion

} // namespace MDSS::SurfaceSimulation
