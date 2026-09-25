/**
 * @file OBJLoader.h
 * @brief OBJ 메시 데이터 로딩과 vertex tangent 전처리.
 */

#pragma once

#include "AssetManager/Loaders/MTLLoader.h"
#include "AssetManager/Assets/MeshSourceData.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace MDSS
{
    struct OBJMeshSectionData
    {
        std::uint32_t  FirstIndex = 0;
        std::uint32_t  IndexCount = 0;
        std::int32_t   MaterialIndex = -1;
        SurfaceLocalID Surface = InvalidSurfaceID;
    };

    struct OBJLoadResult
    {
        std::vector<Vertex>             Vertices;
        std::vector<std::uint32_t>      Indices;
        std::vector<OBJMeshSectionData> Sections;
        std::vector<MaterialSourceData> Materials;
        std::vector<MeshTriangleSource> Triangles;
    };

    class OBJLoader
    {
    public:
        /**
         * @brief OBJ 메시와 MTL 참조를 읽어 CPU-side 메시 데이터를 만든다.
         * @param Path 읽을 OBJ 파일 경로.
         * @return 정점, 인덱스, 재질 section 및 변환된 material 데이터.
         * @throws std::runtime_error 파일 파싱 실패 또는 비삼각형 face가 발견된 경우.
         */
        [[nodiscard]] static OBJLoadResult Load(const std::filesystem::path& Path);

    private:
        /** @brief 삼각형 UV gradient에서 각 정점의 tangent와 handedness를 계산한다. */
        static void GenerateTangents(std::vector<Vertex>& Vertices, const std::vector<std::uint32_t>& Indices);
    };
} // namespace MDSS
