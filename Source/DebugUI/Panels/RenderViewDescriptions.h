/** @file RenderViewDescriptions.h
 * @brief 진단 뷰가 사용하는 형상 기준 설명을 제공한다.
 */
#pragma once
#include "Rendering/Core/RenderSettings.h"
namespace MDSS
{
    namespace
    {
        const char* GetViewGeometryDescription(Rendering::ERenderViewMode Mode)
        {
            switch (Mode)
            {
                case Rendering::ERenderViewMode::Lit:
                    return "기본적으로 원본 메시를 렌더링합니다. Mud/WaterFilm/Lava 적층 레이어가 활성화된 물체는 "
                           "베이스 표면도 텍셀 메시로 그립니다.";
                case Rendering::ERenderViewMode::Unlit:
                    return "원본 메시 위치에 조명 없이 재질색을 표시합니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::ERenderViewMode::Wireframe:
                    return "원본 메시 삼각형을 표시합니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::ERenderViewMode::VertexNormalWS:
                    return "원본 메시 정점 노멀을 표시합니다. 형상 변위는 없습니다.";
                case Rendering::ERenderViewMode::NormalTextureTS:
                    return "원본 메시의 노멀 텍스처를 표시합니다. 형상 변위는 없습니다.";
                case Rendering::ERenderViewMode::MappedNormalWS:
                    return "원본 메시의 매핑된 노멀을 표시합니다. 형상 변위는 없습니다.";
                case Rendering::ERenderViewMode::SurfaceStateHeatmap:
                    return "원본 메시 위에 State 데이터를 색으로 표시합니다. Relief 음영은 Meso 노멀을 쓸 수 있지만 "
                           "형상 변위는 없습니다.";
                case Rendering::ERenderViewMode::SurfaceValidity:
                case Rendering::ERenderViewMode::SurfaceID:
                case Rendering::ERenderViewMode::NeighborCount:
                case Rendering::ERenderViewMode::SurfaceSeam:
                case Rendering::ERenderViewMode::OutgoingFluxScale:
                case Rendering::ERenderViewMode::SolverTransferWeight:
                case Rendering::ERenderViewMode::SurfaceTexelGrid:
                case Rendering::ERenderViewMode::SurfaceTexelArea:
                    return "원본 메시 위에 진단 데이터를 표시합니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::ERenderViewMode::MesoHeight:
                case Rendering::ERenderViewMode::MesoOffset:
                    return "Meso 높이만 형상에 적용합니다. State 적층은 포함하지 않습니다.";
                case Rendering::ERenderViewMode::MacroGeometry:
                    return "원본 메시 형상입니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::ERenderViewMode::SurfaceAccumulation:
                    return "Meso + 선택 State 전체 높이를 형상에 적용합니다. 색은 선택한 적층 성분을 표시합니다.";
                case Rendering::ERenderViewMode::SurfaceFinalGeometry:
                    return "Meso + 선택 State 적층을 적용한 형상입니다. 모든 State를 합친 최종 형상은 아닙니다.";
                case Rendering::ERenderViewMode::TotalSimulationHeight:
                    return "현재 시뮬레이션 상태의 모든 State 높이를 합쳐 형상에 적용합니다.";
            }
            return "원본 메시 형상입니다.";
        }
    }
}
