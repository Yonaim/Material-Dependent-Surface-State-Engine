// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface

layout(std430, set = 0, binding = 23) readonly buffer TReactionTable { uint Values[]; } ReactionTable;
layout(std430, set = 0, binding = 24) coherent buffer TReactionScratch { float Values[]; } ReactionScratch;
bool useReactions() { return (Solver.Flags & (1u << 25u)) != 0u; }
uint reactionScratchBase(uint Texel) { return Texel * (5u * Solver.StateChannelCount + ReactionTable.Values[1] + 4u); }
// #endregion
