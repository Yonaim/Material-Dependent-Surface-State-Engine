/**
 * @file SceneLoader.cpp
 * @brief TScene JSON 파싱, 경로 해석과 Asset 연결.
 */

#include "AssetManager/Loaders/SceneLoader.h"

#include "AssetManager/Core/AssetManager.h"
#include "Scene/StaticMeshInstance.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>

namespace MDSS
{
    namespace
    {
        using TJson = nlohmann::json;

        const TJson& RequireMember(const TJson& Object, const char* Name, const std::string& Context)
        {
            if (!Object.is_object())
            {
                throw std::runtime_error(Context + " must be an object.");
            }
            const auto Found = Object.find(Name);
            if (Found == Object.end())
            {
                throw std::runtime_error(Context + " is missing required field '" + Name + "'.");
            }
            return *Found;
        }

        std::string ReadString(const TJson& Object, const char* Name, const std::string& Context)
        {
            const TJson& Value = RequireMember(Object, Name, Context);
            if (!Value.is_string() || Value.get_ref<const std::string&>().empty())
            {
                throw std::runtime_error(Context + "." + Name + " must be a non-empty string.");
            }
            return Value.get<std::string>();
        }

        std::uint32_t ReadVersion(const TJson& Value)
        {
            if (!Value.is_number_unsigned() && !Value.is_number_integer())
            {
                throw std::runtime_error("Scene version must be the integer 1.");
            }
            const std::int64_t Version = Value.get<std::int64_t>();
            if (Version != 1)
            {
                throw std::runtime_error("Unsupported Scene version; expected version 1.");
            }
            return static_cast<std::uint32_t>(Version);
        }

        float ReadFiniteFloat(const TJson& Value, const std::string& Context)
        {
            if (!Value.is_number())
            {
                throw std::runtime_error(Context + " must be a number.");
            }
            const double Number = Value.get<double>();
            const float Result = static_cast<float>(Number);
            if (!std::isfinite(Number) || !std::isfinite(Result))
            {
                throw std::runtime_error(Context + " must be finite.");
            }
            return Result;
        }

        glm::vec3 ReadVector3(const TJson& Value, const std::string& Context)
        {
            if (!Value.is_array() || Value.size() != 3)
            {
                throw std::runtime_error(Context + " must be a three-element array.");
            }
            return {ReadFiniteFloat(Value[0], Context + "[0]"),
                    ReadFiniteFloat(Value[1], Context + "[1]"),
                    ReadFiniteFloat(Value[2], Context + "[2]")};
        }

        TTransform ReadTransform(const TJson& Object, const std::string& Context)
        {
            TTransform Transform;
            if (Object.is_null())
            {
                return Transform;
            }
            if (!Object.is_object())
            {
                throw std::runtime_error(Context + " must be an object.");
            }

            if (const auto Position = Object.find("position"); Position != Object.end())
            {
                Transform.Position = ReadVector3(*Position, Context + ".position");
            }
            if (const auto Rotation = Object.find("rotationDegrees"); Rotation != Object.end())
            {
                Transform.RotationDegrees = ReadVector3(*Rotation, Context + ".rotationDegrees");
            }
            if (const auto Scale = Object.find("scale"); Scale != Object.end())
            {
                Transform.Scale = ReadVector3(*Scale, Context + ".scale");
            }
            return Transform;
        }

        std::filesystem::path ResolveScenePath(const std::filesystem::path& SceneDirectory,
                                               const std::string& PathText,
                                               const std::string& Field,
                                               const char* ExpectedExtension)
        {
            const std::filesystem::path RelativePath(PathText);
            if (RelativePath.is_absolute())
            {
                throw std::runtime_error("Scene " + Field + " path must be relative to the Scene file.");
            }
            if (RelativePath.extension() != ExpectedExtension)
            {
                throw std::runtime_error("Scene " + Field + " path must use the " + ExpectedExtension + " extension.");
            }
            return (SceneDirectory / RelativePath).lexically_normal();
        }
    } // namespace

    TScene TSceneLoader::Load(const std::filesystem::path& Path, TAssetManager& Assets)
    {
        std::ifstream Input(Path);
        if (!Input)
        {
            throw std::runtime_error("Unable to open Scene file: " + Path.string());
        }

        TJson Root;
        try
        {
            Input >> Root;
        }
        catch (const TJson::exception& Exception)
        {
            throw std::runtime_error("Invalid Scene JSON in '" + Path.string() + "': " + Exception.what());
        }
        if (!Root.is_object())
        {
            throw std::runtime_error("Scene root must be a JSON object.");
        }
        if (ReadString(Root, "type", "Scene") != "TScene")
        {
            throw std::runtime_error("Scene type must be 'TScene'.");
        }
        (void)ReadVersion(RequireMember(Root, "version", "Scene"));
        const TJson& Objects = RequireMember(Root, "objects", "Scene");
        if (!Objects.is_array())
        {
            throw std::runtime_error("Scene 'objects' must be an array.");
        }

        const std::filesystem::path SceneDirectory = std::filesystem::absolute(Path).parent_path();
        TScene Scene;
        Scene.SetSourcePath(std::filesystem::absolute(Path).lexically_normal());
        for (std::size_t Index = 0; Index < Objects.size(); ++Index)
        {
            const TJson& Object = Objects[Index];
            const std::string Context = "Scene objects[" + std::to_string(Index) + "]";
            const std::filesystem::path MeshPath = ResolveScenePath(
                SceneDirectory, ReadString(Object, "mesh", Context), "mesh", ".obj");
            const TMeshAssetHandle MeshHandle = Assets.LoadOBJ(MeshPath);

            TSurfaceRuntimeDataHandle SurfaceDataHandle = InvalidSurfaceRuntimeDataHandle;
            std::filesystem::path DistributionPath;
            if (const auto Distribution = Object.find("surfaceProfileMap"); Distribution != Object.end())
            {
                if (!Distribution->is_string() || Distribution->get_ref<const std::string&>().empty())
                {
                    throw std::runtime_error(Context + ".surfaceProfileMap must be a non-empty path string.");
                }
                DistributionPath = ResolveScenePath(
                    SceneDirectory, Distribution->get<std::string>(), "surfaceProfileMap", ".SurfaceProfileMap");
                SurfaceDataHandle = Assets.LoadSurfaceData(MeshHandle, DistributionPath);
            }

            TTransform Transform;
            if (const auto TransformJson = Object.find("transform"); TransformJson != Object.end())
            {
                Transform = ReadTransform(*TransformJson, Context + ".transform");
            }
            Scene.AddStaticMeshInstance(TStaticMeshInstance(MeshHandle,
                                                            SurfaceDataHandle,
                                                            Transform,
                                                            MeshPath,
                                                            DistributionPath));
        }
        return Scene;
    }

    void TSceneLoader::Save(const TScene& Scene, const std::filesystem::path& Path)
    {
        const std::filesystem::path AbsolutePath = std::filesystem::absolute(Path).lexically_normal();
        const std::filesystem::path Directory = AbsolutePath.parent_path();
        TJson Root;
        Root["type"] = "TScene";
        Root["version"] = 1;
        Root["objects"] = TJson::array();
        for (const TStaticMeshInstance& Instance : Scene.GetStaticMeshInstances())
        {
            if (Instance.GetMeshPath().empty())
            {
                throw std::runtime_error("Cannot save Scene object without its source Mesh path.");
            }
            const TTransform& Transform = Instance.GetTransform();
            TJson Object;
            Object["mesh"] = Instance.GetMeshPath().lexically_relative(Directory).generic_string();
            if (!Instance.GetProfileMapPath().empty())
            {
                Object["surfaceProfileMap"] =
                    Instance.GetProfileMapPath().lexically_relative(Directory).generic_string();
            }
            Object["transform"] = {{"position", {Transform.Position.x, Transform.Position.y, Transform.Position.z}},
                                    {"rotationDegrees", {Transform.RotationDegrees.x,
                                                          Transform.RotationDegrees.y,
                                                          Transform.RotationDegrees.z}},
                                    {"scale", {Transform.Scale.x, Transform.Scale.y, Transform.Scale.z}}};
            Root["objects"].push_back(std::move(Object));
        }

        std::ofstream Output(AbsolutePath);
        if (!Output)
        {
            throw std::runtime_error("Unable to write Scene file: " + AbsolutePath.string());
        }
        Output << std::setw(2) << Root << '\n';
        if (!Output)
        {
            throw std::runtime_error("Failed while writing Scene file: " + AbsolutePath.string());
        }
    }
} // namespace MDSS
