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
