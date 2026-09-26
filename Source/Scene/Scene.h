/**
 * @file Scene.h
 * @brief 주 카메라와 정적 메시 인스턴스 구성.
 */

#pragma once

#include "Scene/Camera.h"
#include "Scene/StaticMeshInstance.h"

#include <vector>
#include <filesystem>

namespace MDSS
{
    class TScene
    {
    public:
        TScene();

        [[nodiscard]] TCamera&       GetMainCamera() noexcept;
        [[nodiscard]] const TCamera& GetMainCamera() const noexcept;
        void SetSourcePath(std::filesystem::path Path);
        [[nodiscard]] const std::filesystem::path& GetSourcePath() const noexcept;

        /** @brief TScene 소유 목록에 정적 메시 인스턴스를 추가한다. */
        void                                                 AddStaticMeshInstance(TStaticMeshInstance Instance);
        [[nodiscard]] std::vector<TStaticMeshInstance>&       GetStaticMeshInstances() noexcept;
        [[nodiscard]] const std::vector<TStaticMeshInstance>& GetStaticMeshInstances() const noexcept;

    private:
        TCamera                          MainCamera;
        std::vector<TStaticMeshInstance> StaticMeshInstances;
        std::filesystem::path SourcePath;
    };
} // namespace MDSS
