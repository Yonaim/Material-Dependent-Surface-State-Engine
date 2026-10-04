/**
 * @file SRProfileLoader.cpp
 * @brief SRProfile JSON 파싱과 데이터 검증.
 */

#include "AssetManager/Loaders/SRProfileLoader.h"

#include "AssetManager/Assets/SRProfileAsset.h"

#include <array>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace MDSS::Asset
{
    namespace
    {
        using TJson = nlohmann::json;

        /** @brief JSON object에서 필수 key를 찾고 오류 경로를 포함해 실패를 보고한다. */
        const TJson& RequireMember(const TJson& Object, std::string_view Key, std::string_view JsonPath)
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

        std::string ReadString(const TJson& Object, std::string_view Key, std::string_view JsonPath)
        {
            const TJson& Value = RequireMember(Object, Key, JsonPath);
            if (!Value.is_string())
            {
                throw std::invalid_argument(std::string(JsonPath) + "." + std::string(Key) + " must be a string.");
            }

            return Value.get<std::string>();
        }

        /** @brief 필수 numeric field를 float으로 읽고 자료형과 유한성을 확인한다. */
        float ReadFloat(const TJson& Object, std::string_view Key, std::string_view JsonPath)
        {
            const TJson& Value = RequireMember(Object, Key, JsonPath);
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

        SurfaceState::TSurfaceStateParameters
        ReadStateParameters(const TJson& State, const std::string& StateName, std::int64_t Version)
        {
            const std::string JsonPath = "states." + StateName;
            const bool        bCurrentSchema = Version == 4;
            const char*       SaturationKey = bCurrentSchema ? "saturationSpreadFactor" : "saturationTransferFactor";
            const char*       GravityKey = bCurrentSchema ? "gravityFlowFactor" : "geometryTransferFactor";
            const char* ExitKey = bCurrentSchema ? "cavityExitResistanceFactor" : "cavityTransportRetentionFactor";
            const char* DecayProtectionKey = bCurrentSchema ? "cavityDecayProtectionFactor" : "cavityRetentionFactor";
            const std::array<const char*, 4> WrongVersionKeys =
                bCurrentSchema ? std::array<const char*, 4>{"saturationTransferFactor",
                                                            "geometryTransferFactor",
                                                            "cavityTransportRetentionFactor",
                                                            "cavityRetentionFactor"}
                               : std::array<const char*, 4>{"saturationSpreadFactor",
                                                            "gravityFlowFactor",
                                                            "cavityExitResistanceFactor",
                                                            "cavityDecayProtectionFactor"};
            for (const char* Key : WrongVersionKeys)
            {
                if (State.contains(Key))
                {
                    throw std::invalid_argument(JsonPath + "." + Key + " belongs to the other .SRProfile version.");
                }
            }

            float CavityTransportRetentionFactor = 0.0F;
            if (const auto Found = State.find(ExitKey); Found != State.end())
            {
                if (!Found->is_number())
                {
                    throw std::invalid_argument(JsonPath + "." + ExitKey + " must be a number.");
                }
                CavityTransportRetentionFactor = Found->get<float>();
            }

            return {
                ReadFloat(State, "stateCapacity", JsonPath),
                ReadFloat(State, "inputFactor", JsonPath),
                ReadFloat(State, SaturationKey, JsonPath),
                ReadFloat(State, GravityKey, JsonPath),
                ReadFloat(State, "decayRate", JsonPath),
                ReadFloat(State, DecayProtectionKey, JsonPath),
                ReadFloat(State, "accumulationFactor", JsonPath),
                ReadFloat(State, "cavityFillFactor", JsonPath),
                ReadFloat(State, "thicknessPerAmount", JsonPath),
                CavityTransportRetentionFactor,
            };
        }

        /** @brief Profile JSON schema를 동적 State domain data로 변환한다. */
        SurfaceState::TSurfaceResponseProfileData ParseProfile(const TJson& Root, std::string& Name)
        {
            if (!Root.is_object())
            {
                throw std::invalid_argument("$ must be a JSON object.");
            }
            if (ReadString(Root, "type", "$") != "SurfaceResponseProfile")
            {
                throw std::invalid_argument("$.type must be 'SurfaceResponseProfile'.");
            }

            const TJson& Version = RequireMember(Root, "version", "$");
            if (!Version.is_number_unsigned() && !Version.is_number_integer())
            {
                throw std::invalid_argument("$.version must be an integer.");
            }
            const std::int64_t SchemaVersion = Version.get<std::int64_t>();
            if (SchemaVersion != 3 && SchemaVersion != 4)
            {
                throw std::invalid_argument("$.version is unsupported; expected version 3 or 4.");
            }

            Name = ReadString(Root, "name", "$");
            if (Name.empty())
            {
                throw std::invalid_argument("$.name must not be empty.");
            }

            const TJson& States = RequireMember(Root, "states", "$");
            if (!States.is_object())
            {
                throw std::invalid_argument("$.states must be an object.");
            }
            SurfaceState::TSurfaceResponseProfileData Data;
            for (auto Iterator = States.begin(); Iterator != States.end(); ++Iterator)
            {
                if (!Iterator.value().is_object())
                {
                    throw std::invalid_argument("$.states." + Iterator.key() + " must be an object.");
                }

                const std::string CanonicalName = SurfaceState::NormalizeSurfaceStateName(Iterator.key());
                if (CanonicalName.empty())
                {
                    throw std::invalid_argument("$.states contains an empty State name after normalization.");
                }
                if (Data.States.contains(CanonicalName))
                {
                    throw std::invalid_argument("$.states contains duplicate State name after normalization: '" +
                                                CanonicalName + "'.");
                }
                Data.States.emplace(CanonicalName, ReadStateParameters(Iterator.value(), CanonicalName, SchemaVersion));
            }

            const TJson& Transitions = RequireMember(Root, "transitions", "$");
            if (!Transitions.is_array())
            {
                throw std::invalid_argument("$.transitions must be an array.");
            }

            Data.Transitions.reserve(Transitions.size());
            for (std::size_t Index = 0; Index < Transitions.size(); ++Index)
            {
                const TJson&      Transition = Transitions[Index];
                const std::string JsonPath = "transitions[" + std::to_string(Index) + "]";
                const std::string SourceName = SurfaceState::NormalizeSurfaceStateName(ReadString(Transition, "source", JsonPath));
                const std::string TargetName = SurfaceState::NormalizeSurfaceStateName(ReadString(Transition, "target", JsonPath));
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

    std::unique_ptr<TSRProfileAsset> TSRProfileLoader::Load(TAssetID ID, const std::filesystem::path& Path)
    {
        std::ifstream File(Path);
        if (!File)
        {
            throw std::runtime_error("Unable to open SRProfile file: " + Path.string());
        }

        try
        {
            const TJson                 Root = TJson::parse(File);
            std::string                 Name;
            SurfaceState::TSurfaceResponseProfileData Data = ParseProfile(Root, Name);
            return std::make_unique<TSRProfileAsset>(ID, std::move(Name), Path, std::move(Data));
        }
        catch (const std::exception& Exception)
        {
            throw std::runtime_error("Failed to load SRProfile '" + Path.string() + "': " + Exception.what());
        }
    }
} // namespace MDSS::Asset
