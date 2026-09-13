#include "AssetManager/Loader/OBJLoader.h"

#include "Logger/Logger.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <cmath>
#include <cstddef>
#include <glm/geometric.hpp>
#include <stdexcept>
#include <string>
#include <tiny_obj_loader.h>
#include <unordered_map>
#include <utility>

namespace MDSS
{
    namespace
    {
        struct VertexKey
        {
            int Position = -1;
            int Normal = -1;
            int UV = -1;

            bool operator==(const VertexKey& Other) const noexcept
            {
                return Position == Other.Position && Normal == Other.Normal && UV == Other.UV;
            }
        };

        struct VertexKeyHash
        {
            std::size_t operator()(const VertexKey& Key) const noexcept
            {
                std::size_t Hash = static_cast<std::size_t>(Key.Position + 1);
                Hash ^= static_cast<std::size_t>(Key.Normal + 1) * 0x9e3779b9U + (Hash << 6U) + (Hash >> 2U);
                Hash ^= static_cast<std::size_t>(Key.UV + 1) * 0x85ebca6bU + (Hash << 6U) + (Hash >> 2U);
                return Hash;
            }
        };

        glm::vec3 ReadPosition(const tinyobj::attrib_t& Attrib, int Index)
        {
            if (Index < 0)
            {
                throw std::runtime_error("OBJ face contains a missing position index.");
            }
            const std::size_t Base = static_cast<std::size_t>(Index) * 3U;
            return {Attrib.vertices.at(Base), Attrib.vertices.at(Base + 1U), Attrib.vertices.at(Base + 2U)};
        }

        glm::vec3 ReadNormal(const tinyobj::attrib_t& Attrib, int Index)
        {
            if (Index < 0)
            {
                return {0.0F, 0.0F, 0.0F};
            }
            const std::size_t Base = static_cast<std::size_t>(Index) * 3U;
            return {Attrib.normals.at(Base), Attrib.normals.at(Base + 1U), Attrib.normals.at(Base + 2U)};
        }

        glm::vec2 ReadUV(const tinyobj::attrib_t& Attrib, int Index)
        {
            if (Index < 0)
            {
                return {0.0F, 0.0F};
            }
            const std::size_t Base = static_cast<std::size_t>(Index) * 2U;
            // OBJ's conventional V axis has the opposite image-space direction from Vulkan texture coordinates.
            return {Attrib.texcoords.at(Base), 1.0F - Attrib.texcoords.at(Base + 1U)};
        }
    } // namespace

    OBJLoadResult OBJLoader::Load(const std::filesystem::path& Path)
    {
        tinyobj::attrib_t                Attrib{};
        std::vector<tinyobj::shape_t>    Shapes;
        std::vector<tinyobj::material_t> Materials;
        std::string                      Warning;
        std::string                      Error;

        const std::filesystem::path BaseDirectory = Path.parent_path();
        const std::string           BaseDirectoryString =
            BaseDirectory.string() + std::string(1, std::filesystem::path::preferred_separator);

        if (!tinyobj::LoadObj(&Attrib,
                              &Shapes,
                              &Materials,
                              &Warning,
                              &Error,
                              Path.string().c_str(),
                              BaseDirectoryString.c_str(),
                              true))
        {
            throw std::runtime_error("Failed to load OBJ '" + Path.string() + "': " + Warning + Error);
        }

        if (!Warning.empty())
        {
            Logger::Warning("OBJLoader", Warning);
        }

        Logger::Debug("OBJLoader",
                      "Parsed '" + Path.filename().string() + "': shapes=" + std::to_string(Shapes.size()) +
                          ", materials=" + std::to_string(Materials.size()) + ".");

        OBJLoadResult Result{};
        Result.Materials.reserve(Materials.size());
        for (const tinyobj::material_t& Material : Materials)
        {
            Result.Materials.push_back(MTLLoader::Convert(Material, BaseDirectory));
        }

        std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> UniqueVertices;
        std::vector<glm::vec3>                                      GeneratedNormalAccumulator;

        auto AppendVertex = [&](const tinyobj::index_t& Index) -> std::uint32_t
        {
            const VertexKey Key{Index.vertex_index, Index.normal_index, Index.texcoord_index};
            if (const auto Existing = UniqueVertices.find(Key); Existing != UniqueVertices.end())
            {
                return Existing->second;
            }

            Vertex NewVertex{};
            NewVertex.Position = ReadPosition(Attrib, Index.vertex_index);
            NewVertex.Normal = ReadNormal(Attrib, Index.normal_index);
            NewVertex.UV = ReadUV(Attrib, Index.texcoord_index);

            const std::uint32_t NewIndex = static_cast<std::uint32_t>(Result.Vertices.size());
            Result.Vertices.push_back(NewVertex);
            GeneratedNormalAccumulator.emplace_back(0.0F);
            UniqueVertices.emplace(Key, NewIndex);
            return NewIndex;
        };

        for (const tinyobj::shape_t& Shape : Shapes)
        {
            std::size_t IndexOffset = 0;
            for (std::size_t Face = 0; Face < Shape.mesh.num_face_vertices.size(); ++Face)
            {
                const int VertexCount = Shape.mesh.num_face_vertices[Face];
                if (VertexCount != 3)
                {
                    throw std::runtime_error("OBJ triangulation did not produce a triangle face.");
                }

                const std::int32_t MaterialIndex =
                    Face < Shape.mesh.material_ids.size() ? Shape.mesh.material_ids[Face] : -1;

                if (Result.Sections.empty() || Result.Sections.back().MaterialIndex != MaterialIndex)
                {
                    Result.Sections.push_back({static_cast<std::uint32_t>(Result.Indices.size()), 0, MaterialIndex});
                }

                std::uint32_t Triangle[3]{};
                for (int VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
                {
                    const tinyobj::index_t& ObjIndex =
                        Shape.mesh.indices[IndexOffset + static_cast<std::size_t>(VertexIndex)];
                    Triangle[VertexIndex] = AppendVertex(ObjIndex);
                    Result.Indices.push_back(Triangle[VertexIndex]);
                    ++Result.Sections.back().IndexCount;
                }

                const glm::vec3 Edge1 = Result.Vertices[Triangle[1]].Position - Result.Vertices[Triangle[0]].Position;
                const glm::vec3 Edge2 = Result.Vertices[Triangle[2]].Position - Result.Vertices[Triangle[0]].Position;
                const glm::vec3 FaceNormal = glm::cross(Edge1, Edge2);
                if (glm::dot(FaceNormal, FaceNormal) > 1.0e-12F)
                {
                    const glm::vec3 NormalizedFaceNormal = glm::normalize(FaceNormal);
                    for (std::uint32_t Index : Triangle)
                    {
                        if (glm::dot(Result.Vertices[Index].Normal, Result.Vertices[Index].Normal) <= 1.0e-12F)
                        {
                            GeneratedNormalAccumulator[Index] += NormalizedFaceNormal;
                        }
                    }
                }

                IndexOffset += static_cast<std::size_t>(VertexCount);
            }
        }

        for (std::size_t Index = 0; Index < Result.Vertices.size(); ++Index)
        {
            if (glm::dot(Result.Vertices[Index].Normal, Result.Vertices[Index].Normal) <= 1.0e-12F)
            {
                const glm::vec3& Sum = GeneratedNormalAccumulator[Index];
                Result.Vertices[Index].Normal =
                    glm::dot(Sum, Sum) > 1.0e-12F ? glm::normalize(Sum) : glm::vec3(0.0F, 1.0F, 0.0F);
            }
            else
            {
                Result.Vertices[Index].Normal = glm::normalize(Result.Vertices[Index].Normal);
            }
        }

        GenerateTangents(Result.Vertices, Result.Indices);

        if (Result.Vertices.empty() || Result.Indices.empty())
        {
            throw std::runtime_error("OBJ contains no renderable triangle data: " + Path.string());
        }

        if (Result.Sections.empty())
        {
            Result.Sections.push_back({0, static_cast<std::uint32_t>(Result.Indices.size()), -1});
        }

        Logger::Info("OBJLoader",
                     "Generated mesh data: vertices=" + std::to_string(Result.Vertices.size()) +
                         ", indices=" + std::to_string(Result.Indices.size()) +
                         ", sections=" + std::to_string(Result.Sections.size()) + ".");
        return Result;
    }

    void OBJLoader::GenerateTangents(std::vector<Vertex>& Vertices, const std::vector<std::uint32_t>& Indices)
    {
        std::vector<glm::vec3> TangentSums(Vertices.size(), glm::vec3(0.0F));
        std::vector<glm::vec3> BitangentSums(Vertices.size(), glm::vec3(0.0F));

        for (std::size_t Index = 0; Index + 2U < Indices.size(); Index += 3U)
        {
            const std::uint32_t I0 = Indices[Index];
            const std::uint32_t I1 = Indices[Index + 1U];
            const std::uint32_t I2 = Indices[Index + 2U];

            const glm::vec3 Edge1 = Vertices[I1].Position - Vertices[I0].Position;
            const glm::vec3 Edge2 = Vertices[I2].Position - Vertices[I0].Position;
            const glm::vec2 UV1 = Vertices[I1].UV - Vertices[I0].UV;
            const glm::vec2 UV2 = Vertices[I2].UV - Vertices[I0].UV;

            const float Determinant = UV1.x * UV2.y - UV1.y * UV2.x;
            if (std::abs(Determinant) <= 1.0e-8F)
            {
                continue;
            }

            const float     Reciprocal = 1.0F / Determinant;
            const glm::vec3 Tangent = Reciprocal * (UV2.y * Edge1 - UV1.y * Edge2);
            const glm::vec3 Bitangent = Reciprocal * (-UV2.x * Edge1 + UV1.x * Edge2);

            for (const std::uint32_t VertexIndex : {I0, I1, I2})
            {
                TangentSums[VertexIndex] += Tangent;
                BitangentSums[VertexIndex] += Bitangent;
            }
        }

        for (std::size_t Index = 0; Index < Vertices.size(); ++Index)
        {
            const glm::vec3 Normal = glm::normalize(Vertices[Index].Normal);
            glm::vec3       Tangent = TangentSums[Index];

            if (glm::dot(Tangent, Tangent) <= 1.0e-12F)
            {
                const glm::vec3 Axis =
                    std::abs(Normal.y) < 0.999F ? glm::vec3(0.0F, 1.0F, 0.0F) : glm::vec3(1.0F, 0.0F, 0.0F);
                Tangent = glm::normalize(glm::cross(Axis, Normal));
            }
            else
            {
                Tangent = glm::normalize(Tangent - Normal * glm::dot(Normal, Tangent));
            }

            const glm::vec3 Bitangent = BitangentSums[Index];
            const float     Handedness = glm::dot(glm::cross(Normal, Tangent), Bitangent) < 0.0F ? -1.0F : 1.0F;
            Vertices[Index].Tangent = glm::vec4(Tangent, Handedness);
        }
    }
} // namespace MDSS
