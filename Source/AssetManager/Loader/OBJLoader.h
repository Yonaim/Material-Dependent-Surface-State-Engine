#pragma once

#include "AssetManager/Loader/MTLLoader.h"
#include "AssetManager/MeshAsset.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace MDSS
{
    struct OBJMeshSectionData
    {
        std::uint32_t FirstIndex = 0;
        std::uint32_t IndexCount = 0;
        std::int32_t  MaterialIndex = -1;
    };

    struct OBJLoadResult
    {
        std::vector<Vertex>             Vertices;
        std::vector<std::uint32_t>      Indices;
        std::vector<OBJMeshSectionData> Sections;
        std::vector<MaterialSourceData> Materials;
    };

    class OBJLoader
    {
    public:
        [[nodiscard]] static OBJLoadResult Load(const std::filesystem::path& Path);

    private:
        static void GenerateTangents(std::vector<Vertex>& Vertices, const std::vector<std::uint32_t>& Indices);
    };
} // namespace MDSS
