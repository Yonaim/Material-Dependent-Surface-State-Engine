// Headless, explicitly invoked research harness. No renderer, wall-clock stepping or demo drain frames.
// ============================================================
// 00. Research Dependencies
// ============================================================
#pragma region 00_Research_Dependencies

#include "ResearchVulkanDevice.h"
#include "ModularBaseline.h"
#include "AssetManager/Loaders/OBJLoader.h"
#include "AssetManager/Loaders/SRProfileLoader.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "SurfaceState/Mapping/SurfaceMappingBuilder.h"
#include "SurfaceState/Geometry/SurfaceGeometryBuilder.h"
#include "SurfaceState/Geometry/MesoGeometryBuilder.h"
#include "SurfaceState/Solver/SurfaceStateSolver.h"
#include <nlohmann/json.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <chrono>

using namespace MDSS::SurfaceState;
using FJson = nlohmann::json;
namespace fs = std::filesystem;

#pragma endregion

// ============================================================
// 01. GPU State Readback Helpers
// ============================================================
#pragma region 01_GPU_State_Readback_Helpers

std::vector<float> Read(const MDSS::GPU::FGPUBuffer& B)
{
    std::vector<float> V(B.GetSize() / sizeof(float));
    B.Download(V.data(), B.GetSize());
    return V;
}

void Save(const fs::path& P, const std::vector<float>& V)
{
    std::ofstream F(P, std::ios::binary);
    F.write(reinterpret_cast<const char*>(V.data()), V.size() * sizeof(float));

    if (!F)
        throw std::runtime_error("Cannot save " + P.string());
}

std::string Hash(const std::vector<float>& V)
{
    std::uint64_t H = 14695981039346656037ULL;

    for (float X : V)
        for (unsigned S = 0; S < 32; S += 8)
        {
            H ^= (std::bit_cast<std::uint32_t>(X) >> S) & 255;
            H *= 1099511628211ULL;
        }

    return std::to_string(H);
}

double Gaussian(float U, float V, float CU, float CV, float RU, float RV)
{
    return std::exp(-std::pow((U - CU) / RU, 2) - std::pow((V - CV) / RV, 2));
}

#include "MesoStudy.h"

#pragma endregion

// ============================================================
// 02. Experiment Orchestration
// ============================================================
#pragma region 02_Experiment_Orchestration

int main(int argc, char** argv)
try
{
    if (argc != 2)
        throw std::runtime_error("Usage: MDSS_CouplingStudy plan.json");
    FJson Plan;
    std::ifstream(argv[1]) >> Plan;
    fs::path Out = Plan.at("output").get<std::string>();

    if (fs::exists(Out / "summary.json") || fs::exists(Out / "series.jsonl"))
        throw std::runtime_error("Refusing to overwrite a run");
    fs::create_directories(Out);
    std::ofstream(Out / "plan.json") << Plan.dump(2);
    const std::string Case = Plan.at("case"), Variant = Plan.at("variant");

    if (Variant.size() != 1 || std::string("ABCDEF").find(Variant) == std::string::npos)
        throw std::runtime_error("Invalid variant");
    const bool        bCoupled = Variant != "A" && Variant != "B", bFrozen = Variant == "E",
                      bReact = Variant != "A" && Variant != "C";
    const bool        bLedger = Plan.value("ledger", true), bFull = Plan.value("full_geometry", true),
                      bSparse = Plan.value("sparse_solver", false);
    const bool        bTiming = Plan.value("timing_only", false), bReverse = Plan.value("reverse_modules", false);
    const std::string Implementation = Plan.value("implementation", std::string("U"));

    if (Implementation != "U" && Implementation != "M" && Implementation != "M-separate")
        throw std::runtime_error("Unknown implementation");
    const unsigned Resolution = Plan.at("resolution"), Hz = Plan.at("hz");

    if (Hz % 4 || Hz == 0)
        throw std::runtime_error("hz must be positive and divisible by four");
    const unsigned Steps = unsigned(Plan.at("seconds").get<double>() * Hz);
    const float    Dt = 1.0F / Hz;
    auto           P = MDSS::Asset::FSRProfileLoader::Load(0, Plan.at("profile").get<std::string>())->GetData();

    if (Case == "mud")
    {
        auto Mud = MDSS::Asset::FSRProfileLoader::Load(1, "Assets/SurfaceProfiles/DemoMud.SRProfile")
                       ->GetData()
                       .States.at("mud");
        // Predeclared flat-meso study: no chart-dependent cavity factor in either implementation.
        Mud.CavityFillFactor = 0;
        Mud.CavityRetentionFactor = 0;
        Mud.CavityTransportRetentionFactor = 0;
        P.States["mud"] = Mud;
    }

    if (Plan.contains("add_states") || Plan.contains("append_reactions"))
    {
        FJson Effective;
        std::ifstream(Plan.at("profile").get<std::string>()) >> Effective;

        if (Case == "mud")
        {
            FJson MP;
            std::ifstream("Assets/SurfaceProfiles/DemoMud.SRProfile") >> MP;
            auto Mud = MP["states"]["mud"];

            for (auto Key : {"cavityFillFactor", "cavityDecayProtectionFactor", "cavityExitResistanceFactor"})
                Mud[Key] = 0;
            Effective["states"]["mud"] = Mud;
        }

        if (Plan.contains("add_states"))
            for (auto It = Plan["add_states"].begin(); It != Plan["add_states"].end(); ++It)
                Effective["states"][It.key()] = It.value();
        if (Plan.contains("append_reactions"))
            for (auto& R : Plan["append_reactions"])
                Effective["reactions"].push_back(R);
        std::ofstream(Out / "effective-profile.SRProfile") << Effective.dump(2);
        P = MDSS::Asset::FSRProfileLoader::Load(0, (Out / "effective-profile.SRProfile").string())->GetData();
    }

    if (!bReact)
        for (auto& R : P.Reactions)
            R.bEnabled = false;
    if (Variant == "F")
        P.States.at("ice").AccumulationFactor = 0;
    FSurfaceStateRegistry    Registry({P});
    const std::size_t        K = Registry.GetStateCount();
    const auto               Ice = Registry.GetStateId("ice"), Water = Registry.GetStateId("waterfilm"),
                             Heat = Registry.GetStateId("heat");
    std::vector<std::string> Names;

    for (unsigned C = 0; C < K; ++C)
        Names.push_back(Registry.GetStateName(C));
    auto     Mesh = MDSS::Asset::FOBJLoader::Load(Plan.at("mesh").get<std::string>());
    unsigned SurfaceCount = 0;

    for (auto& S : Mesh.Sections)
        SurfaceCount = std::max(SurfaceCount, S.Surface + 1);
    std::vector<FSurfaceDefinition> Definitions;

    for (unsigned S = 0; S < SurfaceCount; ++S)
        Definitions.push_back({S, {Resolution, Resolution}});
    auto Mapping = FSurfaceMappingBuilder::Build(Mesh.Vertices, Mesh.Triangles, Definitions);
    std::vector<FSurfaceProfileIndex> ProfileMap(Mapping.Texels.size(), InvalidSurfaceProfileIndex);

    for (std::size_t I = 0; I < Mapping.Texels.size(); ++I)
        if (Mapping.Texels[I].IsValid())
            ProfileMap[I] = 0;
    auto Geometry = FSurfaceGeometryBuilder::Build(Mapping, std::move(ProfileMap), 1);

    // Flat remains the historical default. New meso inputs use the production sampler/integrator.
    const auto Meso = ConfigureMesoStudy(Geometry, Mesh.Vertices, Mesh.Triangles, Plan, Out);

    if (Plan.value("geometry_only", false))
    {
        std::ofstream(Out / "summary.json")
            << FJson({{"geometry_only", true}, {"meso", Meso}, {"mapping_warnings", Mapping.Warnings}}).dump(2);
        return 0;
    }

    const auto      N = Geometry.GetTexelCount();
    auto&           Texels = Geometry.GetTexels();
    const auto      Scale = Plan.at("scale"), Rotation = Plan.at("rotation_x_degrees");
    const glm::mat4 Model =
        glm::scale(glm::rotate(glm::mat4(1), glm::radians(Rotation.get<float>()), glm::vec3(1, 0, 0)),
                   glm::vec3(Scale[0], Scale[1], Scale[2]));
    auto               Areas = BuildSurfaceGPUWorldTexelAreas(Geometry, Model);
    auto               Weights = BuildSurfaceGPUTransferWeights(Geometry, Model);
    std::vector<float> U(N), V(N), Active(N), Initial(N * K, 0), Positions(N * 3), AreaScale(N), Neighbors(N * 8);
    glm::vec3          Min(1e30F), Max(-1e30F);

    for (auto& T : Texels)
        if (T.IsValid())
        {
            auto W = glm::vec3(Model * glm::vec4(T.Position, 1));
            Min = glm::min(Min, W);
            Max = glm::max(Max, W);
        }

    const glm::mat3 NM = glm::transpose(glm::inverse(glm::mat3(Model)));
    std::size_t     Valid = 0;

    for (std::size_t I = 0; I < N; ++I)
    {
        auto& T = Texels[I];

        for (unsigned D = 0; D < 8; ++D)
            Neighbors[I * 8 + D] = T.NeighborIndices[D] == InvalidTexelIndex ? -1.0F : float(T.NeighborIndices[D]);
        if (!T.IsValid())
            continue;
        ++Valid;
        auto W = glm::vec3(Model * glm::vec4(T.Position, 1));

        for (int J = 0; J < 3; ++J)
            Positions[I * 3 + J] = W[J];
        AreaScale[I] = Areas[I] / SurfaceStateReferenceArea;
        U[I] = Case == "bunny" ? (W.x - Min.x) / (Max.x - Min.x) - .5F : T.Position.x;
        V[I] = Case == "bunny" ? (W.z - Min.z) / (Max.z - Min.z) - .5F : T.Position.y;
        Active[I] = Case == "bunny" ? float(glm::normalize(NM * T.Normal).z > .15F) : float(T.Normal.z > .9F);

        if (!Active[I])
            continue;
        const double Density = 3.5 * std::exp(-std::pow(U[I] / .30, 4) - std::pow(V[I] / .10, 4));
        Initial[I * K + Ice] = float(Density * AreaScale[I]);

        if (Case == "mud")
            Initial[I * K + Registry.GetStateId("mud")] =
                float(1.5 * Gaussian(U[I], V[I], .10F, -.12F, .18F, .08F) * AreaScale[I]);
    }

    Save(Out / "initial.f32", Initial);
    Save(Out / "positions.f32", Positions);
    Save(Out / "areas.f32", Areas);
    Save(Out / "u.f32", U);
    Save(Out / "v.f32", V);
    Save(Out / "patch.f32", Active);
    Save(Out / "neighbors.f32", Neighbors);
    FVulkanTestDevice          GPU;
    VkPhysicalDeviceProperties DeviceProperties{};
    vkGetPhysicalDeviceProperties(GPU.GetPhysicalDevice(), &DeviceProperties);
    FSurfaceSharedGeometryGPUResources Shared(GPU.GetPhysicalDevice(), GPU.GetDevice(), Geometry);
    FSurfaceProfileGPUResources        Profiles(GPU.GetPhysicalDevice(), GPU.GetDevice(), {P}, Registry);
    const unsigned                     M = Profiles.GetMaxReactionCount();
    FSurfaceInstanceGPUResources       Instance(
        GPU.GetPhysicalDevice(), GPU.GetDevice(), N, K, Weights, {}, Areas, nullptr, {}, M, bLedger);
    FSurfaceStateDescriptorResources Descriptors(GPU.GetDevice(), Shared, Profiles, Instance);
    auto                             Modules = MDSS::Research::MakeLocalFluxModules(Registry, P, bReverse);
    FJson                            ModuleList = FJson::array();

    for (auto& Module : Modules)
        ModuleList.push_back({{"state", Module.StateKey},
                              {"channel", Module.Channel},
                              {"height_contributor", Module.bHeightContributor},
                              {"provider", "LocalFlux"}});
    std::ofstream(Out / "modules.json") << ModuleList.dump(2);
    std::unique_ptr<FSurfaceStateSolver>                 Solver;
    std::unique_ptr<MDSS::Research::FModularCoordinator> Modular;

    if (Implementation == "U")
        Solver = std::make_unique<FSurfaceStateSolver>(GPU.GetDevice(), Descriptors.GetLayout());
    else
        Modular = std::make_unique<MDSS::Research::FModularCoordinator>(
            GPU.GetDevice(), Descriptors.GetLayout(), Modules, Implementation == "M-separate");
    Instance.GetStateABuffer().Upload(Initial.data(), Initial.size() * sizeof(float));
    bool                bAB = true;
    const std::uint32_t Base = SurfaceSolverRawFluxCacheFlag | SurfaceSolverDistanceWeightFlag |
                               SurfaceSolverNormalWeightFlag | SurfaceSolverProfileBoundaryWeightFlag |
                               (bCoupled ? SurfaceSolverStateDrivenGeometryUpdateFlag : 0U) |
                               (bSparse ? SurfaceSolverSparseSolverFlag : 0U);
    std::vector<double> RecordTimes;
    auto                Execute = [&](float StepDt, std::uint32_t Extra, VkQueryPool Queries = VK_NULL_HANDLE)
    {
        GPU.Execute(
            [&](VkCommandBuffer C)
            {
                if (Queries != VK_NULL_HANDLE)
                    vkCmdResetQueryPool(C, Queries, 0, 8);
                const auto Start = std::chrono::steady_clock::now();
                if (Solver)
                    Solver->RecordStep(C, Descriptors, bAB, N, K, StepDt, Model, {0, 0, -1}, Base | Extra, Queries);
                else
                    Modular->Record(C, Descriptors, bAB, N, K, StepDt, Model, {0, 0, -1}, Base | Extra, Queries);
                if (Queries)
                    RecordTimes.push_back(
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count());
            });
        bAB = !bAB;
    };

    // A zero-time initialization builds G0 without evaluating a finite reaction interval.
    Execute(0, SurfaceSolverForceFullGeometryFlag | SurfaceSolverSeedPersistentActivityFlag);
    auto   Before = Read(bAB ? Instance.GetStateABuffer() : Instance.GetStateBBuffer());
    double BootstrapError = 0;

    for (std::size_t J = 0; J < Initial.size(); ++J)
        BootstrapError = std::max(BootstrapError, std::abs(double(Before[J]) - Initial[J]));
    Save(Out / "bootstrap.f32", Before);
    std::cout << "Bootstrap maximum absolute difference: " << BootstrapError << std::endl;

    if (BootstrapError > 1e-10)
        throw std::runtime_error("Bootstrap changed S0 above denormal tolerance");
    // Vulkan float32 may flush subnormal Gaussian tails. Use the common post-bootstrap S0 for all budgets.
    Initial = Before;
    auto G0 = Read(Instance.GetDynamicGeometryBuffer()), W0 = Read(Instance.GetTransferWeightBuffer()),
         C0 = Read(Instance.GetDynamicConcavityWeightBuffer());
    Save(Out / "geometry-0.f32", G0);
    Save(Out / "weights-0.f32", W0);
    Save(Out / "height-0.f32", Read(Instance.GetAccumulationHeightBuffer()));
    Save(Out / "state-0.f32", Before);
    Save(Out / "concavity-0.f32", C0);
    auto BuildInput = [&](unsigned Step)
    {
        std::vector<float> Input(N * K, 0);

        if (Step % (Hz / 4) == 0 && Step < 12 * Hz)
            for (std::size_t I = 0; I < N; ++I)
                if (Active[I])
                {
                    Input[I * K + Heat] = float(.2 * Gaussian(U[I], V[I], -.08F, .02F, .16F, .14F) * AreaScale[I]);
                    Input[I * K + Water] = float(.15 * Gaussian(U[I], V[I], 0, .30F, .18F, .08F) * AreaScale[I]);
                }

        return Input;
    };

    if (bTiming)
    {
        // One-second scratch trajectory; then restore every state/cache/schedule, not just displayed time.
        for (unsigned Step = 0; Step < Hz; ++Step)
        {
            auto Input = BuildInput(Step);
            Instance.GetInputDeltaBuffer().Upload(Input.data(), Input.size() * sizeof(float));
            Execute(Dt, SurfaceSolverEveryStepGeometryFlag);
        }

        auto Zero = [](const MDSS::GPU::FGPUBuffer& B)
        {
            std::vector<float> Z(B.GetSize() / 4, 0);
            B.Upload(Z.data(), B.GetSize());
        };

        for (auto* B : {&Instance.GetStateABuffer(),
                        &Instance.GetStateBBuffer(),
                        &Instance.GetInputDeltaBuffer(),
                        &Instance.GetRawOutgoingBuffer(),
                        &Instance.GetOutgoingFluxScaleBuffer(),
                        &Instance.GetReactionScratchBuffer(),
                        &Instance.GetDynamicGeometryBuffer(),
                        &Instance.GetDynamicConcavityWeightBuffer(),
                        &Instance.GetAccumulationHeightBuffer()})
            Zero(*B);
        Instance.GetTransferWeightBuffer().Upload(W0.data(), W0.size() * 4);
        Instance.GetStateABuffer().Upload(Initial.data(), Initial.size() * 4);
        bAB = true;
        Execute(0, SurfaceSolverForceFullGeometryFlag | SurfaceSolverSeedPersistentActivityFlag);

        if (Read(bAB ? Instance.GetStateABuffer() : Instance.GetStateBBuffer()) != Before ||
            Read(Instance.GetDynamicGeometryBuffer()) != G0 || Read(Instance.GetTransferWeightBuffer()) != W0 ||
            Read(Instance.GetDynamicConcavityWeightBuffer()) != C0)
            throw std::runtime_error("Warmup reset did not reproduce S0/G0/weights/concavity");
    }

    VkQueryPool           Query{};
    VkQueryPoolCreateInfo QI{};
    QI.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    QI.queryType = VK_QUERY_TYPE_TIMESTAMP;
    QI.queryCount = 8;

    if (vkCreateQueryPool(GPU.GetDevice(), &QI, nullptr, &Query) != VK_SUCCESS)
        throw std::runtime_error("Timestamp query creation failed");
    std::ofstream       Series(Out / "series.jsonl");
    std::ofstream       TimingSeries(Out / "timings.jsonl");
    std::vector<double> CumInput(K, 0), CumDecay(K, 0), CumExtent(M, 0), CumCross(K, 0), Times;
    double              MaxLocalResidual = 0, MaxGlobalResidual = 0, MaxMassResidual = 0;
    std::uint64_t       Nonfinite = 0, Negative = 0, Budget = 0, Arithmetic = 0, FrozenFailures = 0;
    const bool          bHasWetness = P.States.contains("wetness");
    const auto          Wetness = bHasWetness ? Registry.GetStateId("wetness") : InvalidStateId;
    const double        InitialMass = std::accumulate(Initial.begin(),
                                                      Initial.end(),
                                                      0.0,
                                                      [&, index = std::size_t(0)](double Sum, float X) mutable
                                                      {
                                                   auto C = index++ % K;
                                                   return Sum + ((C == Ice || C == Water || C == Wetness) ? X : 0);
                                                      });
    std::vector<std::vector<double>> Stoich(M, std::vector<double>(K, 0));
    unsigned                         RIndex = 0;

    for (auto& R : P.Reactions)
        if (R.bEnabled)
        {
            for (auto& T : R.Consumes)
                Stoich[RIndex][Registry.GetStateId(T.State)] -= T.Coefficient;
            for (auto& T : R.Produces)
                Stoich[RIndex][Registry.GetStateId(T.State)] += T.Coefficient;
            ++RIndex;
        }

    const auto         Stride = 5 * K + M + 4;
    const auto         SparseLayout = GetSurfaceSparseMetadataLayout(N);
    std::vector<float> DecayRates;

    for (const auto& Name : Names)
        DecayRates.push_back(P.States.at(Name).DecayRate);
    for (unsigned Step = 0; Step < Steps; ++Step)
    {
        auto Input = BuildInput(Step);
        Instance.GetInputDeltaBuffer().Upload(Input.data(), Input.size() * sizeof(float));
        const bool         bSeed = bSparse && Step % (Hz / 4) == 0 && Step < 12 * Hz;
        std::vector<float> ExecutedGroups(SparseLayout.WorkgroupCount, 1);

        if (bSparse && !bSeed)
        {
            std::fill(ExecutedGroups.begin(), ExecutedGroups.end(), 0);
            auto       Scheduling = Read(Instance.GetAccumulationHeightBuffer());
            const auto Command = bAB ? SparseLayout.SolverACommandWord : SparseLayout.SolverBCommandWord;
            const auto List = bAB ? SparseLayout.SolverAListWord : SparseLayout.SolverBListWord;
            const auto Count = std::bit_cast<std::uint32_t>(Scheduling[Command]);

            if (Count > SparseLayout.WorkgroupCount)
                throw std::runtime_error("Invalid active group count");
            for (unsigned G = 0; G < Count; ++G)
                ExecutedGroups.at(std::bit_cast<std::uint32_t>(Scheduling[List + G])) = 1;
        }

        // External analytic injections have no CPU activation list: conservatively seed the full grid on event steps.
        const std::uint32_t Extra = (bFrozen ? SurfaceSolverFreezeGeometryFlag : 0U) |
                                    (bSeed ? SurfaceSolverSeedPersistentActivityFlag : 0U) |
                                    ((bFull && !bFrozen) ? SurfaceSolverEveryStepGeometryFlag : 0U) |
                                    ((bLedger && M) ? SurfaceSolverReactionLedgerFlag : 0U);
        Execute(Dt, Extra, Query);
        std::uint64_t TS[8]{};

        if (vkGetQueryPoolResults(GPU.GetDevice(),
                                  Query,
                                  0,
                                  8,
                                  sizeof(TS),
                                  TS,
                                  sizeof(std::uint64_t),
                                  VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS)
            throw std::runtime_error("Timestamp readback failed");
        const double GpuMs = double(TS[3] - TS[4]) * DeviceProperties.limits.timestampPeriod / 1e6;
        Times.push_back(GpuMs);
        TimingSeries << FJson({{"step", Step + 1},
                               {"time", double(Step + 1) / Hz},
                               {"gpu_ms", GpuMs},
                               {"record_cpu_ms", RecordTimes.back()}})
                            .dump()
                     << '\n';
        if (bTiming)
        {
            // Timed runs upload events but perform no per-step state/scratch/geometry readback.
            if (Step + 1 == Steps)
                Before = Read(bAB ? Instance.GetStateABuffer() : Instance.GetStateBBuffer());
            continue;
        }

        auto After = Read(bAB ? Instance.GetStateABuffer() : Instance.GetStateBBuffer());
        auto Raw = Read(Instance.GetRawOutgoingBuffer()), Alpha = Read(Instance.GetOutgoingFluxScaleBuffer());
        auto Scratch = M ? Read(Instance.GetReactionScratchBuffer()) : std::vector<float>{};
        auto Dynamic = Read(Instance.GetDynamicGeometryBuffer());

        if (bFrozen && (Dynamic != G0 || Read(Instance.GetTransferWeightBuffer()) != W0 ||
                        Read(Instance.GetDynamicConcavityWeightBuffer()) != C0))
            ++FrozenFailures;
        std::vector<double> Net(N * K, 0), Decay(N * K, 0), Sums(K, 0), Downstream(K, 0), StepExtents(M, 0),
            Residual(K, 0);
        for (std::size_t I = 0; I < N; ++I)
            if (Texels[I].IsValid())
            {
                if (!ExecutedGroups[I / 64])
                    continue; // Inactive scratch contains previous-step data, not new extents/flux.
                if (M)
                {
                    Budget += Scratch[I * Stride + Stride - 2] != 0;
                    Arithmetic += Scratch[I * Stride + Stride - 1] != 0;

                    if (bLedger)
                        for (unsigned R = 0; R < M; ++R)
                            StepExtents[R] += Scratch[N * Stride + I * M + R];
                }

                bool bEmpty = true;

                for (unsigned C = 0; C < K; ++C)
                    if (Before[I * K + C] != 0 || Input[I * K + C] != 0 || After[I * K + C] != 0)
                    {
                        bEmpty = false;
                        break;
                    }

                if (bEmpty)
                    continue;
                for (unsigned C = 0; C < K; ++C)
                {
                    const auto J = I * K + C;
                    CumInput[C] += Input[J];

                    // All study profiles have zero cavity decay protection; audit against the actual shader amount.
                    const float D = std::min(Before[J], DecayRates[C] * AreaScale[I] * Dt);
                    Decay[J] = D;
                    CumDecay[C] += D;
                    Net[J] += Input[J] - D;

                    for (unsigned Dir = 0; Dir < 8; ++Dir)
                    {
                        const auto Other = Texels[I].NeighborIndices[Dir];

                        if (Other == InvalidTexelIndex)
                            continue;
                        const double Flow = Before[J] > 0 ? double(Raw[N * K + J * 8 + Dir]) * Alpha[J] : 0.0;
                        Net[J] -= Flow;
                        Net[Other * K + C] += Flow;
                        const bool bHere = V[I] < -.1F, bThere = V[Other] < -.1F;

                        if (bHere != bThere)
                            CumCross[C] += bThere ? Flow : -Flow;
                    }

                    if (M && bLedger)
                        for (unsigned R = 0; R < M; ++R)
                            Net[J] += Stoich[R][C] * Scratch[N * Stride + I * M + R];
                }
            }

        for (unsigned R = 0; R < M; ++R)
            CumExtent[R] += StepExtents[R];
        double Mass = 0, MaxHeightDelta = 0;

        for (std::size_t I = 0; I < N; ++I)
        {
            MaxHeightDelta = std::max(MaxHeightDelta, std::abs(double(Dynamic[I * 8 + 7] - G0[I * 8 + 7])));

            for (unsigned C = 0; C < K; ++C)
            {
                auto J = I * K + C;
                Nonfinite += !std::isfinite(After[J]);
                Negative += After[J] < 0;
                Sums[C] += After[J];

                if (V[I] < -.1F)
                    Downstream[C] += After[J];
                if (bLedger || !M)
                {
                    double Err = After[J] - Before[J] - Net[J];
                    MaxLocalResidual = std::max(MaxLocalResidual, std::abs(Err));
                    Residual[C] += Err;
                }

                if (C == Ice || C == Water || C == Wetness)
                    Mass += After[J];
            }
        }

        for (auto X : Residual)
            MaxGlobalResidual = std::max(MaxGlobalResidual, std::abs(X));
        MaxMassResidual = std::max(MaxMassResidual,
                                   std::abs(Mass - InitialMass - CumInput[Ice] - CumInput[Water] -
                                            (bHasWetness ? CumInput[Wetness] : 0) + CumDecay[Ice] + CumDecay[Water] +
                                            (bHasWetness ? CumDecay[Wetness] : 0)));
        FJson Row = {{"step", Step + 1},
                     {"time", double(Step + 1) / Hz},
                     {"gpu_ms", GpuMs},
                     {"state_sums", Sums},
                     {"downstream_sums", Downstream},
                     {"cumulative_input", CumInput},
                     {"cumulative_decay", CumDecay},
                     {"cumulative_extents", CumExtent},
                     {"cumulative_crossing", CumCross},
                     {"geometry_max_height_change", MaxHeightDelta},
                     {"max_mass_residual", MaxMassResidual}};
        Series << Row.dump() << '\n';

        if ((Step + 1) % Hz == 0 &&
            (Step + 1 == Hz || Step + 1 == 2 * Hz || Step + 1 == 4 * Hz || Step + 1 == 8 * Hz || Step + 1 == Steps))
        {
            auto Suffix = std::to_string((Step + 1) / Hz) + ".f32";
            Save(Out / ("state-" + Suffix), After);
            Save(Out / ("geometry-" + Suffix), Dynamic);
            Save(Out / ("height-" + Suffix), Read(Instance.GetAccumulationHeightBuffer()));
            Save(Out / ("weights-" + Suffix), Read(Instance.GetTransferWeightBuffer()));
            Save(Out / ("concavity-" + Suffix), Read(Instance.GetDynamicConcavityWeightBuffer()));
            Save(Out / ("flux-" + Suffix), Raw);
            Save(Out / ("alpha-" + Suffix), Alpha);
            Save(Out / ("executed-groups-" + Suffix), ExecutedGroups);
            std::cout << Variant << " " << Case << " t=" << double(Step + 1) / Hz
                      << " mass residual=" << MaxMassResidual << std::endl;
        }

        Before = std::move(After);
    }

    vkDestroyQueryPool(GPU.GetDevice(), Query, nullptr);
    Save(Out / "final.f32", Before);
    std::sort(Times.begin(), Times.end());
    std::sort(RecordTimes.begin(), RecordTimes.end());
    const auto Memory = MDSS::GPU::FGPUResourceMemoryStats::GetSnapshot();
    FJson      Summary = {
        {"case", Case},
        {"variant", Variant},
        {"channels", Names},
        {"texels", N},
        {"valid_texels", Valid},
        {"steps", Steps},
        {"hz", Hz},
        {"initial_hash", Hash(Initial)},
        {"bootstrap_max_abs", BootstrapError},
        {"initial_geometry_hash", Hash(G0)},
        {"final_hash", Hash(Before)},
        {"reaction_ledger_enabled", bLedger},
        {"full_geometry", bFull},
        {"sparse_solver", bSparse},
        {"implementation", Implementation},
        {"timing_only", bTiming},
        {"module_order_reversed", bReverse},
        {"diagnostic_steps", bTiming ? 0 : Steps},
        {"warmup_seconds", bTiming ? 1 : 0},
        {"warmup_reset_verified", bTiming},
        {"record_cpu_median_ms", RecordTimes.at(RecordTimes.size() / 2)},
        {"allocated_buffer_bytes", Memory.CurrentBufferBytes},
        {"peak_allocated_buffer_bytes", Memory.PeakBufferBytes},
        {"gpu", DeviceProperties.deviceName},
        {"driver_version", DeviceProperties.driverVersion},
        {"nonfinite", Nonfinite},
        {"negative", Negative},
        {"reaction_budget", Budget},
        {"reaction_arithmetic", Arithmetic},
        {"frozen_failures", FrozenFailures},
        {"max_local_budget_residual", MaxLocalResidual},
        {"max_global_step_budget_residual", MaxGlobalResidual},
        {"max_mass_residual", MaxMassResidual},
        {"initial_ice_water", InitialMass},
        {"cumulative_input", CumInput},
        {"cumulative_decay", CumDecay},
        {"cumulative_extents", CumExtent},
        {"gpu_median_ms", Times.at(Times.size() / 2)},
        {"gpu_p95_ms", Times.at(std::min(Times.size() - 1, Times.size() * 95 / 100))},
        {"timing_scope", "headless production solver; diagnostic readbacks between steps; not application performance"},
        {"meso", Meso},
        {"mapping_warnings", Mapping.Warnings}};
    std::ofstream(Out / "summary.json") << Summary.dump(2);

    if (Nonfinite || Negative || Budget || Arithmetic || FrozenFailures)
        return 2;
    return 0;
}
catch (const std::exception& E)
{
    std::cerr << E.what() << std::endl;

    return 1;
}

#pragma endregion
