/**
 * @file DemoAnimation.cpp
 * @brief JSON keyframe을 읽고 Scene 객체와 카메라 animation을 평가한다.
 */

#include "Scene/DemoAnimation.h"

#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"

#include <glm/common.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace MDSS
{
    namespace
    {
        using TJson = nlohmann::json;

        const TJson& Required(const TJson& Object, const char* Field, const std::string& Context)
        {
            if (!Object.is_object() || !Object.contains(Field))
                throw std::runtime_error(Context + " is missing '" + Field + "'.");
            return Object.at(Field);
        }

        std::string String(const TJson& Object, const char* Field, const std::string& Context)
        {
            const TJson& Value = Required(Object, Field, Context);
            if (!Value.is_string() || Value.get_ref<const std::string&>().empty())
                throw std::runtime_error(Context + "." + Field + " must be a non-empty string.");
            return Value.get<std::string>();
        }

        float Float(const TJson& Value, const std::string& Context)
        {
            if (!Value.is_number()) throw std::runtime_error(Context + " must be numeric.");
            const double Number = Value.get<double>();
            const float Result = static_cast<float>(Number);
            if (!std::isfinite(Number) || !std::isfinite(Result))
                throw std::runtime_error(Context + " must be finite.");
            return Result;
        }

        EDemoAnimationProperty Property(const std::string& Target, const std::string& Name)
        {
            if (Target == "camera")
            {
                if (Name == "position") return EDemoAnimationProperty::CameraPosition;
                if (Name == "target") return EDemoAnimationProperty::CameraTarget;
            }
            else
            {
                if (Name == "position") return EDemoAnimationProperty::Position;
                if (Name == "rotation") return EDemoAnimationProperty::Rotation;
                if (Name == "scale") return EDemoAnimationProperty::Scale;
            }
            throw std::runtime_error("Demo animation property '" + Name + "' is invalid for target '" + Target + "'.");
        }

        glm::vec4 ReadValue(const TJson& Value, bool bRotation, const std::string& Context)
        {
            const std::size_t Expected = bRotation ? 4U : 3U;
            if (!Value.is_array() || Value.size() != Expected)
                throw std::runtime_error(Context + " must have " + std::to_string(Expected) + " numeric components.");
            glm::vec4 Result(0.0F);
            for (std::size_t I = 0; I < Expected; ++I)
                Result[static_cast<glm::length_t>(I)] = Float(Value[I], Context + "[" + std::to_string(I) + "]");
            if (bRotation)
            {
                const float Length = glm::length(Result);
                if (Length <= 1.0e-6F) throw std::runtime_error(Context + " quaternion must have non-zero length.");
                Result /= Length;
            }
            return Result;
        }

        glm::vec4 Sample(const TDemoAnimationTrack& Track, float Time)
        {
            if (Time <= Track.Keys.front().Time) return Track.Keys.front().Value;
            if (Time >= Track.Keys.back().Time) return Track.Keys.back().Value;
            const auto Upper = std::upper_bound(Track.Keys.begin(), Track.Keys.end(), Time,
                [](float T, const TDemoAnimationKey& Key) { return T < Key.Time; });
            const TDemoAnimationKey& B = *Upper;
            const TDemoAnimationKey& A = *(Upper - 1);
            if (Track.Interpolation == EDemoAnimationInterpolation::Step) return A.Value;
            const float Blend = (Time - A.Time) / (B.Time - A.Time);
            if (Track.Interpolation == EDemoAnimationInterpolation::Slerp)
            {
                const glm::quat QA(A.Value.w, A.Value.x, A.Value.y, A.Value.z);
                const glm::quat QB(B.Value.w, B.Value.x, B.Value.y, B.Value.z);
                const glm::quat Q = glm::normalize(glm::slerp(QA, QB, Blend));
                return {Q.x, Q.y, Q.z, Q.w};
            }
            return glm::mix(A.Value, B.Value, Blend);
        }

        glm::vec3 QuaternionToTransformEuler(const glm::vec4& Value)
        {
            const glm::mat3 R = glm::mat3_cast(glm::quat(Value.w, Value.x, Value.y, Value.z));
            // TTransform composes rotations as Rx * Ry * Rz.
            const float Y = std::asin(glm::clamp(R[2][0], -1.0F, 1.0F));
            const float CosY = std::cos(Y);
            const float X = std::abs(CosY) > 1.0e-5F
                ? std::atan2(-R[2][1], R[2][2])
                : std::atan2(std::copysign(1.0F, Y) * R[0][1], R[1][1]);
            const float Z = std::abs(CosY) > 1.0e-5F ? std::atan2(-R[1][0], R[0][0]) : 0.0F;
            return glm::degrees(glm::vec3(X, Y, Z));
        }
    } // namespace

    TDemoAnimationClip LoadDemoAnimation(const std::filesystem::path& Path, const TScene& Scene)
    {
        std::ifstream Input(Path);
        if (!Input) throw std::runtime_error("Unable to open Demo Animation: " + Path.string());
        TJson Root;
        try { Input >> Root; }
        catch (const TJson::exception& Error)
        {
            throw std::runtime_error("Invalid Demo Animation JSON in '" + Path.string() + "': " + Error.what());
        }
        const TJson& Version = Required(Root, "version", "Demo Animation");
        if (!Version.is_number_integer() || Version.get<int>() != 1)
            throw std::runtime_error("Demo Animation version must be 1.");
        TDemoAnimationClip Clip;
        Clip.DurationSeconds = Float(Required(Root, "durationSeconds", "Demo Animation"), "durationSeconds");
        if (Clip.DurationSeconds <= 0.0F) throw std::runtime_error("Demo Animation duration must be positive.");
        if (const auto Loop = Root.find("loop"); Loop != Root.end())
        {
            if (!Loop->is_boolean()) throw std::runtime_error("Demo Animation loop must be a boolean.");
            Clip.bLoop = Loop->get<bool>();
        }
        const TJson& Tracks = Required(Root, "tracks", "Demo Animation");
        if (!Tracks.is_array() || Tracks.empty())
            throw std::runtime_error("Demo Animation tracks must be a non-empty array.");
        std::unordered_set<std::string> UsedProperties;
        Clip.Tracks.reserve(Tracks.size());
        for (std::size_t TrackIndex = 0; TrackIndex < Tracks.size(); ++TrackIndex)
        {
            const TJson& JsonTrack = Tracks[TrackIndex];
            const std::string Context = "tracks[" + std::to_string(TrackIndex) + "]";
            TDemoAnimationTrack Track;
            Track.Target = String(JsonTrack, "target", Context);
            const std::string PropertyName = String(JsonTrack, "property", Context);
            Track.Property = Property(Track.Target, PropertyName);
            if (Track.Target != "camera")
            {
                const auto& Objects = Scene.GetStaticMeshInstances();
                const bool bFound = std::any_of(Objects.begin(), Objects.end(),
                    [&Track](const TStaticMeshInstance& Object) { return Object.GetId() == Track.Target; });
                if (!bFound) throw std::runtime_error(Context + " targets an unknown Scene object ID.");
            }
            if (!UsedProperties.insert(Track.Target + "\n" + PropertyName).second)
                throw std::runtime_error(Context + " duplicates an earlier target/property track.");
            const bool bRotation = Track.Property == EDemoAnimationProperty::Rotation;
            const std::string Interpolation = JsonTrack.contains("interpolation")
                ? String(JsonTrack, "interpolation", Context) : (bRotation ? "slerp" : "linear");
            if (Interpolation == "step") Track.Interpolation = EDemoAnimationInterpolation::Step;
            else if (Interpolation == "linear" && !bRotation)
                Track.Interpolation = EDemoAnimationInterpolation::Linear;
            else if (Interpolation == "slerp" && bRotation)
                Track.Interpolation = EDemoAnimationInterpolation::Slerp;
            else throw std::runtime_error(Context + " uses an unsupported interpolation for its property.");
            const TJson& Keys = Required(JsonTrack, "keys", Context);
            if (!Keys.is_array() || Keys.empty()) throw std::runtime_error(Context + ".keys must be non-empty.");
            Track.Keys.reserve(Keys.size());
            for (std::size_t KeyIndex = 0; KeyIndex < Keys.size(); ++KeyIndex)
            {
                const std::string KeyContext = Context + ".keys[" + std::to_string(KeyIndex) + "]";
                const TJson& Key = Keys[KeyIndex];
                const float Time = Float(Required(Key, "time", KeyContext), KeyContext + ".time");
                if (Time < 0.0F || Time > Clip.DurationSeconds ||
                    (!Track.Keys.empty() && Time <= Track.Keys.back().Time) ||
                    (Track.Keys.empty() && Time != 0.0F))
                    throw std::runtime_error(KeyContext + ".time must start at 0 and increase within duration.");
                Track.Keys.push_back({Time, ReadValue(Required(Key, "value", KeyContext), bRotation,
                                                      KeyContext + ".value")});
            }
            Clip.Tracks.push_back(std::move(Track));
        }
        return Clip;
    }

    void ApplyDemoAnimation(TScene& Scene, const TDemoAnimationClip& Clip, float Time)
    {
        for (const TDemoAnimationTrack& Track : Clip.Tracks)
        {
            const glm::vec4 Value = Sample(Track, Time);
            if (Track.Target == "camera")
            {
                if (Track.Property == EDemoAnimationProperty::CameraPosition)
                    Scene.GetMainCamera().SetPosition(glm::vec3(Value));
                else
                    Scene.GetMainCamera().SetTarget(glm::vec3(Value));
                continue;
            }
            for (TStaticMeshInstance& Object : Scene.GetStaticMeshInstances())
            {
                if (Object.GetId() != Track.Target) continue;
                TTransform& Transform = Object.GetTransform();
                switch (Track.Property)
                {
                    case EDemoAnimationProperty::Position: Transform.Position = glm::vec3(Value); break;
                    case EDemoAnimationProperty::Rotation:
                        Transform.RotationDegrees = QuaternionToTransformEuler(Value); break;
                    case EDemoAnimationProperty::Scale: Transform.Scale = glm::vec3(Value); break;
                    case EDemoAnimationProperty::CameraPosition:
                    case EDemoAnimationProperty::CameraTarget: break;
                }
                break;
            }
        }
    }
} // namespace MDSS
