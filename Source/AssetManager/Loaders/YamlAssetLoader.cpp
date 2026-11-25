/**
 * @file YamlAssetLoader.cpp
 * @brief YAML 저작 에셋을 공용 스키마 검증에서 읽을 수 있는 값으로 변환한다.
 */

#include "AssetManager/Loaders/YamlAssetLoader.h"
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string_view>
#include <yaml-cpp/yaml.h>

namespace MDSS::Asset
{
    // ============================================================
    // 00. YAML Value Conversion
    // ============================================================
    #pragma region 00_YAML_Value_Conversion
        namespace
        {
            nlohmann::json ConvertYamlScalar(const YAML::Node &Node)
            {
                const auto &Value = Node.Scalar();
                const auto &Tag = Node.Tag();
                if (Tag == "!" || Tag == "tag:yaml.org,2002:str")
                    return Value;
                if (Value == "true" || Value == "True" || Value == "TRUE")
                    return true;
                if (Value == "false" || Value == "False" || Value == "FALSE")
                    return false;

                std::string_view Number = Value;
                if (Number.starts_with('+'))
                    Number.remove_prefix(1);
                std::int64_t Integer = 0;
                const auto   End = Number.data() + Number.size();
                const auto   IntegerParse = std::from_chars(Number.data(), End, Integer);
                if (IntegerParse.ec == std::errc{} && IntegerParse.ptr == End)
                    return Integer;
                double     Decimal = 0.0;
                const auto DecimalParse = std::from_chars(Number.data(), End, Decimal);
                if (DecimalParse.ec == std::errc{} && DecimalParse.ptr == End && std::isfinite(Decimal))
                    return Decimal;
                return Value;
            }

            nlohmann::json ConvertYamlValue(const YAML::Node &Node, int Depth = 0)
            {
                // 저작 에셋의 유한한 트리 구조를 보장해 순환 별칭에서도 변환을 끝낸다.
                if (Depth > 64)
                    throw std::runtime_error("YAML asset nesting exceeds 64 levels.");
                if (Node.IsNull())
                    return nullptr;
                if (Node.IsScalar())
                    return ConvertYamlScalar(Node);
                if (Node.IsSequence())
                {
                    auto Values = nlohmann::json::array();
                    for (const auto &Value : Node)
                        Values.push_back(ConvertYamlValue(Value, Depth + 1));
                    return Values;
                }
                if (Node.IsMap())
                {
                    auto Values = nlohmann::json::object();
                    for (const auto &Entry : Node)
                    {
                        if (!Entry.first.IsScalar())
                            throw std::runtime_error("YAML asset keys must be scalar.");
                        const auto &Key = Entry.first.Scalar();
                        if (Values.contains(Key))
                            throw std::runtime_error("Duplicate YAML asset key: " + Key);
                        Values[Key] = ConvertYamlValue(Entry.second, Depth + 1);
                    }
                    return Values;
                }
                throw std::runtime_error("Unsupported YAML asset value.");
            }
        } // namespace
    #pragma endregion

    nlohmann::json LoadVersionedYamlAsset(const std::filesystem::path &Path)
    {
        try
        {
            std::ifstream Stream(Path, std::ios::binary);
            if (!Stream)
                throw std::runtime_error("Unable to open asset.");
            const auto Documents = YAML::LoadAll(Stream);
            if (Documents.size() != 1 || !Documents.front().IsMap())
                throw std::runtime_error("Expected one YAML mapping.");
            auto       Data = ConvertYamlValue(Documents.front());
            const auto Version = Data.find("version");
            if (Version == Data.end() || !Version->is_number_integer() || *Version != 1 ||
                Data.value("id", std::string{}) != Path.stem().string())
                throw std::runtime_error("Expected a version 1 asset with an ID matching the filename.");
            return Data;
        }
        catch (const std::exception &Error)
        {
            throw std::runtime_error(Path.string() + ": " + Error.what());
        }
    }
} // namespace MDSS::Asset
