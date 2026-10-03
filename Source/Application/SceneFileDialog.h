/**
 * @file SceneFileDialog.h
 * @brief Scene 파일 선택 창을 여는 함수 인터페이스를 선언한다.
 */
#pragma once

#include <filesystem>
#include <optional>

namespace MDSS
{
    class TSceneFileDialog final
    {
    public:
        [[nodiscard]] static std::optional<std::filesystem::path> OpenScene();
        [[nodiscard]] static std::optional<std::filesystem::path> SaveScene();
    };
} // namespace MDSS
