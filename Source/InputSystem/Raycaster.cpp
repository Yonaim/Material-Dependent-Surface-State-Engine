/**
 * @file Raycaster.cpp
 * @brief CPU ray queries against static mesh instances in a scene.
 */

#include "InputSystem/Raycaster.h"

#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Core/AssetManager.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace MDSS
{
    namespace
    {
        constexpr float RayEpsilon = 1.0e-6F;

        bool IntersectFrontFacingTriangle(glm::vec3 Origin,
                                          glm::vec3 Direction,
                                          glm::vec3 A,
                                          glm::vec3 B,
                                          glm::vec3 C,
                                          float&    Distance,
                                          glm::vec3& Barycentric)
        {
            const glm::vec3 Edge1 = B - A;
            const glm::vec3 Edge2 = C - A;
            const glm::vec3 P = glm::cross(Direction, Edge2);
            const float     Determinant = glm::dot(Edge1, P);

            // Positive determinant selects the triangle's front side and rejects degenerate/back-facing hits.
            if (Determinant <= RayEpsilon)
            {
                return false;
            }

            const float     InverseDeterminant = 1.0F / Determinant;
            const glm::vec3 FromA = Origin - A;
            const float     U = glm::dot(FromA, P) * InverseDeterminant;
            if (U < 0.0F || U > 1.0F)
            {
                return false;
            }

            const glm::vec3 Q = glm::cross(FromA, Edge1);
            const float     V = glm::dot(Direction, Q) * InverseDeterminant;
            if (V < 0.0F || U + V > 1.0F)
            {
                return false;
            }

            const float T = glm::dot(Edge2, Q) * InverseDeterminant;
            if (T <= RayEpsilon)
            {
                return false;
            }

            Distance = T;
            Barycentric = {1.0F - U - V, U, V};
            return true;
        }
    } // namespace

    TSurfaceRayHit TRaycaster::Cast(const TScene&       Scene,
                                    const TAssetManager& Assets,
                                    glm::vec3           WorldOrigin,
                                    glm::vec3           WorldDirection)
    {
        TSurfaceRayHit Result;
        const float    DirectionLengthSquared = glm::dot(WorldDirection, WorldDirection);
        if (!std::isfinite(DirectionLengthSquared) || DirectionLengthSquared <= RayEpsilon * RayEpsilon)
        {
            return Result;
        }
        WorldDirection = glm::normalize(WorldDirection);

        float BestDistance = std::numeric_limits<float>::infinity();
        const std::vector<TStaticMeshInstance>& Instances = Scene.GetStaticMeshInstances();
        for (std::size_t InstanceIndex = 0; InstanceIndex < Instances.size(); ++InstanceIndex)
        {
            const TStaticMeshInstance& Instance = Instances[InstanceIndex];
            if (Instance.GetMesh() == InvalidAssetHandle)
            {
                continue;
            }

            const TMeshAsset& Mesh = Assets.GetMesh(Instance.GetMesh());
            const std::vector<TVertex>& Vertices = Mesh.GetVertices();
            const glm::mat4              Model = Instance.GetTransform().GetMatrix();
            const std::vector<TMeshTriangleSource>& Triangles = Mesh.GetTriangles();

            for (std::size_t TriangleIndex = 0; TriangleIndex < Triangles.size(); ++TriangleIndex)
            {
                const TMeshTriangleSource& Triangle = Triangles[TriangleIndex];
                const std::uint32_t        IA = Triangle.RenderVertexIndices[0];
                const std::uint32_t        IB = Triangle.RenderVertexIndices[1];
                const std::uint32_t        IC = Triangle.RenderVertexIndices[2];
                if (IA >= Vertices.size() || IB >= Vertices.size() || IC >= Vertices.size())
                {
                    continue;
                }

                const glm::vec3 A = glm::vec3(Model * glm::vec4(Vertices[IA].Position, 1.0F));
                const glm::vec3 B = glm::vec3(Model * glm::vec4(Vertices[IB].Position, 1.0F));
                const glm::vec3 C = glm::vec3(Model * glm::vec4(Vertices[IC].Position, 1.0F));

                float     Distance = 0.0F;
                glm::vec3 Barycentric{0.0F};
                if (!IntersectFrontFacingTriangle(
                        WorldOrigin, WorldDirection, A, B, C, Distance, Barycentric) ||
                    Distance >= BestDistance)
                {
                    continue;
                }

                BestDistance = Distance;
                Result.Hit = true;
                Result.InstanceIndex = InstanceIndex;
                Result.TriangleID = static_cast<std::uint32_t>(TriangleIndex);
                Result.Barycentric = Barycentric;
                Result.WorldPosition = WorldOrigin + WorldDirection * Distance;
                Result.SimulationUV = Barycentric.x * Vertices[IA].UV +
                                      Barycentric.y * Vertices[IB].UV +
                                      Barycentric.z * Vertices[IC].UV;
                Result.Distance = Distance;
            }
        }

        return Result;
    }
} // namespace MDSS
