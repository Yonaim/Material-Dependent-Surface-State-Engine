#pragma once
// Research coordinator. It does not call FSurfaceStateSolver::RecordStep(s).
// Numeric provider bodies, descriptor ABI, reaction and geometry services are deliberately shared.
#include "ShaderCatalog.h"
#include "SurfaceState/Solver/SurfaceStateSolver.h"
#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

// ============================================================
// 00. Module Contracts
// ============================================================
#pragma region 00_Module_Contracts

namespace MDSS::Research
{
    using namespace SurfaceState;
    struct FPhenomenonModule
    {
        std::string StateKey;
        unsigned    Channel = 0;
        bool        bHeightContributor = false;

        // All v1 phenomena elect the common LocalFlux provider. No state-name dispatch exists here.
    };

#pragma endregion

// ============================================================
// 01. Module Selection
// ============================================================
#pragma region 01_Module_Selection

    inline std::vector<FPhenomenonModule> MakeLocalFluxModules(const FSurfaceStateRegistry&       Registry,
                                                               const FSurfaceResponseProfileData& Profile,
                                                               bool                               bReverse)
    {
        std::vector<FPhenomenonModule> Modules;

        for (unsigned C = 0; C < Registry.GetStateCount(); ++C)
        {
            auto Key = Registry.GetStateName(C);
            Modules.push_back({Key, C, Profile.States.at(Key).AccumulationFactor > 0});
        }

        if (bReverse)
            std::reverse(Modules.begin(), Modules.end());
        return Modules;
    }

#pragma endregion

// ============================================================
// 02. Compute Provider Ownership and Recording
// ============================================================
#pragma region 02_Compute_Provider_Ownership_and_Recording

    class FModularCoordinator
    {
        VkDevice         Device;
        VkPipelineLayout Layout = VK_NULL_HANDLE;
        VkPipeline       Pass1 = VK_NULL_HANDLE, Pass2 = VK_NULL_HANDLE, Reaction = VK_NULL_HANDLE;
        VkPipeline Height = VK_NULL_HANDLE, Geometry = VK_NULL_HANDLE, Weights = VK_NULL_HANDLE, Reset = VK_NULL_HANDLE;
        std::vector<FPhenomenonModule> Modules;
        bool                           bSeparate;

        VkPipeline Load(const std::string& Path)
        {
            std::ifstream F(Path, std::ios::binary | std::ios::ate);

            if (!F)
                throw std::runtime_error("Missing research shader: " + Path);
            auto                       Size = F.tellg();
            std::vector<std::uint32_t> Code(std::size_t(Size) / 4);
            F.seekg(0);
            F.read(reinterpret_cast<char*>(Code.data()), Size);
            VkShaderModuleCreateInfo SI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            SI.codeSize = Code.size() * 4;
            SI.pCode = Code.data();
            VkShaderModule Module{};

            if (vkCreateShaderModule(Device, &SI, nullptr, &Module) != VK_SUCCESS)
                throw std::runtime_error("Module shader creation failed");
            VkComputePipelineCreateInfo PI{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            PI.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            PI.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            PI.stage.module = Module;
            PI.stage.pName = "main";
            PI.layout = Layout;
            VkPipeline P{};
            auto       Result = vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PI, nullptr, &P);
            vkDestroyShaderModule(Device, Module, nullptr);

            if (Result != VK_SUCCESS)
                throw std::runtime_error("Module pipeline creation failed");
            return P;
        }

        static void Barrier(VkCommandBuffer C)
        {
            VkMemoryBarrier B{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            B.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            B.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(C,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 1,
                                 &B,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr);
        }

        void Dispatch(VkCommandBuffer                    C,
                      VkPipeline                         P,
                      VkDescriptorSet                    Set,
                      const FSurfaceSolverPushConstants& PC,
                      unsigned                           Groups)
        {
            vkCmdBindPipeline(C, VK_PIPELINE_BIND_POINT_COMPUTE, P);
            vkCmdBindDescriptorSets(C, VK_PIPELINE_BIND_POINT_COMPUTE, Layout, 0, 1, &Set, 0, nullptr);
            vkCmdPushConstants(C, Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
            vkCmdDispatch(C, Groups, 1, 1);
        }

        static FSurfaceSolverPushConstants
        Constants(std::size_t N, std::size_t K, float Dt, const glm::mat4& Model, glm::vec3 Gravity, unsigned Flags)
        {
            FSurfaceSolverPushConstants PC{};
            PC.DeltaTime = Dt;
            PC.LocalTexelCount = unsigned(N);
            PC.StateChannelCount = unsigned(K);
            PC.Flags = Flags;
            PC.GravityWorld = {Gravity.x, Gravity.y, Gravity.z, 0};
            glm::mat3 Linear(Model), Normal(0);
            glm::vec3 Up(0);

            if (std::isfinite(glm::determinant(Linear)) && std::abs(glm::determinant(Linear)) > 1e-6F)
                Normal = glm::transpose(glm::inverse(Linear));
            if ((Flags & 1U) == 0 && std::isfinite(glm::length(Gravity)) && glm::length(Gravity) > 1e-6F)
                Up = -Gravity / glm::length(Gravity);
            for (unsigned Col = 0; Col < 3; ++Col)
            {
                for (unsigned Row = 0; Row < 3; ++Row)
                {
                    PC.ModelLinearColumns[Col][Row] = Linear[Col][Row];
                    PC.NormalMatrixAndUpColumns[Col][Row] = Normal[Col][Row];
                }

                PC.NormalMatrixAndUpColumns[Col][3] = Up[Col];
            }

            return PC;
        }

    public:
        FModularCoordinator(VkDevice                       D,
                            VkDescriptorSetLayout          SetLayout,
                            std::vector<FPhenomenonModule> Registered,
                            bool                           bSeparateSchedule)
            : Device(D), Modules(std::move(Registered)), bSeparate(bSeparateSchedule)
        {
            if (Modules.empty() || Modules.size() > 7)
                throw std::invalid_argument("Research module selector supports 1..7 channels");
            std::vector<bool> Seen(Modules.size(), false);

            for (auto& M : Modules)
            {
                if (M.Channel >= Seen.size() || Seen[M.Channel])
                    throw std::invalid_argument("Module channel ownership must be unique and complete");
                Seen[M.Channel] = true;
            }

            VkPushConstantRange        Range{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FSurfaceSolverPushConstants)};
            VkPipelineLayoutCreateInfo LI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            LI.setLayoutCount = 1;
            LI.pSetLayouts = &SetLayout;
            LI.pushConstantRangeCount = 1;
            LI.pPushConstantRanges = &Range;

            if (vkCreatePipelineLayout(Device, &LI, nullptr, &Layout) != VK_SUCCESS)
                throw std::runtime_error("Module layout creation failed");
            // A full registered LocalFlux batch can reuse the original all-channel provider directly.
            // Only the diagnostic separate schedule needs a runtime channel selector.
            Pass1 =
                Load(bSeparate ? std::string(MDSS_MODULAR_SHADER_DIR) + "/ModulePass1.spv"
                               : Shaders::Path(Shaders::EShader::Simulation_Solver_SurfaceSolverPass1_comp_runtime));
            Pass2 =
                Load(bSeparate ? std::string(MDSS_MODULAR_SHADER_DIR) + "/ModulePass2.spv"
                               : Shaders::Path(Shaders::EShader::Simulation_Solver_SurfaceSolverPass2_comp_runtime));
            Reaction = Load(Shaders::Path(Shaders::EShader::Simulation_Solver_SurfaceReaction_comp));
            Height = Load(Shaders::Path(Shaders::EShader::Simulation_Geometry_SurfaceAccumulationHeight_comp));
            Geometry = Load(Shaders::Path(Shaders::EShader::Simulation_Geometry_SurfaceDynamicGeometryUpdate_comp));
            Weights =
                Load(Shaders::Path(Shaders::EShader::Simulation_Geometry_SurfaceDynamicWeightsUpdate_comp_runtime));
            Reset = Load(Shaders::Path(Shaders::EShader::Simulation_Scheduling_SurfaceSparseScheduleReset_comp));
        }

        ~FModularCoordinator()
        {
            for (auto P : {Pass1, Pass2, Reaction, Height, Geometry, Weights, Reset})
                if (P)
                    vkDestroyPipeline(Device, P, nullptr);
            if (Layout)
                vkDestroyPipelineLayout(Device, Layout, nullptr);
        }

        void Record(VkCommandBuffer                         C,
                    const FSurfaceStateDescriptorResources& D,
                    bool                                    bAB,
                    std::size_t                             N,
                    std::size_t                             K,
                    float                                   Dt,
                    const glm::mat4&                        Model,
                    glm::vec3                               Gravity,
                    unsigned                                Flags,
                    VkQueryPool                             Q = VK_NULL_HANDLE)
        {
            if (K != Modules.size() ||
                (Flags & (SurfaceSolverSparseSolverFlag | SurfaceSolverSparseGeometryFlag |
                          SurfaceSolverSparseAccumulationHeightFlag | SurfaceSolverActiveChannelMaskFlag)))
                throw std::invalid_argument("v1 modular coordinator is a complete dense channel batch");
            Flags |= SurfaceSolverPrepareAccumulationHeightFlag;

            if (bAB)
                Flags |= SurfaceSolverCurrentStateABFlag;
            else
                Flags &= ~SurfaceSolverCurrentStateABFlag;
            if (D.HasReactions())
                Flags |= SurfaceSolverReactionFlag;
            auto       PC = Constants(N, K, Dt, Model, Gravity, Flags);
            const auto Current = bAB ? D.GetABSet() : D.GetBASet(), Next = bAB ? D.GetBASet() : D.GetABSet();
            unsigned   Groups = unsigned((N + 63) / 64);
            bool       bUpdate =
                (Flags & SurfaceSolverStateDrivenGeometryUpdateFlag) && !(Flags & SurfaceSolverFreezeGeometryFlag);
            bool bPrebuild = bUpdate && (Flags & SurfaceSolverForceFullGeometryFlag);
            auto TS = [&](unsigned I)
            {
                if (Q)
                    vkCmdWriteTimestamp(C, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, Q, I);
            };
            Barrier(C);

            if (bPrebuild)
            {
                Dispatch(C, Height, Current, PC, Groups);
                Barrier(C);
                Dispatch(C, Geometry, Current, PC, Groups);
                Barrier(C);
                Dispatch(C, Weights, Current, PC, Groups);
                Barrier(C);
            }

            TS(4);

            if (bUpdate)
            {
                Dispatch(C, Reset, Current, PC, 1);
                Barrier(C);
            }

            auto Providers = [&](VkPipeline P)
            {
                if (!bSeparate)
                {
                    Dispatch(C, P, Current, PC, Groups);
                    return;
                }

                for (auto& M : Modules)
                {
                    auto Selected = PC;
                    Selected.Flags |= (M.Channel + 1U) << 29U;
                    Dispatch(C, P, Current, Selected, Groups);
                }
            };
            Providers(Pass1);
            TS(5);
            Barrier(C);
            TS(6);
            Providers(Pass2);
            Barrier(C);

            if (D.HasReactions())
            {
                Dispatch(C, Reaction, Current, PC, Groups);
                Barrier(C);
            }

            TS(7);
            TS(0);
            auto Post = PC;

            if (bPrebuild)
                Post.Flags &= ~SurfaceSolverForceFullGeometryFlag;
            Dispatch(C, Height, Next, Post, Groups);
            Barrier(C);

            if (bUpdate)
            {
                Dispatch(C, Geometry, Next, Post, Groups);
                Barrier(C);
            }

            TS(1);
            TS(2);

            if (bUpdate)
            {
                Dispatch(C, Weights, Next, Post, Groups);
                Barrier(C);
            }

            TS(3);
        }
    };
}

#pragma endregion
