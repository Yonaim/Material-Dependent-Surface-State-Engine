/**
 * @file MesoGeometryBuilder.cpp
 * @brief 샘플링한 노멀 맵을 적분해 Meso 높이와 미분 기하 정보를 만든다.
 */

#include "SurfaceStateSystem/Geometry/MesoGeometryBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

namespace MDSS
{
    namespace
    {
        constexpr float         NormalDotMinimum = 0.05F;
        constexpr float         GeometryEpsilon = 1.0e-8F;
        constexpr std::uint32_t MaxCGIterations = 128;
        constexpr float         RelativeResidualSquared = 1.0e-8F;

        bool IsFinite(const glm::vec3& V)
        {
            return std::isfinite(V.x) && std::isfinite(V.y) && std::isfinite(V.z);
        }

        bool IsActive(const TSurfaceTexelGeometry& Texel)
        {
            if (!Texel.IsValid() || !Texel.HasTransferNormal || !IsFinite(Texel.TransferNormal) ||
                !IsFinite(Texel.Normal))
            {
                return false;
            }
            const float NormalLength2 = glm::dot(Texel.TransferNormal, Texel.TransferNormal);
            return std::isfinite(NormalLength2) && NormalLength2 > GeometryEpsilon &&
                   glm::dot(glm::normalize(Texel.TransferNormal), glm::normalize(Texel.Normal)) > NormalDotMinimum;
        }

        // 노멀 맵 normal이 요구하는 Source→Target 높이 차이를 mesh-local 길이 단위로 계산한다.
        float EdgeHeightDelta(const TSurfaceTexelGeometry& Source, const TSurfaceTexelGeometry& Target)
        {
            const glm::vec3 Delta = Target.Position - Source.Position;
            const glm::vec3 SourceNormal = glm::normalize(Source.Normal);
            const glm::vec3 TargetNormal = glm::normalize(Target.Normal);
            const glm::vec3 SourceTangentDelta = Delta - SourceNormal * glm::dot(Delta, SourceNormal);
            const glm::vec3 TargetTangentDelta = Delta - TargetNormal * glm::dot(Delta, TargetNormal);
            const float     SourceLength = glm::length(SourceTangentDelta);
            const float     TargetLength = glm::length(TargetTangentDelta);
            const float     SourceNormalComponent = glm::dot(Source.TransferNormal, SourceNormal);
            const float     TargetNormalComponent = glm::dot(Target.TransferNormal, TargetNormal);
            if (!std::isfinite(SourceLength) || !std::isfinite(TargetLength) || SourceLength <= GeometryEpsilon ||
                TargetLength <= GeometryEpsilon || SourceNormalComponent <= NormalDotMinimum ||
                TargetNormalComponent <= NormalDotMinimum)
            {
                return 0.0F;
            }

            const glm::vec3 SourceDirection = SourceTangentDelta / SourceLength;
            const glm::vec3 TargetDirection = TargetTangentDelta / TargetLength;
            const float     SourceSlope = -glm::dot(Source.TransferNormal, SourceDirection) / SourceNormalComponent;
            const float     TargetSlope = -glm::dot(Target.TransferNormal, TargetDirection) / TargetNormalComponent;
            const float     Result = 0.5F * (SourceSlope * SourceLength + TargetSlope * TargetLength);
            return std::isfinite(Result) ? Result : 0.0F;
        }

        bool Solve5x5(std::array<std::array<double, 6>, 5>& Matrix, std::array<double, 5>& Out)
        {
            // 국소 이차 곡면 fit에서 만든 5×5 선형 방정식을 부분 피벗 소거법으로 푼다.
            for (std::size_t Column = 0; Column < 5; ++Column)
            {
                std::size_t Pivot = Column;
                for (std::size_t Row = Column + 1; Row < 5; ++Row)
                {
                    if (std::abs(Matrix[Row][Column]) > std::abs(Matrix[Pivot][Column]))
                        Pivot = Row;
                }
                if (std::abs(Matrix[Pivot][Column]) < 1.0e-12)
                    return false;
                std::swap(Matrix[Column], Matrix[Pivot]);
                const double Divisor = Matrix[Column][Column];
                for (std::size_t J = Column; J < 6; ++J)
                    Matrix[Column][J] /= Divisor;
                for (std::size_t Row = 0; Row < 5; ++Row)
                {
                    if (Row == Column)
                        continue;
                    const double Factor = Matrix[Row][Column];
                    for (std::size_t J = Column; J < 6; ++J)
                        Matrix[Row][J] -= Factor * Matrix[Column][J];
                }
            }
            for (std::size_t I = 0; I < 5; ++I)
                Out[I] = Matrix[I][5];
            return true;
        }

        void BuildDifferentialGeometry(std::vector<TSurfaceTexelGeometry>& Texels,
                                       const std::vector<std::uint8_t>&    Active)
        {
            // 이웃 높이를 국소 접평면 좌표의 이차식에 맞춰 normal과 두 곡률을 구한다.
            for (std::size_t Index = 0; Index < Texels.size(); ++Index)
            {
                TSurfaceTexelGeometry& Texel = Texels[Index];
                if (!Active[Index])
                    continue;
                const glm::vec3 N = glm::normalize(Texel.Normal);
                const glm::vec3 Axis = std::abs(N.z) < 0.85F ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
                const glm::vec3 T = glm::normalize(glm::cross(Axis, N));
                const glm::vec3 B = glm::normalize(glm::cross(N, T));

                std::array<std::array<double, 6>, 5> NormalEquations{};
                float                                MeanSpacing = 0.0F;
                std::size_t                          SampleCount = 0;
                for (const TLocalTexelIndex NeighborIndex : Texel.NeighborIndices)
                {
                    if (NeighborIndex >= Texels.size() || !Active[NeighborIndex])
                        continue;
                    const glm::vec3 Edge = Texels[NeighborIndex].Position - Texel.Position;
                    const float     X = glm::dot(Edge, T);
                    const float     Y = glm::dot(Edge, B);
                    MeanSpacing += std::sqrt(X * X + Y * Y);
                    ++SampleCount;
                }

                std::array<double, 5> Fit{};
                if (SampleCount < 5)
                {
                    Texel.MesoNormal = glm::normalize(Texel.TransferNormal);
                    Texel.HasMesoNormal = true;
                    continue;
                }
                const double Scale = std::max(static_cast<double>(MeanSpacing / static_cast<float>(SampleCount)),
                                              static_cast<double>(GeometryEpsilon));
                for (const TLocalTexelIndex NeighborIndex : Texel.NeighborIndices)
                {
                    if (NeighborIndex >= Texels.size() || !Active[NeighborIndex])
                        continue;
                    const TSurfaceTexelGeometry& Neighbor = Texels[NeighborIndex];
                    const glm::vec3              Edge = Neighbor.Position - Texel.Position;
                    const double                 X = static_cast<double>(glm::dot(Edge, T)) / Scale;
                    const double                 Y = static_cast<double>(glm::dot(Edge, B)) / Scale;
                    const double                 Z =
                        static_cast<double>(Neighbor.Geometry.MesoVirtualHeight - Texel.Geometry.MesoVirtualHeight) /
                        Scale;
                    const double Features[5] = {X, Y, X * X, X * Y, Y * Y};
                    const double Weight = 1.0 / std::max(X * X + Y * Y, 1.0e-10);
                    for (std::size_t Row = 0; Row < 5; ++Row)
                    {
                        NormalEquations[Row][5] += Weight * Features[Row] * Z;
                        for (std::size_t Column = 0; Column < 5; ++Column)
                            NormalEquations[Row][Column] += Weight * Features[Row] * Features[Column];
                    }
                }
                if (!Solve5x5(NormalEquations, Fit))
                {
                    Texel.MesoNormal = glm::normalize(Texel.TransferNormal);
                    Texel.HasMesoNormal = true;
                    continue;
                }
                const float Fx = static_cast<float>(Fit[0]);
                const float Fy = static_cast<float>(Fit[1]);
                const float Fxx = static_cast<float>(2.0 * Fit[2] / Scale);
                const float Fxy = static_cast<float>(Fit[3] / Scale);
                const float Fyy = static_cast<float>(2.0 * Fit[4] / Scale);
                const float GradientScale = 1.0F + Fx * Fx + Fy * Fy;
                const float MeanCurvature = ((1.0F + Fy * Fy) * Fxx - 2.0F * Fx * Fy * Fxy + (1.0F + Fx * Fx) * Fyy) /
                                            (2.0F * std::pow(GradientScale, 1.5F));
                const float GaussianCurvature = (Fxx * Fyy - Fxy * Fxy) / (GradientScale * GradientScale);
                if (std::isfinite(MeanCurvature) && std::isfinite(GaussianCurvature))
                {
                    Texel.Geometry.MesoMeanCurvature = MeanCurvature;
                    Texel.Geometry.MesoGaussianCurvature = GaussianCurvature;
                    Texel.MesoNormal = glm::normalize(N - Fx * T - Fy * B);
                    Texel.HasMesoNormal = IsFinite(Texel.MesoNormal);
                    const float Spacing = MeanSpacing / static_cast<float>(SampleCount);
                    // 위쪽을 향한 normal 기준에서 양의 signed mean curvature는 오목한 그릇 형태를 뜻한다.
                    Texel.Geometry.ConcavityWeight = std::clamp(MeanCurvature * Spacing, 0.0F, 1.0F);
                }
            }
        }
    } // 내부 네임스페이스

    TMesoGeometryBuildReport BuildMesoGeometry(TSharedSurfaceGeometryData& Geometry)
    {
        TMesoGeometryBuildReport            Report;
        std::vector<TSurfaceTexelGeometry>& Texels = Geometry.GetTexels();
        const std::size_t                   Count = Texels.size();
        if (Count == 0)
            return Report;

        std::vector<std::uint8_t> Active(Count, 0U);
        for (std::size_t I = 0; I < Count; ++I)
        {
            Active[I] = IsActive(Texels[I]) ? 1U : 0U;
            Report.ActiveTexelCount += Active[I];
        }

        // 각 연결 성분의 기준 texel을 고정해 높이의 임의 상수를 제거한다.
        // 서로 끊긴 UV chart끼리는 높이 기준을 공유하지 않는다.
        std::vector<std::uint8_t> Pinned(Count, 0U);
        std::vector<std::uint8_t> Visited(Count, 0U);
        std::vector<std::size_t>  ComponentIds(Count, std::numeric_limits<std::size_t>::max());
        std::queue<std::size_t>   Queue;
        for (std::size_t Root = 0; Root < Count; ++Root)
        {
            if (!Active[Root] || Visited[Root])
                continue;
            ++Report.ComponentCount;
            const std::size_t ComponentId = Report.ComponentCount - 1U;
            Pinned[Root] = 1U;
            Visited[Root] = 1U;
            ComponentIds[Root] = ComponentId;
            Queue.push(Root);
            while (!Queue.empty())
            {
                const std::size_t Current = Queue.front();
                Queue.pop();
                for (const TLocalTexelIndex Neighbor : Texels[Current].NeighborIndices)
                {
                    if (Neighbor >= Count || !Active[Neighbor] || Visited[Neighbor])
                        continue;
                    Visited[Neighbor] = 1U;
                    ComponentIds[Neighbor] = ComponentId;
                    Queue.push(Neighbor);
                }
            }
        }

        // 이웃 높이 차이 제약을 최소제곱 문제로 바꾼 Laplacian의 우변과 PCG 작업 벡터다.
        std::vector<float> RHS(Count, 0.0F), X(Count, 0.0F), R(Count, 0.0F), Z(Count, 0.0F), P(Count, 0.0F),
            AP(Count, 0.0F);
        std::vector<float> Degree(Count, 0.0F);
        for (std::size_t I = 0; I < Count; ++I)
        {
            if (!Active[I])
                continue;
            for (const TLocalTexelIndex Neighbor : Texels[I].NeighborIndices)
            {
                if (Neighbor >= Count || Neighbor == I || !Active[Neighbor])
                    continue;
                Degree[I] += 1.0F;
                RHS[I] -= EdgeHeightDelta(Texels[I], Texels[Neighbor]);
            }
            if (Pinned[I])
            {
                Degree[I] = 1.0F;
                RHS[I] = 0.0F;
            }
        }
        R = RHS;
        for (std::size_t I = 0; I < Count; ++I)
        {
            if (Active[I] && !Pinned[I] && Degree[I] > 0.0F)
                Z[I] = R[I] / Degree[I];
        }
        P = Z;
        double RZ = 0.0;
        double RHSNorm2 = 0.0;
        for (std::size_t I = 0; I < Count; ++I)
        {
            RZ += static_cast<double>(R[I]) * Z[I];
            RHSNorm2 += static_cast<double>(RHS[I]) * RHS[I];
        }

        // Jacobi 전처리 켤레기울기법(PCG)으로 전체 texel 높이를 반복 계산한다.
        // Degree는 Jacobi 전처리의 대각 성분이고 P/AP/R/Z는 탐색 방향·행렬곱·잔차·전처리 잔차다.
        for (std::uint32_t Iteration = 0; Iteration < MaxCGIterations && RZ > 0.0; ++Iteration)
        {
            Report.IterationCount = Iteration + 1U;
            double PAP = 0.0;
            for (std::size_t I = 0; I < Count; ++I)
            {
                if (!Active[I])
                {
                    AP[I] = 0.0F;
                    continue;
                }
                if (Pinned[I])
                {
                    AP[I] = P[I];
                    PAP += static_cast<double>(P[I]) * P[I];
                    continue;
                }
                AP[I] = Degree[I] * P[I];
                for (const TLocalTexelIndex Neighbor : Texels[I].NeighborIndices)
                {
                    if (Neighbor < Count && Neighbor != I && Active[Neighbor] && !Pinned[Neighbor])
                        AP[I] -= P[Neighbor];
                }
                PAP += static_cast<double>(P[I]) * AP[I];
            }
            if (!(PAP > 1.0e-30) || !std::isfinite(PAP))
                break;
            const float Alpha = static_cast<float>(RZ / PAP);
            double      NextRZ = 0.0;
            for (std::size_t I = 0; I < Count; ++I)
            {
                X[I] += Alpha * P[I];
                R[I] -= Alpha * AP[I];
                Z[I] = Active[I] && !Pinned[I] && Degree[I] > 0.0F ? R[I] / Degree[I] : R[I];
                NextRZ += static_cast<double>(R[I]) * Z[I];
            }
            if (RHSNorm2 <= 1.0e-30 || NextRZ <= RHSNorm2 * RelativeResidualSquared)
                break;
            const float Beta = static_cast<float>(NextRZ / RZ);
            for (std::size_t I = 0; I < Count; ++I)
                P[I] = Z[I] + Beta * P[I];
            RZ = NextRZ;
        }

        for (std::size_t I = 0; I < Count; ++I)
        {
            Texels[I].Geometry.MesoVirtualHeight = Active[I] && std::isfinite(X[I]) ? X[I] : 0.0F;
        }

        // 기준점을 고정해 푼 뒤 component 평균을 0으로 옮겨 cavity 높이 기준을 일관되게 둔다.
        std::vector<double>      ComponentHeightSums(Report.ComponentCount, 0.0);
        std::vector<std::size_t> ComponentSizes(Report.ComponentCount, 0U);
        for (std::size_t I = 0; I < Count; ++I)
        {
            if (!Active[I])
                continue;
            ComponentHeightSums[ComponentIds[I]] += Texels[I].Geometry.MesoVirtualHeight;
            ++ComponentSizes[ComponentIds[I]];
        }
        for (std::size_t I = 0; I < Count; ++I)
        {
            if (!Active[I])
                continue;
            const std::size_t Component = ComponentIds[I];
            if (ComponentSizes[Component] > 0U)
            {
                Texels[I].Geometry.MesoVirtualHeight -=
                    static_cast<float>(ComponentHeightSums[Component] / static_cast<double>(ComponentSizes[Component]));
            }
        }

        // 적분 높이와 각 edge가 요구한 차이의 상대 RMS 잔차를 기록한다.
        double      EdgeErrorSquared = 0.0;
        double      EdgeTargetSquared = 0.0;
        std::size_t DirectedEdgeCount = 0;
        for (std::size_t I = 0; I < Count; ++I)
        {
            if (!Active[I])
                continue;
            for (const TLocalTexelIndex Neighbor : Texels[I].NeighborIndices)
            {
                if (Neighbor >= Count || Neighbor == I || !Active[Neighbor])
                    continue;
                const float Target = EdgeHeightDelta(Texels[I], Texels[Neighbor]);
                const float Error =
                    Texels[Neighbor].Geometry.MesoVirtualHeight - Texels[I].Geometry.MesoVirtualHeight - Target;
                EdgeErrorSquared += static_cast<double>(Error) * Error;
                EdgeTargetSquared += static_cast<double>(Target) * Target;
                ++DirectedEdgeCount;
            }
        }
        if (DirectedEdgeCount > 0)
        {
            Report.RelativeEdgeResidual =
                static_cast<float>(std::sqrt(EdgeErrorSquared / std::max(EdgeTargetSquared, 1.0e-20)));
        }
        BuildDifferentialGeometry(Texels, Active);
        return Report;
    }
} // MDSS 네임스페이스
