/**
 * @file SceneLoader.cpp
 * @brief TScene JSON 파싱, 경로 해석과 Asset 연결.
 */

#include "Scene/SceneLoader.h"

#include "AssetManager/Core/AssetManager.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/Preprocessing/SurfaceDataManager.h"
#include "SurfaceState/Types/SurfaceStateTypes.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_set>

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

        std::uint32_t ReadSimulationResolution(const TJson& Value)
        {
            constexpr const char* Error = "Scene simulationResolution must be the integer 128, 256 or 512.";
            std::uint64_t         Number = 0;
            if (Value.is_number_unsigned())
            {
                Number = Value.get<std::uint64_t>();
            }
            else if (Value.is_number_integer())
            {
                const auto SignedNumber = Value.get<std::int64_t>();
                if (SignedNumber < 0)
                    throw std::runtime_error(Error);
                Number = static_cast<std::uint64_t>(SignedNumber);
            }
            else
            {
                throw std::runtime_error(Error);
            }
            for (const auto& Preset : SurfaceState::SurfaceSimulationResolutionPresets)
            {
                if (Number == Preset.Resolution)
                    return Preset.Resolution;
            }
            throw std::runtime_error(Error);
        }

        float ReadFiniteFloat(const TJson& Value, const std::string& Context)
        {
            if (!Value.is_number())
            {
                throw std::runtime_error(Context + " must be a number.");
            }
            const double Number = Value.get<double>();
            const float  Result = static_cast<float>(Number);
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
                                               const std::string&           PathText,
                                               const std::string&           Field,
                                               const char*                  ExpectedExtension)
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

    TScene TSceneLoader::Load(const std::filesystem::path&       Path,
                              Asset::TAssetManager&              Assets,
                              SurfaceState::TSurfaceDataManager& SurfaceData)
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
        TScene                      Scene;
        Scene.SetSourcePath(std::filesystem::absolute(Path).lexically_normal());
        if (const auto Resolution = Root.find("simulationResolution"); Resolution != Root.end())
        {
            Scene.SetSimulationResolution(ReadSimulationResolution(*Resolution));
        }
        if (const auto DisplayScale = Root.find("litHeightDisplayScale"); DisplayScale != Root.end())
        {
            Scene.SetLitHeightDisplayScale(ReadFiniteFloat(*DisplayScale, "Scene.litHeightDisplayScale"));
        }
        if (const auto Camera = Root.find("camera"); Camera != Root.end())
        {
            if (!Camera->is_object())
                throw std::runtime_error("Scene.camera must be an object.");
            const std::string Context = "Scene.camera";
            TCamera&          MainCamera = Scene.GetMainCamera();
            MainCamera.SetPosition(ReadVector3(RequireMember(*Camera, "position", Context), Context + ".position"));
            MainCamera.SetTarget(ReadVector3(RequireMember(*Camera, "target", Context), Context + ".target"));
            const float FieldOfView = ReadFiniteFloat(RequireMember(*Camera, "verticalFieldOfViewDegrees", Context),
                                                      Context + ".verticalFieldOfViewDegrees");
            if (FieldOfView <= 0.0F || FieldOfView >= 180.0F)
                throw std::runtime_error("Scene.camera.verticalFieldOfViewDegrees must be between 0 and 180.");
            MainCamera.SetVerticalFieldOfViewDegrees(FieldOfView);
        }
        std::unordered_set<std::string> ObjectIds;
        for (std::size_t Index = 0; Index < Objects.size(); ++Index)
        {
            const TJson&      Object = Objects[Index];
            const std::string Context = "Scene objects[" + std::to_string(Index) + "]";
            std::string       ObjectId;
            if (const auto Id = Object.find("id"); Id != Object.end())
            {
                ObjectId = ReadString(Object, "id", Context);
                if (ObjectId == "camera" || !ObjectIds.insert(ObjectId).second)
                    throw std::runtime_error(Context + ".id must be unique and cannot be 'camera'.");
            }
            const std::filesystem::path MeshPath =
                ResolveScenePath(SceneDirectory, ReadString(Object, "mesh", Context), "mesh", ".obj");
            const Asset::TMeshAssetHandle MeshHandle = Assets.LoadOBJ(MeshPath);

            SurfaceState::TSurfaceRuntimeDataHandle SurfaceDataHandle = SurfaceState::InvalidSurfaceRuntimeDataHandle;
            std::filesystem::path                   DistributionPath;
            if (const auto Distribution = Object.find("surfaceProfileMap"); Distribution != Object.end())
            {
                if (!Distribution->is_string() || Distribution->get_ref<const std::string&>().empty())
                {
                    throw std::runtime_error(Context + ".surfaceProfileMap must be a non-empty path string.");
                }
                DistributionPath = ResolveScenePath(
                    SceneDirectory, Distribution->get<std::string>(), "surfaceProfileMap", ".SurfaceProfileMap");
                SurfaceDataHandle =
                    SurfaceData.LoadSurfaceData(MeshHandle, DistributionPath, Scene.GetSimulationResolution());
            }

            TTransform Transform;
            if (const auto TransformJson = Object.find("transform"); TransformJson != Object.end())
            {
                Transform = ReadTransform(*TransformJson, Context + ".transform");
            }
            Scene.AddStaticMeshInstance(TStaticMeshInstance(
                MeshHandle, SurfaceDataHandle, Transform, MeshPath, DistributionPath, std::move(ObjectId)));
        }
        if (const auto Contacts = Root.find("initialContacts"); Contacts != Root.end())
        {
            if (!Contacts->is_array())
                throw std::runtime_error("Scene.initialContacts must be an array.");
            for (std::size_t Index = 0; Index < Contacts->size(); ++Index)
            {
                const TJson&         Contact = (*Contacts)[Index];
                const std::string    Context = "Scene.initialContacts[" + std::to_string(Index) + "]";
                TSceneInitialContact Initial;
                Initial.Target = ReadString(Contact, "target", Context);
                Initial.State = ReadString(Contact, "state", Context);
                if (!ObjectIds.contains(Initial.Target))
                    throw std::runtime_error(Context + ".target must name a Scene object with an id.");
                if (SurfaceState::NormalizeSurfaceStateName(Initial.State) != Initial.State)
                    throw std::runtime_error(Context + ".state must be a normalized State name.");
                Initial.WorldPosition =
                    ReadVector3(RequireMember(Contact, "worldPosition", Context), Context + ".worldPosition");
                Initial.Radius = ReadFiniteFloat(RequireMember(Contact, "radius", Context), Context + ".radius");
                Initial.Strength = ReadFiniteFloat(RequireMember(Contact, "strength", Context), Context + ".strength");
                if (const auto Falloff = Contact.find("falloff"); Falloff != Contact.end())
                    Initial.Falloff = ReadFiniteFloat(*Falloff, Context + ".falloff");
                if (Initial.Radius <= 0.0F || Initial.Strength < 0.0F || Initial.Falloff < 0.0F)
                    throw std::runtime_error(Context + " requires positive radius and nonnegative strength/falloff.");
                Scene.AddInitialContact(std::move(Initial));
            }
        }
        if (const auto Animation = Root.find("animation"); Animation != Root.end())
        {
            const std::filesystem::path AnimationPath =
                ResolveScenePath(SceneDirectory, ReadString(Root, "animation", "Scene"), "animation", ".DemoAnim");
            Scene.SetDemoAnimation(AnimationPath, LoadDemoAnimation(AnimationPath, Scene));
        }
        Scene.CaptureInitialState();
        return Scene;
    }

    void TSceneLoader::Save(const TScene& Scene, const std::filesystem::path& Path)
    {
        const std::filesystem::path AbsolutePath = std::filesystem::absolute(Path).lexically_normal();
        const std::filesystem::path Directory = AbsolutePath.parent_path();
        TJson                       Root;
        Root["type"] = "TScene";
        Root["version"] = 1;
        Root["simulationResolution"] = Scene.GetSimulationResolution();
        Root["litHeightDisplayScale"] = Scene.GetLitHeightDisplayScale();
        const TCamera&   Camera = Scene.GetMainCamera();
        const glm::vec3& CameraPosition = Camera.GetPosition();
        const glm::vec3& CameraTarget = Camera.GetTarget();
        Root["camera"] = {{"position", {CameraPosition.x, CameraPosition.y, CameraPosition.z}},
                          {"target", {CameraTarget.x, CameraTarget.y, CameraTarget.z}},
                          {"verticalFieldOfViewDegrees", Camera.GetVerticalFieldOfViewDegrees()}};
        if (Scene.HasDemoAnimation())
            Root["animation"] = Scene.GetDemoAnimationPath().lexically_relative(Directory).generic_string();
        if (!Scene.GetInitialContacts().empty())
        {
            Root["initialContacts"] = TJson::array();
            for (const auto& Contact : Scene.GetInitialContacts())
                Root["initialContacts"].push_back(
                    {{"target", Contact.Target},
                     {"state", Contact.State},
                     {"worldPosition", {Contact.WorldPosition.x, Contact.WorldPosition.y, Contact.WorldPosition.z}},
                     {"radius", Contact.Radius},
                     {"strength", Contact.Strength},
                     {"falloff", Contact.Falloff}});
        }
        Root["objects"] = TJson::array();
        for (const TStaticMeshInstance& Instance : Scene.GetStaticMeshInstances())
        {
            if (Instance.GetMeshPath().empty())
            {
                throw std::runtime_error("Cannot save Scene object without its source Mesh path.");
            }
            const TTransform& Transform = Instance.GetTransform();
            TJson             Object;
            if (!Instance.GetId().empty())
                Object["id"] = Instance.GetId();
            Object["mesh"] = Instance.GetMeshPath().lexically_relative(Directory).generic_string();
            if (!Instance.GetProfileMapPath().empty())
            {
                Object["surfaceProfileMap"] =
                    Instance.GetProfileMapPath().lexically_relative(Directory).generic_string();
            }
            Object["transform"] = {
                {"position", {Transform.Position.x, Transform.Position.y, Transform.Position.z}},
                {"rotationDegrees",
                 {Transform.RotationDegrees.x, Transform.RotationDegrees.y, Transform.RotationDegrees.z}},
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
