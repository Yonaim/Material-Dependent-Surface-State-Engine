// ReactionBindings.glsl — 공유 반응 테이블·instance scratch와 shader의 접근 함수를 선언한다.
// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface
// CPU가 전달하는 리소스 선언과 shader의 접근 경계를 모아 ABI 대응을 확인한다.

layout(std430, set = 0, binding = 23) readonly buffer TReactionTable
{
    uint Values[];
}

ReactionTable;
// CPU InstanceResources: ordinary N*(5K+M+4) floats; opt-in flag 28 appends N*M rule extents.
layout(std430, set = 0, binding = 24) coherent buffer TReactionScratch
{
    float Values[];
}

ReactionScratch;
bool useReactions()
{
    return (Solver.Flags & (1u << 25u)) != 0u;
}

uint reactionScratchBase(uint Texel)
{
    return Texel * (5u * Solver.StateChannelCount + ReactionTable.Values[1] + 4u);
}

// #endregion
