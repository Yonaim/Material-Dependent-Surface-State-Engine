/**
 * @file SurfaceSimulationGpu.h
 * @brief 표면 시뮬레이션의 Vulkan 명령 기록과 상태 읽기 API를 정의한다.
 */

#pragma once

#include "GPU/Vulkan/Resource/GPUBuffer.h"
#include "SurfaceState/Simulation/SurfaceSimulationSnapshot.h"
#include <memory>

namespace MDSS::GPU
{
    class FVulkanContext;
}

namespace MDSS::Liquid
{
    /** @brief 최신 상태와 대표 액체 층의 마지막 수송 서브스텝 진단값. */
    struct FSurfaceSimulationReadback
    {
        FSurfaceSimulationState State;
        std::vector<float>      FaceCandidateMassKg;
        std::vector<float>      RegionOutflowScale;
    };

    /**
     * @brief 표면 시뮬레이션 계산 파이프라인과 상태 버퍼를 소유한다.
     * @pre Context의 Vulkan 장치는 이 객체가 소멸할 때까지 유효해야 한다.
     */
    class FSurfaceSimulationGpu
    {
      public:
        FSurfaceSimulationGpu(const GPU::FVulkanContext      &Context,
                              const FSurfaceSimulationDomain &Geometry,
                              bool                            bEnableSparseUpdates = true);
        ~FSurfaceSimulationGpu();
        FSurfaceSimulationGpu(const FSurfaceSimulationGpu &) = delete;
        FSurfaceSimulationGpu &operator=(const FSurfaceSimulationGpu &) = delete;

        /**
         * @brief 고정 스텝 하나를 Commands에 기록한다. 전역 갱신에는 다음 슬롯을 사용한다.
         * @param InputMass 이번 스텝에 유입되는 질량(kg).
         * @param Dose 클릭으로 지정한 저장 영역과 주입 질량(kg).
         * @param Thermal 재질 온도의 장면 보간 비율과 오프셋(K).
         */
        void RecordStep(VkCommandBuffer        Commands,
                        FSurfaceTilt           Pose,
                        float                  InputMass,
                        FThermalInput          Thermal = {},
                        FLiquidDose            Dose = {},
                        const FLocalStepInput *LocalInput = nullptr,
                        bool                   bRecordOverflow = false);
        /** @brief 다음 표시 구간을 시작하며 접촉별 마지막 이동 시각은 보존한다. */
        void BeginDisplayInterval() noexcept;
        /** @brief 최신 셀 상태를 별도 표시 버퍼에 복사하고 그래픽 셰이더에서 읽을 수 있게 한다. */
        void RecordDisplayCopy(VkCommandBuffer Commands, VkBuffer Display) const;
        /**
         * @brief 최신 상태와 대표 액체 층의 면별 후보 질량 및 공급 분지별 유출 제한 배율을 읽는다.
         * @pre 이 객체에 기록한 GPU 작업이 완료된 뒤 호출한다.
         */
        [[nodiscard]] FSurfaceSimulationReadback Readback() const;
        /**
         * @brief 접촉 및 방향별로 가장 최근에 월류가 있었던 구간의 기록을 읽는다.
         * @pre 월류를 기록한 GPU 작업이 완료된 뒤 호출한다.
         */
        [[nodiscard]] std::vector<FOverflowTransferRecord> ReadOverflow() const;

      private:
        void RecordPhase(VkCommandBuffer Commands,
                         std::uint32_t   Phase,
                         FSurfaceTilt    Pose,
                         float           InputMass,
                         FThermalInput   Thermal = {},
                         FLiquidDose     Dose = {},
                         bool            bLocalInput = false,
                         bool            bRecordOverflow = false,
                         bool            bSparse = false,
                         int             TransportIndex = 0,
                         bool            bTransportOnly = false) const;
        void Release() noexcept;

        VkDevice      Device;
        std::uint32_t CellCount;
        std::uint32_t RegionCount = 0;
        std::uint32_t FaceCount;
        std::size_t   ContactCount;
        struct FStorage;
        std::unique_ptr<FStorage> Storage;
        VkDescriptorSetLayout     SetLayout = VK_NULL_HANDLE;
        VkDescriptorPool          Pool = VK_NULL_HANDLE;
        VkDescriptorSet           Set = VK_NULL_HANDLE;
        VkPipelineLayout          Layout = VK_NULL_HANDLE;
        VkPipeline                Pipeline = VK_NULL_HANDLE;
        std::uint32_t             ReadSlot = 0;
        std::uint32_t             DisplayInterval = 0;
        int                       TransportSubsteps = 1;
        std::uint32_t             CompletedSteps = 0;
        FSurfaceTilt              PreparedPose{};
        FThermalInput             PreviousThermal{};
        bool                      bEnableSparseUpdates = true;
        bool                      bExternalExchange = true;
        float                     InitialTemperatureWeight = 1.F;
        bool                      bPreviousLocalInput = false;
    };
} // namespace MDSS::Liquid
