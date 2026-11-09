/**
 * @file WorldReferenceGeometry.h
 * @brief Renderer 내부에서 사용하는 grid·축·이동/회전 gizmo의 정점 생성.
 */
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
namespace MDSS::Rendering
{
    namespace
    {
// ============================================================
// 00. World Reference and Gizmo Geometry
// ============================================================
#pragma region 00_World_Reference_and_Gizmo_Geometry

        struct FGizmoVertex
        {
            glm::vec3 Position;
            glm::vec4 Color;
            float     EdgeCoordinate = -2.0F;
        };

        struct FGizmoPushConstants
        {
            glm::mat4    ViewProjectionModel{1.0F};
            std::int32_t HighlightAxis = -1;
            std::int32_t Padding0 = 0;
            std::int32_t Padding1 = 0;
            std::int32_t Padding2 = 0;
        };

        constexpr std::array<glm::vec4, 3> WorldAxisColors = {glm::vec4(1.0F, 0.10F, 0.10F, 1.0F),
                                                              glm::vec4(0.10F, 0.95F, 0.20F, 1.0F),
                                                              glm::vec4(0.12F, 0.35F, 1.0F, 1.0F)};

        std::vector<FGizmoVertex> BuildWorldReferenceVertices(std::uint32_t& GridVertexCount,
                                                              std::uint32_t& AxisVertexCount)
        {
            std::vector<FGizmoVertex> Vertices;
            constexpr float           GridExtent = 50.0F;
            constexpr float           GridHalfWidth = 0.008F;
            constexpr float           GridEdgeCoordinate = 1.25F;
            constexpr float           AxisHalfWidth = 0.035F;
            const glm::vec4           GridColor(1.0F);
            auto                      AddQuad = [&](glm::vec3            A,
                               glm::vec3            B,
                               glm::vec3            C,
                               glm::vec3            D,
                               glm::vec4            Color,
                               std::array<float, 4> EdgeCoordinates)
            {
                Vertices.insert(Vertices.end(),
                                {{A, Color, EdgeCoordinates[0]},
                                 {B, Color, EdgeCoordinates[1]},
                                 {C, Color, EdgeCoordinates[2]},
                                 {A, Color, EdgeCoordinates[0]},
                                 {C, Color, EdgeCoordinates[2]},
                                 {D, Color, EdgeCoordinates[3]}});
            };

            for (int I = -static_cast<int>(GridExtent); I <= static_cast<int>(GridExtent); ++I)
            {
                const float Coordinate = static_cast<float>(I);
                const float ExpandedHalfWidth = GridHalfWidth * GridEdgeCoordinate;
                AddQuad({Coordinate - ExpandedHalfWidth, -GridExtent, 0.0F},
                        {Coordinate + ExpandedHalfWidth, -GridExtent, 0.0F},
                        {Coordinate + ExpandedHalfWidth, GridExtent, 0.0F},
                        {Coordinate - ExpandedHalfWidth, GridExtent, 0.0F},
                        GridColor,
                        {-GridEdgeCoordinate, GridEdgeCoordinate, GridEdgeCoordinate, -GridEdgeCoordinate});
                AddQuad({-GridExtent, Coordinate - ExpandedHalfWidth, 0.0F},
                        {GridExtent, Coordinate - ExpandedHalfWidth, 0.0F},
                        {GridExtent, Coordinate + ExpandedHalfWidth, 0.0F},
                        {-GridExtent, Coordinate + ExpandedHalfWidth, 0.0F},
                        GridColor,
                        {-GridEdgeCoordinate, -GridEdgeCoordinate, GridEdgeCoordinate, GridEdgeCoordinate});
            }

            GridVertexCount = static_cast<std::uint32_t>(Vertices.size());
            constexpr std::array<float, 4> NoEdgeFade{-2.0F, -2.0F, -2.0F, -2.0F};
            AddQuad({-GridExtent, -AxisHalfWidth, 0.002F},
                    {GridExtent, -AxisHalfWidth, 0.002F},
                    {GridExtent, AxisHalfWidth, 0.002F},
                    {-GridExtent, AxisHalfWidth, 0.002F},
                    WorldAxisColors[0],
                    NoEdgeFade);
            AddQuad({-AxisHalfWidth, -GridExtent, 0.002F},
                    {AxisHalfWidth, -GridExtent, 0.002F},
                    {AxisHalfWidth, GridExtent, 0.002F},
                    {-AxisHalfWidth, GridExtent, 0.002F},
                    WorldAxisColors[1],
                    NoEdgeFade);
            AddQuad({-AxisHalfWidth, 0.0F, -GridExtent},
                    {AxisHalfWidth, 0.0F, -GridExtent},
                    {AxisHalfWidth, 0.0F, GridExtent},
                    {-AxisHalfWidth, 0.0F, GridExtent},
                    WorldAxisColors[2],
                    NoEdgeFade);
            AxisVertexCount = static_cast<std::uint32_t>(Vertices.size()) - GridVertexCount;
            return Vertices;
        }

        std::vector<FGizmoVertex> BuildTranslateGizmoVertices()
        {
            std::vector<FGizmoVertex>      Vertices;
            constexpr int                  Segments = 16;
            constexpr float                ShaftRadius = 0.025F;
            constexpr float                HeadRadius = 0.075F;
            constexpr float                ShaftEnd = 0.76F;
            const std::array<glm::vec3, 3> Axes = {glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)};
            const std::array<glm::vec3, 3> U = {glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0)};
            const std::array<glm::vec3, 3> V = {glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)};
            const auto&                    Colors = WorldAxisColors;
            Vertices.reserve(3U * static_cast<std::size_t>(Segments) * 9U);
            for (std::size_t Axis = 0; Axis < Axes.size(); ++Axis)
            {
                const glm::vec4 Color = Colors[Axis];
                auto            PushTriangle = [&](glm::vec3 A, glm::vec3 B, glm::vec3 C)
                {
                    auto World = [&](glm::vec3 P) { return U[Axis] * P.x + V[Axis] * P.y + Axes[Axis] * P.z; };
                    Vertices.push_back({World(A), Color});
                    Vertices.push_back({World(B), Color});
                    Vertices.push_back({World(C), Color});
                };
                for (int I = 0; I < Segments; ++I)
                {
                    const float     A0 = glm::two_pi<float>() * static_cast<float>(I) / Segments;
                    const float     A1 = glm::two_pi<float>() * static_cast<float>(I + 1) / Segments;
                    const glm::vec3 S0{ShaftRadius * std::cos(A0), ShaftRadius * std::sin(A0), 0.0F};
                    const glm::vec3 S1{ShaftRadius * std::cos(A1), ShaftRadius * std::sin(A1), 0.0F};
                    const glm::vec3 E0{S0.x, S0.y, ShaftEnd};
                    const glm::vec3 E1{S1.x, S1.y, ShaftEnd};
                    PushTriangle(S0, S1, E1);
                    PushTriangle(S0, E1, E0);
                    const glm::vec3 H0{HeadRadius * std::cos(A0), HeadRadius * std::sin(A0), ShaftEnd};
                    const glm::vec3 H1{HeadRadius * std::cos(A1), HeadRadius * std::sin(A1), ShaftEnd};
                    PushTriangle(H0, H1, {0.0F, 0.0F, 1.0F});
                }
            }
            return Vertices;
        }

        std::vector<FGizmoVertex> BuildRotateGizmoVertices()
        {
            std::vector<FGizmoVertex>      Vertices;
            constexpr int                  Segments = 96;
            constexpr float                Radius = 0.9F;
            constexpr float                HalfWidth = 0.012F;
            const std::array<glm::vec3, 3> U = {glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0)};
            const std::array<glm::vec3, 3> V = {glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)};

            Vertices.reserve(3U * static_cast<std::size_t>(Segments) * 6U);
            for (std::size_t Axis = 0; Axis < U.size(); ++Axis)
            {
                const glm::vec4 Color = WorldAxisColors[Axis];
                for (int Segment = 0; Segment < Segments; ++Segment)
                {
                    const float     A0 = glm::two_pi<float>() * static_cast<float>(Segment) / Segments;
                    const float     A1 = glm::two_pi<float>() * static_cast<float>(Segment + 1) / Segments;
                    const glm::vec3 Radial0 = U[Axis] * std::cos(A0) + V[Axis] * std::sin(A0);
                    const glm::vec3 Radial1 = U[Axis] * std::cos(A1) + V[Axis] * std::sin(A1);
                    const glm::vec3 Inner0 = (Radius - HalfWidth) * Radial0;
                    const glm::vec3 Outer0 = (Radius + HalfWidth) * Radial0;
                    const glm::vec3 Inner1 = (Radius - HalfWidth) * Radial1;
                    const glm::vec3 Outer1 = (Radius + HalfWidth) * Radial1;
                    Vertices.insert(Vertices.end(),
                                    {{Inner0, Color},
                                     {Outer0, Color},
                                     {Outer1, Color},
                                     {Inner0, Color},
                                     {Outer1, Color},
                                     {Inner1, Color}});
                }
            }
            return Vertices;
        }

#pragma endregion
    }
}
