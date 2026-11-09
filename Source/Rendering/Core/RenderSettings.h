#pragma once
#include <cstdint>

namespace MDSS::Rendering
{
    enum class EOverlayDisplayMode : std::uint32_t
    {
        Both = 0,
        TopOnly,
        SidesOnly
    };

    enum class EOverlayShadingDiagnostic : std::uint32_t
    {
        Lit = 0,
        FixedNormal,
        NormalRGB,
        Facing,
        MeshNormal,
        VertexNormal,
        LitOpaque,
        StateLayers,
        MudDiffuse,
        MudSpecular
    };

    enum class EOverlayOccupancyTileSize : std::uint32_t
    {
        Tile8 = 8,
        Tile16 = 16,
        Tile32 = 32
    };

    struct FRendererProfilingStats
    {
        float               SolverGpuMilliseconds = -1.0F;
        float               SolverPass1GpuMilliseconds = -1.0F;
        float               SolverPass2GpuMilliseconds = -1.0F;
        float               AccumulationGeometryGpuMilliseconds = -1.0F;
        float               TransferWeightGpuMilliseconds = -1.0F;
        float               RenderPreparationGpuMilliseconds = -1.0F;
        float               SceneDrawGpuMilliseconds = -1.0F;
        float               TexelInspectorGpuMilliseconds = -1.0F;
        float               BaseMeshDrawGpuMilliseconds = -1.0F;
        float               MudOverlayDrawGpuMilliseconds = -1.0F;
        float               ReservedOverlayDrawGpuMilliseconds = -1.0F;
        float               OverlayPreparationGpuMilliseconds = -1.0F;
        float               OverlayGeometryGpuMilliseconds = -1.0F;
        float               OverlayHeightGpuMilliseconds = -1.0F;
        float               OverlayNormalVertexGpuMilliseconds = -1.0F;
        float               OverlaySmoothingGpuMilliseconds = -1.0F;
        float               OverlaySidesGpuMilliseconds = -1.0F;
        float               OverlaySidesPreBarrierGpuMilliseconds = -1.0F;
        float               OverlaySidesDispatchGpuMilliseconds = -1.0F;
        float               OverlaySidesPostBarrierGpuMilliseconds = -1.0F;
        float               OverlayBoundarySearchGpuMilliseconds = -1.0F;
        float               OverlayTopCommandGpuMilliseconds = -1.0F;
        float               OverlaySidesInterPassBarrierGpuMilliseconds = -1.0F;
        float               OverlayCoverageSampleGpuMilliseconds = -1.0F;
        float               SceneSetupGpuMilliseconds = -1.0F;
        float               RenderPassBeginGpuMilliseconds = -1.0F;
        float               RenderPassColorStageGpuMilliseconds = -1.0F;
        float               RenderPassDepthStageGpuMilliseconds = -1.0F;
        float               ViewportSetupGpuMilliseconds = -1.0F;
        float               UIDrawGpuMilliseconds = -1.0F;
        float               RenderPassEndGpuMilliseconds = -1.0F;
        float               SceneBetweenDrawsGpuMilliseconds = -1.0F;
        float               SceneTailGpuMilliseconds = -1.0F;
        float               FrameFenceWaitCpuMilliseconds = -1.0F;
        float               AcquireCpuMilliseconds = -1.0F;
        float               CommandRecordCpuMilliseconds = -1.0F;
        float               QueueSubmitCpuMilliseconds = -1.0F;
        float               PresentCpuMilliseconds = -1.0F;
        std::uint32_t       SimulationSteps = 0;
        float               SimulatedThisFrameSeconds = 0.0F;
        float               PendingSimulationSeconds = 0.0F;
        float               DroppedSimulationSeconds = 0.0F;
        std::uint32_t       SimulationInstances = 0;
        std::uint64_t       SimulationTexels = 0;
        std::uint32_t       StateChannels = 0;
        std::uint32_t       SimulationResolution = 0;
        std::uint32_t       SurfaceTexelMeshResolution = 0;
        std::uint32_t       OverlayTexelMeshResolution = 0;
        std::uint64_t       OverlayActiveTopTriangles = 0;
        std::uint64_t       OverlayTotalTopTriangles = 0;
        std::uint64_t       OverlayActiveTiles = 0;
        std::uint64_t       OverlayTotalTiles = 0;
        EOverlayDisplayMode OverlayDisplayMode = EOverlayDisplayMode::Both;
    };

    enum class ERenderViewMode : std::uint32_t
    {
        Lit = 0,
        Unlit = 1,
        Wireframe = 2,
        VertexNormalWS = 3,
        NormalTextureTS = 4,
        MappedNormalWS = 5,
        SurfaceStateHeatmap = 6,
        SurfaceValidity = 7,
        SurfaceID = 8,
        NeighborCount = 9,
        SurfaceSeam = 10,
        OutgoingFluxScale = 11,
        SolverTransferWeight = 12,
        /** @brief 부호가 있는 중간 규모 높이를 색상으로 표시한다. */
        MesoHeight = 13,
        /** @brief texel 연결면을 복원한 Meso 높이로 표시한다. */
        MesoOffset = 14,
        /** @brief 원본 거시 형상을 노멀 맵 음영 없이 표시한다. */
        MacroGeometry = 15,
        /** @brief 확대 시 개별 텍셀과 고정 크기 묶음의 UV 격자를 표시한다. */
        SurfaceTexelGrid = 16,
        /** @brief 원본 삼각형의 월드 면적 / UV 면적 / 텍셀 수를 표시한다. */
        SurfaceTexelArea = 17,
        SurfaceAccumulation = 18,
        SurfaceFinalGeometry = 19,
        TotalSimulationHeight = 21
    };

    enum class ESolverTransferWeightView : std::uint32_t
    {
        Combined = 0,
        Distance,
        Normal,
        ProfileBoundary
    };

    struct FSurfaceDebugDisplaySettings
    {
        std::uint32_t AccumulationComponent = 0;
        float         DisplacementScale = 1.0F;

        // 0: shaded surface, 1: grid overlay, 2: grid on a dark surface.
        std::uint32_t HeightGridMode = 0;
        std::uint32_t HeightGridBlockSize = 8;
    };

} // namespace MDSS::Rendering
