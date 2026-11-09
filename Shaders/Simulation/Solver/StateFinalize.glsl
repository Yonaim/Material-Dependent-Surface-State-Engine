
// ============================================================
// 00. State Finalization
// ============================================================
// #region 00_State_Finalization

bool accumulationContributionChanged(uint TexelIndex,
                                     uint ChannelIndex,
                                     float OldState,
                                     float NewState)
{
    if (!useSparseAccumulationHeight() || !prepareAccumulationHeight())
        return false;

    uint Record = profileRecordIndex(TexelIndex, ChannelIndex);
    // Sparse-height scheduling only needs capacity and accumulation factor: fetch two SoA planes, not all 3 vec4s.
    float Factor = profileDecayAndGeometry(Record).z;
    if (!solverFinite(Factor) || Factor <= 0.0)
        return false;

    float AreaScale = texelAreaScale(TexelIndex);
    float Capacity = profileCapacityInputAndTransfer(Record).x * AreaScale;
    if (!solverFinite(Capacity) || Capacity <= 0.0)
        return false;

    // Height는 capacity까지의 state만 사용하므로 capacity 위의 overflow-like state 변화는 재계산 사유가 아니다.
    float OldVisible = min(max(OldState, 0.0), Capacity);
    float NewVisible = min(max(NewState, 0.0), Capacity);
    // Height pass 자체가 BuiltHeight 기준 tolerance를 갖고 있으므로 여기서는 누적 손실이 생기지 않게
    // visible state가 조금이라도 바뀌면 schedule한다.
    return NewVisible != OldVisible;
}
// #endregion
