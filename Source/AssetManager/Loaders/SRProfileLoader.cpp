/**
 * @file SRProfileLoader.cpp
 * @brief SRProfile JSON 파싱과 데이터 검증.
 */

#include "AssetManager/Loaders/SRProfileLoader.h"

#include "AssetManager/Assets/SRProfileAsset.h"

#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace MDSS
{
    namespace
    {
        using Json = nlohmann::json;

        /** @brief JSON object에서 필수 key를 찾고 오류 경로를 포함해 실패를 보고한다. */
        const Json& RequireMember(const Json& Object, std::string_view Key, std::string_view JsonPath)
        {
            if (!Object.is_object())
            {
                throw std::invalid_argument(std::string(JsonPath) + " must be an object.");
            }

            const auto Found = Object.find(Key);
            if (Found == Object.end())
            {
                throw std::invalid_argument(std::string(JsonPath) + "." + std::string(Key) + " is required.");
            }

            return *Found;
        }

        std::string ReadString(const Json& Object, std::string_view Key, std::string_view JsonPath)
        {
            const Json& Value = RequireMember(Object, Key, JsonPath);
            if (!Value.is_string())
            {
                throw std::invalid_argument(std::string(JsonPath) + "." + std::string(Key) + " must be a string.");
            }

            return Value.get<std::string>();
        }

        /** @brief 필수 numeric field를 float으로 읽고 자료형과 유한성을 확인한다. */
        float ReadFloat(const Json& Object, std::string_view Key, std::string_view JsonPath)
        {
            const Json& Value = RequireMember(Object, Key, JsonPath);
            if (!Value.is_number())
            {
                throw std::invalid_argument(std::string(JsonPath) + "." + std::string(Key) + " must be a number.");
            }

            const double Number = Value.get<double>();
            const float  Result = static_cast<float>(Number);
            if (!std::isfinite(Number) || !std::isfinite(Result))
            {
                throw std::invalid_argument(std::string(JsonPath) + "." + std::string(Key) + " must be finite.");
            }

            return Result;
        }

        SurfaceStateParameters ReadStateParameters(const Json& State, const std::string& StateName)
        {
            const std::string JsonPath = "states." + StateName;

            return {
                ReadFloat(State, "stateCapacity", JsonPath),
                ReadFloat(State, "inputFactor", JsonPath),
                ReadFloat(State, "saturationTransferRate", JsonPath),
                ReadFloat(State, "geometryTransferRate", JsonPath),
                ReadFloat(State, "decayRate", JsonPath),
                ReadFloat(State, "cavityRetentionFactor", JsonPath),
                ReadFloat(State, "accumulationFactor", JsonPath),
                ReadFloat(State, "cavityFillFactor", JsonPath),
            };
        }

        /** @brief Profile JSON schema를 동적 State domain data로 변환한다. */
        SurfaceResponseProfileData ParseProfile(const Json& Root, std::string& Name)
        {
            if (!Root.is_object())
            {
                throw std::invalid_argument("$ must be a JSON object.");
            }
            if (ReadString(Root, "type", "$") != "SurfaceResponseProfile")
            {
                throw std::invalid_argument("$.type must be 'SurfaceResponseProfile'.");
            }

            const Json& Version = RequireMember(Root, "version", "$");
            if (!Version.is_number_unsigned() && !Version.is_number_integer())
            {
                throw std::invalid_argument("$.version must be an integer.");
            }
            if (Version.get<std::int64_t>() != 1)
            {
                throw std::invalid_argument("$.version is unsupported; expected version 1.");
            }

            Name = ReadString(Root, "name", "$");
            if (Name.empty())
            {
                throw std::invalid_argument("$.name must not be empty.");
            }

            const Json& States = RequireMember(Root, "states", "$");
            if (!States.is_object())
            {
                throw std::invalid_argument("$.states must be an object.");
            }
            SurfaceResponseProfileData Data;
            for (auto Iterator = States.begin(); Iterator != States.end(); ++Iterator)
            {
                if (!Iterator.value().is_object())
                {
                    throw std::invalid_argument("$.states." + Iterator.key() + " must be an object.");
                }

                const std::string CanonicalName = NormalizeSurfaceStateName(Iterator.key());
                if (CanonicalName.empty())
                {
                    throw std::invalid_argument("$.states contains an empty State name after normalization.");
                }
                if (Data.States.contains(CanonicalName))
                {
                    throw std::invalid_argument("$.states contains duplicate State name after normalization: '" +
                                                CanonicalName + "'.");
                }
                Data.States.emplace(CanonicalName, ReadStateParameters(Iterator.value(), CanonicalName));
            }

            const Json& Transitions = RequireMember(Root, "transitions", "$");
            if (!Transitions.is_array())
            {
                throw std::invalid_argument("$.transitions must be an array.");
            }

            Data.Transitions.reserve(Transitions.size());
            for (std::size_t Index = 0; Index < Transitions.size(); ++Index)
            {
                const Json&       Transition = Transitions[Index];
                const std::string JsonPath = "transitions[" + std::to_string(Index) + "]";
                const std::string SourceName = NormalizeSurfaceStateName(ReadString(Transition, "source", JsonPath));
                const std::string TargetName = NormalizeSurfaceStateName(ReadString(Transition, "target", JsonPath));
                if (SourceName.empty() || TargetName.empty())
                {
                    throw std::invalid_argument(JsonPath + " source and target must not be empty.");
                }

                Data.Transitions.push_back({SourceName,
                                            TargetName,
                                            ReadFloat(Transition, "threshold", JsonPath),
                                            ReadFloat(Transition, "transitionRate", JsonPath)});
            }

            ValidateSurfaceResponseProfileData(Data);
            return Data;
        }
    } // namespace

    std::unique_ptr<SRProfileAsset> SRProfileLoader::Load(AssetID ID, const std::filesystem::path& Path)
    {
        std::ifstream File(Path);
        if (!File)
        {
            throw std::runtime_error("Unable to open SRProfile file: " + Path.string());
        }

        try
        {
            const Json                 Root = Json::parse(File);
            std::string                Name;
            SurfaceResponseProfileData Data = ParseProfile(Root, Name);
            return std::make_unique<SRProfileAsset>(ID, std::move(Name), Path, std::move(Data));
        }
        catch (const std::exception& Exception)
        {
            throw std::runtime_error("Failed to load SRProfile '" + Path.string() + "': " + Exception.what());
        }
    }
} // namespace MDSS
