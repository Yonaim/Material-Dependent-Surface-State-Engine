#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"

#include <stdexcept>

namespace MDSS::SurfaceState
{
    TSurfaceSparseMetadataLayout GetSurfaceSparseMetadataLayout(std::size_t TexelCount)
    {
        if (TexelCount == 0)
            throw std::invalid_argument("Sparse metadata requires a non-zero texel count.");

        TSurfaceSparseMetadataLayout L{};
        L.WorkgroupCount = TexelCount / 64U + (TexelCount % 64U != 0U);
        const std::size_t G = L.WorkgroupCount;
        const std::size_t Base = TexelCount * 3U;
        const std::size_t SolverBlockWords = 3U * G + 4U; // dispatch3 + flags/list G + global mask + WG masks G

        L.SolverACommandWord = Base;
        L.SolverAFlagWord = L.SolverACommandWord + 3U;
        L.SolverAListWord = L.SolverAFlagWord + G;
        L.SolverAMaskWord = L.SolverAListWord + G;
        L.SolverAWorkgroupMaskWord = L.SolverAMaskWord + 1U;

        L.SolverBCommandWord = Base + SolverBlockWords;
        L.SolverBFlagWord = L.SolverBCommandWord + 3U;
        L.SolverBListWord = L.SolverBFlagWord + G;
        L.SolverBMaskWord = L.SolverBListWord + G;
        L.SolverBWorkgroupMaskWord = L.SolverBMaskWord + 1U;

        L.InputCountWord = Base + SolverBlockWords * 2U;
        L.InputMaskWord = L.InputCountWord + 1U;
        L.InputListWord = L.InputMaskWord + 1U;
        L.InputGroupMaskWord = L.InputListWord + G;

        L.AccumulationCommandWord = L.InputGroupMaskWord + G;
        L.AccumulationFlagWord = L.AccumulationCommandWord + 3U;
        L.AccumulationListWord = L.AccumulationFlagWord + G;

        L.GeometryCommandWord = L.AccumulationListWord + G;
        L.GeometryFlagWord = L.GeometryCommandWord + 3U;
        L.GeometryListWord = L.GeometryFlagWord + G;
        L.TotalWordCount = L.GeometryListWord + G;
        return L;
    }

} // namespace MDSS::SurfaceState
