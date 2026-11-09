// ============================================================
// 00. Shared Shader Helpers
// ============================================================
// #region 00_Shared_Shader_Helpers

float rawFlux(uint SourceTexel,
              uint TargetTexel,
              uint ChannelIndex,
              float CachedTransferWeight,
              TSurfaceGPUProfileParameters SourceParameters,
              float SourceSaturation,
              float TargetSaturation,
              float TargetConcavity,
              vec3 TargetPosition)
{
    if (CachedTransferWeight <= 0.0 || Solver.DeltaTime <= 0.0)
    {
        return 0.0;
    }

    float SaturationTransferRate = SourceParameters.CapacityInputAndTransfer.z * BaseSaturationTransferRate;
    float SaturationDrive = (Solver.Flags & (1u << 1u)) != 0u ? 0.0 : max(SourceSaturation - TargetSaturation, 0.0);
    float GeometryTransferRate = SourceParameters.CapacityInputAndTransfer.w * BaseGeometryTransferRate;
    float GeometryDrive =
        GeometryTransferRate > 0.0 && (Solver.Flags & 1u) == 0u ? geometryDriveToPosition(TargetPosition) : 0.0;
    float Exit = max(concavityWeight(SourceTexel) - TargetConcavity, 0.0);

    // Cavity resistance should disappear as the deposited layer reaches the
    // chart's Meso peak. Otherwise excess State stays trapped in full grooves.
    float CavityFill = clamp(min(SourceSaturation, 1.0) * SourceParameters.CapacityInputAndTransfer.x *
                                 SourceParameters.DecayAndGeometry.z * SourceParameters.DecayAndGeometry.w,
                             0.0,
                             1.0);
    float Retention = clamp(1.0 - SourceParameters.AccumulationThickness.y * Exit * (1.0 - CavityFill), 0.0, 1.0);

    return (SaturationDrive * SaturationTransferRate +
            GeometryDrive * GeometryTransferRate * max(SourceSaturation, 0.0)) *
           CachedTransferWeight * Retention * Solver.DeltaTime;
}

float rawFlux(uint SourceTexel,
              uint TargetTexel,
              uint ChannelIndex,
              float CachedTransferWeight,
              TSurfaceGPUProfileParameters SourceParameters,
              float SourceSaturation)
{
    // 포화도 차이와 경사 방향 이동량을 합쳐 source에서 target으로의 양을 계산한다.
    if (CachedTransferWeight <= 0.0 || Solver.DeltaTime <= 0.0 || !supportsChannel(TargetTexel, ChannelIndex))
    {
        return 0.0;
    }

    float SaturationTransferRate = SourceParameters.CapacityInputAndTransfer.z * BaseSaturationTransferRate;
    float GeometryTransferRate = SourceParameters.CapacityInputAndTransfer.w * BaseGeometryTransferRate;
    float TargetSaturation = 0.0;
    float TargetConcavity = 0.0;
    vec3 TargetPosition = vec3(0.0);

    // 필요한 항만 준비한다. GLSL은 함수 인자를 호출 전에 평가하므로
    // 아래 값을 무조건 인라인 인자로 넘기면 비활성 flux에서도 이웃 데이터를 읽게 된다.
    if ((Solver.Flags & (1u << 1u)) == 0u && SaturationTransferRate != 0.0)
    {
        TargetSaturation = saturation(TargetTexel, ChannelIndex);
    }

    if (SourceParameters.AccumulationThickness.y != 0.0)
    {
        TargetConcavity = concavityWeight(TargetTexel);
    }

    if (GeometryTransferRate > 0.0 && (Solver.Flags & 1u) == 0u)
    {
        TargetPosition = effectiveLocalPosition(TargetTexel);
    }

    return rawFlux(SourceTexel,
                   TargetTexel,
                   ChannelIndex,
                   CachedTransferWeight,
                   SourceParameters,
                   SourceSaturation,
                   TargetSaturation,
                   TargetConcavity,
                   TargetPosition);
}

// #endregion
