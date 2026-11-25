/**
 * @file SurfaceSimulationDomain.cpp
 * @brief 저장 격자를 만들고 지형에서 부피와 수위를 계산한다.
 */

#include "SurfaceState/Simulation/SurfaceSimulationDomain.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>

namespace MDSS::Liquid
{
    // ============================================================
    // 00. Sample Projection Helpers
    // ============================================================
    #pragma region 00_Sample_Projection_Helpers
        // 수위 계산에서 표본의 높이와 잠긴 깊이를 구한다.

        namespace
        {
            double SampleElevation(const FHeightfield &Shape, int X, int Y, FSurfaceTilt Pose)
            {
                return static_cast<double>(Shape.Height(X, Y)) * Pose.Cosine -
                       (Shape.EdgeX(X) + Shape.PitchX() * 0.5) * Pose.Sine;
            }

            double SampleHalfSpan(const FHeightfield &Shape, FSurfaceTilt Pose)
            {
                return 0.5 * Shape.PitchX() * std::abs(Pose.Sine);
            }

            double SampleMeanDepth(double CenterDepth, double HalfSpan, double WedgeFactor)
            {
                if (CenterDepth >= HalfSpan)
                    return CenterDepth;
                const double EndDepth = std::max(0.0, CenterDepth + HalfSpan);
                // 반만 잠긴 표본은 내리막 끝부터 수면의 교차점까지 삼각형을 이룬다.
                return EndDepth * EndDepth * WedgeFactor;
            }
        } // namespace
    #pragma endregion

    // ============================================================
    // 01. Grid Construction
    // ============================================================
    #pragma region 01_Grid_Construction
        // 형상 격자의 절단점에서 계산 셀과 공유 면을 만들고 샘플 소유 셀을 기록한다.

        namespace
        {
            void LabelCellBasins(FSurfaceSimulationDomain &Geometry, int CellIndex, EStorageRegionMode Storage)
            {
                const auto       Cell = Geometry.Cells[CellIndex];
                const auto      &Shape = Geometry.Shape;
                std::vector<int> Samples;
                for (int Y = Cell.FirstRow; Y < Cell.EndRow; ++Y)
                    for (int X = Cell.FirstColumn; X < Cell.EndColumn; ++X)
                        Samples.push_back(Y * Shape.Columns + X);

                const auto Unlabeled = static_cast<std::uint32_t>(MaxRegions);
                const auto Visiting = Unlabeled + 1;
                const auto FirstRegion = static_cast<std::uint32_t>(Geometry.Regions.size());
                auto      &Labels = Geometry.SampleRegions;
                auto       AddRegion = [&]()
                {
                    const auto Id = static_cast<std::uint32_t>(Geometry.Regions.size());
                    Geometry.Regions.push_back({CellIndex});
                    return Id;
                };

                if (Storage == EStorageRegionMode::SinglePerCell || Samples.size() == 1)
                {
                    const auto Region = AddRegion();
                    for (const int Sample : Samples)
                        Labels[Sample] = Region;
                }
                else
                {
                    std::ranges::stable_sort(Samples, {}, [&](int Sample) { return Shape.Heights[Sample]; });
                    std::vector<int> Plateau;
                    for (const int Sample : Samples)
                    {
                        if (Labels[Sample] != Unlabeled)
                            continue;
                        Plateau.clear();
                        Plateau.push_back(Sample);
                        Labels[Sample] = Visiting;
                        const float Height = Shape.Heights[Sample];
                        auto        Downstream = Unlabeled;
                        float       Lowest = Height;
                        for (std::size_t Index = 0; Index < Plateau.size(); ++Index)
                        {
                            const int X = Plateau[Index] % Shape.Columns, Y = Plateau[Index] / Shape.Columns;
                            const std::array<std::array<int, 2>, 4> Neighbors{
                                {{X - 1, Y}, {X + 1, Y}, {X, Y - 1}, {X, Y + 1}}};
                            for (const auto [NX, NY] : Neighbors)
                            {
                                if (NX < Cell.FirstColumn || NX >= Cell.EndColumn || NY < Cell.FirstRow ||
                                    NY >= Cell.EndRow)
                                    continue;
                                const int   Neighbor = NY * Shape.Columns + NX;
                                const float NextHeight = Shape.Heights[Neighbor];
                                if (NextHeight == Height && Labels[Neighbor] == Unlabeled)
                                {
                                    Labels[Neighbor] = Visiting;
                                    Plateau.push_back(Neighbor);
                                }
                                else if (NextHeight < Height &&
                                         (NextHeight < Lowest || (NextHeight == Lowest && Labels[Neighbor] < Downstream)))
                                {
                                    Lowest = NextHeight;
                                    Downstream = Labels[Neighbor];
                                }
                            }
                        }
                        // 같은 높이의 연결 영역을 한꺼번에 배정해 평평한 홈이 여러 개로 갈라지지 않게 한다.
                        const auto Region = Downstream == Unlabeled ? AddRegion() : Downstream;
                        for (const int Member : Plateau)
                            Labels[Member] = Region;
                    }
                }

                const FIndexRange Range{static_cast<int>(FirstRegion),
                                        static_cast<int>(Geometry.Regions.size() - FirstRegion)};
                Geometry.CellRegions.push_back(Range);
                // 전체 셀에 영역이 하나면 수위 계산에서 기존 사각형 순회를 쓴다.
                if (Range.Count == 1)
                {
                    Geometry.Regions[FirstRegion].FirstSample = -1;
                    Geometry.Regions[FirstRegion].SampleCount = static_cast<int>(Samples.size());
                    return;
                }
                std::vector<std::vector<int>> Members(Range.Count);
                for (const int Sample : Samples)
                    Members[Labels[Sample] - FirstRegion].push_back(Sample);
                for (int Offset = 0; Offset < Range.Count; ++Offset)
                {
                    auto &Region = Geometry.Regions[FirstRegion + Offset];
                    Region.FirstSample = static_cast<int>(Geometry.RegionSamples.size());
                    Region.SampleCount = static_cast<int>(Members[Offset].size());
                    Geometry.RegionSamples.insert(Geometry.RegionSamples.end(),
                                                  Members[Offset].begin(),
                                                  Members[Offset].end());
                }
            }

            void BuildRegionFaces(FSurfaceSimulationDomain &Geometry, bool bOpenRight, std::span<const int> ClosedColumns)
            {
                const int Columns = Geometry.Shape.Columns, Rows = Geometry.Shape.Rows;
                std::map<std::pair<int, int>, std::vector<int>> Contacts;
                for (int Y = 0; Y < Rows; ++Y)
                    for (int X = 0; X < Columns; ++X)
                    {
                        const int Sample = Y * Columns + X;
                        const int Region = static_cast<int>(Geometry.SampleRegions[Sample]);
                        auto      AddContact = [&](int OtherSample, int Axis)
                        {
                            const int Other = static_cast<int>(Geometry.SampleRegions[OtherSample]);
                            if (Region != Other)
                                Contacts[std::minmax(Region, Other)].push_back(2 * Sample + Axis);
                        };
                        if (X + 1 < Columns)
                        {
                            if (std::ranges::find(ClosedColumns, X + 1) == ClosedColumns.end())
                                AddContact(Sample + 1, 0);
                        }
                        else if (bOpenRight)
                            Contacts[{Region, -1}].push_back(2 * Sample);
                        if (Y + 1 < Rows)
                            AddContact(Sample + Columns, 1);
                    }

                std::vector<std::vector<int>> Adjacency(Geometry.Regions.size());
                for (const auto &[Pair, Members] : Contacts)
                {
                    const auto [Left, Right] = Pair;
                    const int Index = static_cast<int>(Geometry.Faces.size());
                    Geometry.Faces.push_back(
                        {Left, Right, static_cast<int>(Geometry.Contacts.size()), static_cast<int>(Members.size())});
                    Geometry.Contacts.insert(Geometry.Contacts.end(), Members.begin(), Members.end());
                    Adjacency[Left].push_back(Index);
                    if (Right >= 0)
                        Adjacency[Right].push_back(Index);
                }
                for (std::size_t Region = 0; Region < Adjacency.size(); ++Region)
                {
                    const auto &Faces = Adjacency[Region];
                    Geometry.Regions[Region].FirstFace = static_cast<int>(Geometry.RegionFaces.size());
                    Geometry.FaceCounts.push_back(static_cast<int>(Faces.size()));
                    Geometry.RegionFaces.insert(Geometry.RegionFaces.end(), Faces.begin(), Faces.end());
                }
            }
        } // namespace

        std::vector<int> Partition(int Samples, int Cells)
        {
            if (Cells < 1 || Cells > MaxCellsPerAxis || Samples < Cells || Samples > MaxSamplesPerAxis)
                throw std::invalid_argument("Simulation resolution must be positive and no finer than the geometry.");
            std::vector<int> Cuts;
            for (int Index = 0; Index <= Cells; ++Index)
                Cuts.push_back(Index * Samples / Cells);
            return Cuts;
        }

        FSurfaceSimulationDomain MakeGeometry(FHeightfield         Shape,
                                              std::span<const int> CutsX,
                                              std::span<const int> CutsY,
                                              bool                 bOpenRight,
                                              FLiquidMaterial      Liquid,
                                              EStorageRegionMode   Storage,
                                              std::span<const int> ClosedColumns)
        {
            auto Valid = [](std::span<const int> Cuts, int Samples)
            {
                return Cuts.size() >= 2 && Cuts.size() <= MaxCellsPerAxis + 1 && Samples <= MaxSamplesPerAxis &&
                       Cuts.front() == 0 && Cuts.back() == Samples &&
                       std::adjacent_find(Cuts.begin(), Cuts.end(), std::greater_equal<>()) == Cuts.end();
            };
            if (!Valid(CutsX, Shape.Columns) || !Valid(CutsY, Shape.Rows) ||
                Shape.Heights.size() != static_cast<std::size_t>(Shape.Rows * Shape.Columns) ||
                !std::isfinite(Shape.Width) || !std::isfinite(Shape.Length) || Shape.Width <= 0.F || Shape.Length <= 0.F ||
                !std::all_of(Shape.Heights.begin(), Shape.Heights.end(), [](float Z) { return std::isfinite(Z); }))
                throw std::invalid_argument("Invalid liquid geometry or cell partition.");

            for (const int Column : ClosedColumns)
                if (Column <= 0 || Column >= Shape.Columns || std::ranges::find(CutsX, Column) == CutsX.end())
                    throw std::invalid_argument("Closed column must be an interior simulation cell boundary.");

            if (!std::isfinite(Liquid.DensityKgM3) || Liquid.DensityKgM3 <= 0.F ||
                !std::isfinite(Liquid.DynamicViscosityPaS) || Liquid.DynamicViscosityPaS <= 0.F ||
                (Liquid.LocalRule != ELocalMaterialRule::None && Liquid.LocalRule != ELocalMaterialRule::Water))
                throw std::invalid_argument("Invalid liquid material properties.");
            FSurfaceSimulationDomain Geometry;
            Geometry.Liquid = Liquid;
            Geometry.Shape = std::move(Shape);
            Geometry.SampleCells.resize(Geometry.Shape.Heights.size());
            Geometry.SampleRegions.resize(Geometry.Shape.Heights.size(), MaxRegions);
            const int Columns = static_cast<int>(CutsX.size()) - 1;
            const int Rows = static_cast<int>(CutsY.size()) - 1;
            for (int Y = 0; Y < Rows; ++Y)
                for (int X = 0; X < Columns; ++X)
                {
                    const FSimulationCell Cell{CutsX[X], CutsX[X + 1], CutsY[Y], CutsY[Y + 1]};
                    const auto            Index = static_cast<std::uint32_t>(Geometry.Cells.size());
                    Geometry.Cells.push_back(Cell);
                    for (int SampleY = Cell.FirstRow; SampleY < Cell.EndRow; ++SampleY)
                        for (int SampleX = Cell.FirstColumn; SampleX < Cell.EndColumn; ++SampleX)
                            Geometry.SampleCells[SampleY * Geometry.Shape.Columns + SampleX] = Index;
                }

            for (int Cell = 0; Cell < static_cast<int>(Geometry.Cells.size()); ++Cell)
                LabelCellBasins(Geometry, Cell, Storage);
            BuildRegionFaces(Geometry, bOpenRight, ClosedColumns);
            Geometry.Materials.resize(Geometry.Cells.size());
            Geometry.InitialIceKgM2.resize(Geometry.Cells.size());
            const auto Inlet = Geometry.Cells[(Rows / 2) * Columns];
            Geometry.SourceSample = ((Inlet.FirstRow + Inlet.EndRow) / 2) * Geometry.Shape.Columns + Inlet.FirstColumn;
            return Geometry;
        }

        void SetScheduledInlet(FSurfaceSimulationDomain &Geometry, const FInletFootprint &Footprint)
        {
            for (std::size_t Axis = 0; Axis < Footprint.SizeM.size(); ++Axis)
                if (!std::isfinite(Footprint.CenterM[Axis]) || !std::isfinite(Footprint.SizeM[Axis]) ||
                    Footprint.SizeM[Axis] <= 0.F)
                    throw std::invalid_argument("Scheduled inlet must have a finite center and positive size.");

            const double X = Footprint.CenterM[0], Y = Footprint.CenterM[1];
            const double HalfWidth = 0.5 * Footprint.SizeM[0], HalfLength = 0.5 * Footprint.SizeM[1];
            const auto   Overlap = [](double AMin, double AMax, double BMin, double BMax)
            { return std::max(0.0, std::min(AMax, BMax) - std::max(AMin, BMin)); };

            const auto         &Shape = Geometry.Shape;
            std::vector<double> Areas(Geometry.Regions.size());
            // 셀 경계 대신 형상 표본의 투영 면적을 적분해야 부분 표본과 셀 내부의 홈 분리를 반영할 수 있다.
            for (int Row = 0; Row < Shape.Rows; ++Row)
            {
                const double OverlapY = Overlap(Y - HalfLength, Y + HalfLength, Shape.EdgeY(Row), Shape.EdgeY(Row + 1));
                for (int Column = 0; Column < Shape.Columns; ++Column)
                {
                    const double OverlapX =
                        Overlap(X - HalfWidth, X + HalfWidth, Shape.EdgeX(Column), Shape.EdgeX(Column + 1));
                    Areas[Geometry.SampleRegions[Row * Shape.Columns + Column]] += OverlapX * OverlapY;
                }
            }

            const double Total = std::accumulate(Areas.begin(), Areas.end(), 0.0);
            if (Total <= 0.0)
                throw std::invalid_argument("Scheduled inlet must overlap the surface chart.");
            Geometry.ScheduledInletWeights.resize(Areas.size());
            std::ranges::transform(Areas,
                                   Geometry.ScheduledInletWeights.begin(),
                                   [Total](double Area) { return static_cast<float>(Area / Total); });
        }

    #pragma endregion

    // ============================================================
    // 02. Cell Capacity and Liquid Level
    // ============================================================
    #pragma region 02_Cell_Capacity_and_Liquid_Level
        // 높이를 중력 방향에 맞춰 변환하고 잠긴 면적과 질량으로 수위를 계산한다.

        FSurfaceTilt MakePose(float Radians)
        {
            return {std::sin(Radians), std::cos(Radians)};
        }

        double CellArea(const FSurfaceSimulationDomain &Geometry, int Cell)
        {
            const auto Bounds = Geometry.Cells[Cell];
            return static_cast<double>(Geometry.Shape.PitchX()) * Geometry.Shape.PitchY() *
                   (Bounds.EndColumn - Bounds.FirstColumn) * (Bounds.EndRow - Bounds.FirstRow);
        }

        double VolumeAtLevel(const FSurfaceSimulationDomain &Geometry, int Cell, FSurfaceTilt Pose, double Level)
        {
            const auto  &Shape = Geometry.Shape;
            const auto   Range = Geometry.Cells[Cell];
            const double Area = static_cast<double>(Shape.PitchX()) * Shape.PitchY() * Pose.Cosine;
            const double HalfSpan = SampleHalfSpan(Shape, Pose);
            const double WedgeFactor = HalfSpan > 0.0 ? 0.25 / HalfSpan : 0.0;
            double       Depth = 0.0;
            for (int Y = Range.FirstRow; Y < Range.EndRow; ++Y)
                for (int X = Range.FirstColumn; X < Range.EndColumn; ++X)
                    Depth += SampleMeanDepth(Level - SampleElevation(Shape, X, Y, Pose), HalfSpan, WedgeFactor);
            return Area * Depth;
        }

        double SillElevation(const FSurfaceSimulationDomain &Geometry, FTransportFace Face, int Contact, FSurfaceTilt Pose)
        {
            const auto &Shape = Geometry.Shape;
            const bool  bX = (Contact % 2) == 0;
            const int   Sample = Contact / 2;
            const int   X = Sample % Shape.Columns, Y = Sample / Shape.Columns;
            const float LeftHeight = Shape.Height(X, Y);
            const float Sill =
                Face.Right < 0 ? LeftHeight
                               : std::max(LeftHeight, Shape.Height(X + static_cast<int>(bX), Y + static_cast<int>(!bX)));
            const double Coordinate = bX ? Shape.EdgeX(X + 1) : Shape.EdgeX(X) + Shape.PitchX() * 0.5;
            const double Base = Sill * Pose.Cosine - Coordinate * Pose.Sine;
            if (Geometry.RegionBedLevels.empty())
                return Base;
            const double Left = Geometry.RegionBedLevels[Face.Left];
            return std::max({Base, Left, Face.Right < 0 ? Left : Geometry.RegionBedLevels[Face.Right]});
        }

        double RegionWetArea(const FSurfaceSimulationDomain &Geometry,
                             int                             Region,
                             FSurfaceTilt                    Pose,
                             double                          Level,
                             double                          DepthBand)
        {
            const auto  &Shape = Geometry.Shape;
            const auto  &Bounds = Geometry.Regions[Region];
            const double HalfSpan = SampleHalfSpan(Shape, Pose);
            const double Wedge = HalfSpan > 0.0 ? 0.25 / HalfSpan : 0.0;
            const double Floor = Geometry.RegionBedLevels.empty() ? -std::numeric_limits<double>::infinity()
                                                                  : Geometry.RegionBedLevels[Region];
            if (Level <= Floor)
                return 0.0;
            const auto FractionAt = [&](int X, int Y)
            {
                const double Depth = Level - SampleElevation(Shape, X, Y, Pose);
                if (DepthBand > 0.0)
                {
                    // clamp(depth/band,0,1)의 적분은 두 잠긴 부피의 차에 해당한다.
                    if (Depth >= HalfSpan + DepthBand && Level - Floor >= DepthBand)
                        return 1.0;
                    return std::clamp((SampleMeanDepth(Depth, HalfSpan, Wedge) -
                                       SampleMeanDepth(Depth - std::min(DepthBand, Level - Floor), HalfSpan, Wedge)) /
                                          DepthBand,
                                      0.0,
                                      1.0);
                }
                // 부피 적분과 같은 경사 표본에서 물에 잠긴 가로 길이 비율을 셈한다.
                return HalfSpan > 0.0 ? std::clamp((Depth + HalfSpan) / (2.0 * HalfSpan), 0.0, 1.0)
                                      : static_cast<double>(Depth > 0.0);
            };

            double AreaInSamples = 0.0;
            if (Bounds.FirstSample < 0)
            {
                const auto Cell = Geometry.Cells[Bounds.Cell];
                for (int Y = Cell.FirstRow; Y < Cell.EndRow; ++Y)
                    for (int X = Cell.FirstColumn; X < Cell.EndColumn; ++X)
                        AreaInSamples += FractionAt(X, Y);
            }
            else
            {
                for (const int Sample : FIndexRange{Bounds.FirstSample, Bounds.SampleCount}.In(Geometry.RegionSamples))
                    AreaInSamples += FractionAt(Sample % Shape.Columns, Sample / Shape.Columns);
            }
            return AreaInSamples * Shape.PitchX() * Shape.PitchY();
        }

        double RegionVolume(const FSurfaceSimulationDomain &Geometry, int Region, FSurfaceTilt Pose, double Level)
        {
            const auto  &Bounds = Geometry.Regions[Region];
            const double Floor = Geometry.RegionBedLevels.empty() ? -std::numeric_limits<double>::infinity()
                                                                  : Geometry.RegionBedLevels[Region];
            if (Level <= Floor)
                return 0.0;
            if (Bounds.FirstSample < 0)
                return VolumeAtLevel(Geometry, Bounds.Cell, Pose, Level) -
                       (std::isfinite(Floor) ? VolumeAtLevel(Geometry, Bounds.Cell, Pose, Floor) : 0.0);
            const auto  &Shape = Geometry.Shape;
            const double HalfSpan = SampleHalfSpan(Shape, Pose);
            const double WedgeFactor = HalfSpan > 0.0 ? 0.25 / HalfSpan : 0.0;
            double       Depth = 0.0;
            for (const int Sample : FIndexRange{Bounds.FirstSample, Bounds.SampleCount}.In(Geometry.RegionSamples))
            {
                const double Center = SampleElevation(Shape, Sample % Shape.Columns, Sample / Shape.Columns, Pose);
                // 아래층이 막은 부피를 빼면 기울어진 표본도 부분 잠김을 보존한다.
                Depth += SampleMeanDepth(Level - Center, HalfSpan, WedgeFactor) -
                         (std::isfinite(Floor) ? SampleMeanDepth(Floor - Center, HalfSpan, WedgeFactor) : 0.0);
            }
            return static_cast<double>(Shape.PitchX()) * Shape.PitchY() * Pose.Cosine * Depth;
        }

        namespace
        {
            float SolveTiltedLevel(std::span<const double> Centers, double DepthSum, double HalfSpan)
            {
                const auto   Bounds = std::ranges::minmax_element(Centers);
                const double MeanDepth = DepthSum / static_cast<double>(Centers.size());
                if (*Bounds.min == *Bounds.max)
                {
                    // 한 표본 또는 같은 중심 높이의 표본들은 삼각형 부피의 역함수를 직접 쓴다.
                    const double Offset =
                        MeanDepth < HalfSpan ? std::sqrt(4.0 * HalfSpan * MeanDepth) - HalfSpan : MeanDepth;
                    return static_cast<float>(*Bounds.min + Offset);
                }

                const double WedgeFactor = 0.25 / HalfSpan;
                double       Low = *Bounds.min - HalfSpan;
                double       High = *Bounds.max + HalfSpan + MeanDepth;
                for (int Iteration = 0; Iteration < 27; ++Iteration)
                {
                    const double Level = std::midpoint(Low, High);
                    double       Depth = 0.0;
                    for (const double Center : Centers)
                        Depth += SampleMeanDepth(Level - Center, HalfSpan, WedgeFactor);
                    if (Depth < DepthSum)
                        Low = Level;
                    else
                        High = Level;
                }
                return static_cast<float>(std::midpoint(Low, High));
            }

            float
            SolveLevel(const FSurfaceSimulationDomain &Geometry, FSurfaceTilt Pose, double Mass, std::span<double> Active)
            {
                const auto  &Shape = Geometry.Shape;
                const double HalfSpan = SampleHalfSpan(Shape, Pose);
                if (Mass <= 0.0)
                    return static_cast<float>(*std::ranges::min_element(Active) - HalfSpan);
                const double DepthSum =
                    Mass / (Geometry.Liquid.DensityKgM3 * Shape.PitchX() * Shape.PitchY() * Pose.Cosine);
                if (HalfSpan > 0.0)
                    return SolveTiltedLevel(Active, DepthSum, HalfSpan);

                // 수평 자세에서는 표본마다 통째로 잠기므로 기존의 구간별 선형 역함수를 쓴다.
                std::ranges::sort(Active);
                double HeightSum = Active[0];
                for (std::size_t Index = 1; Index < Active.size(); ++Index)
                {
                    const double Level = (DepthSum + HeightSum) / static_cast<double>(Index);
                    if (Level <= Active[Index])
                        return static_cast<float>(Level);
                    HeightSum += Active[Index];
                }
                return static_cast<float>((DepthSum + HeightSum) / static_cast<double>(Active.size()));
            }

            std::span<double> LevelScratch(std::size_t Count)
            {
                // CPU 기준 스텝은 셀마다 호출하므로 같은 스레드의 임시 메모리를 재사용한다.
                thread_local std::vector<double> Heights;
                if (Heights.size() < Count)
                    Heights.resize(Count);
                return std::span(Heights).first(Count);
            }
        } // namespace

        float RestoreRegionLevel(const FSurfaceSimulationDomain &Geometry, int Region, FSurfaceTilt Pose, double Mass)
        {
            const auto  &Bounds = Geometry.Regions[Region];
            const double Floor = Geometry.RegionBedLevels.empty() ? -std::numeric_limits<double>::infinity()
                                                                  : Geometry.RegionBedLevels[Region];
            if (Bounds.FirstSample < 0)
            {
                const double Occupied =
                    Mass > 0.0 && std::isfinite(Floor) ? VolumeAtLevel(Geometry, Bounds.Cell, Pose, Floor) : 0.0;
                return std::max(static_cast<float>(Floor),
                                RestoreLevel(Geometry, Bounds.Cell, Pose, Mass + Occupied * Geometry.Liquid.DensityKgM3));
            }
            const auto  Heights = LevelScratch(Bounds.SampleCount);
            const auto &Shape = Geometry.Shape;
            std::size_t Count = 0;
            for (const int Sample : FIndexRange{Bounds.FirstSample, Bounds.SampleCount}.In(Geometry.RegionSamples))
                Heights[Count++] = SampleElevation(Shape, Sample % Shape.Columns, Sample / Shape.Columns, Pose);
            if (Mass > 0.0 && std::isfinite(Floor))
            {
                const double HalfSpan = SampleHalfSpan(Shape, Pose);
                const double Wedge = HalfSpan > 0.0 ? 0.25 / HalfSpan : 0.0;
                double       Occupied = 0.0;
                for (const double Center : Heights)
                    Occupied += SampleMeanDepth(Floor - Center, HalfSpan, Wedge);
                Mass += Occupied * Shape.PitchX() * Shape.PitchY() * Pose.Cosine * Geometry.Liquid.DensityKgM3;
            }
            return std::max(static_cast<float>(Floor), SolveLevel(Geometry, Pose, Mass, Heights));
        }
        float RestoreLevel(const FSurfaceSimulationDomain &Geometry, int Cell, FSurfaceTilt Pose, double Mass)
        {
            const auto  Range = Geometry.Cells[Cell];
            const auto  Samples = LevelScratch((Range.EndColumn - Range.FirstColumn) * (Range.EndRow - Range.FirstRow));
            std::size_t Count = 0;
            for (int Y = Range.FirstRow; Y < Range.EndRow; ++Y)
                for (int X = Range.FirstColumn; X < Range.EndColumn; ++X)
                    Samples[Count++] = SampleElevation(Geometry.Shape, X, Y, Pose);
            return SolveLevel(Geometry, Pose, Mass, Samples);
        }

    #pragma endregion

} // namespace MDSS::Liquid
