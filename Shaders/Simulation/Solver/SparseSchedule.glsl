// ============================================================
// 00. Shared Shader Helpers
// ============================================================

uint sparseWorkgroupCount()
{
    return (Solver.LocalTexelCount + 63u) / 64u;
}

uint neighborIndex(uint TexelIndex, uint DirectionIndex);

// AccumulationHeights metadata layout (uint words):
// [3*N]
// solver A/B:   [dispatch 3][scheduled flags G][scheduled list G][global mask 1][WG masks G]
// input merge:  [count 1][channel mask 1][group list G][group channel masks G]
// accumulation: [dispatch 3][scheduled flags G][scheduled list G]
// geometry:     [dispatch 3][scheduled flags G][scheduled list G]
uint solverScheduleBlockWords() { return sparseWorkgroupCount() * 3u + 4u; }
uint solverAScheduleCommandBase() { return 3u * Solver.LocalTexelCount; }
uint solverBScheduleCommandBase() { return solverAScheduleCommandBase() + solverScheduleBlockWords(); }
uint solverScheduleCommandBase(bool AB) { return AB ? solverAScheduleCommandBase() : solverBScheduleCommandBase(); }
uint solverScheduleFlagBase(bool AB) { return solverScheduleCommandBase(AB) + 3u; }
uint solverScheduleListBase(bool AB) { return solverScheduleFlagBase(AB) + sparseWorkgroupCount(); }
uint solverActiveChannelMaskIndex(bool AB) { return solverScheduleListBase(AB) + sparseWorkgroupCount(); }
uint solverWorkgroupMaskBase(bool AB) { return solverActiveChannelMaskIndex(AB) + 1u; }

uint solverCurrentCommandBase() { return solverScheduleCommandBase(currentStateIsAB()); }
uint solverCurrentFlagBase() { return solverScheduleFlagBase(currentStateIsAB()); }
uint solverCurrentListBase() { return solverScheduleListBase(currentStateIsAB()); }
uint solverCurrentMaskIndex() { return solverActiveChannelMaskIndex(currentStateIsAB()); }
uint solverCurrentWorkgroupMaskBase() { return solverWorkgroupMaskBase(currentStateIsAB()); }
uint solverNextCommandBase() { return solverScheduleCommandBase(!currentStateIsAB()); }
uint solverNextFlagBase() { return solverScheduleFlagBase(!currentStateIsAB()); }
uint solverNextListBase() { return solverScheduleListBase(!currentStateIsAB()); }
uint solverNextMaskIndex() { return solverActiveChannelMaskIndex(!currentStateIsAB()); }
uint solverNextWorkgroupMaskBase() { return solverWorkgroupMaskBase(!currentStateIsAB()); }

uint inputScheduleCountIndex() { return solverBScheduleCommandBase() + solverScheduleBlockWords(); }
uint inputScheduleMaskIndex() { return inputScheduleCountIndex() + 1u; }
uint inputScheduleListBase() { return inputScheduleMaskIndex() + 1u; }
uint inputScheduleGroupMaskBase() { return inputScheduleListBase() + sparseWorkgroupCount(); }

uint accumulationScheduleCommandBase() { return inputScheduleGroupMaskBase() + sparseWorkgroupCount(); }
uint accumulationScheduleFlagBase() { return accumulationScheduleCommandBase() + 3u; }
uint accumulationScheduleListBase() { return accumulationScheduleFlagBase() + sparseWorkgroupCount(); }

uint geometryScheduleCommandBase() { return accumulationScheduleListBase() + sparseWorkgroupCount(); }
uint geometryScheduleFlagBase() { return geometryScheduleCommandBase() + 3u; }
uint geometryScheduleListBase() { return geometryScheduleFlagBase() + sparseWorkgroupCount(); }

uint solverOriginalGroup()
{
    if (!useSparseSolver() || seedPersistentActivity()) return gl_WorkGroupID.x;
    return AccumulationHeights.Values[solverCurrentListBase() + gl_WorkGroupID.x];
}

uint accumulationOriginalGroup()
{
    if (!useSparseAccumulationHeight()) return gl_WorkGroupID.x;
    return AccumulationHeights.Values[accumulationScheduleListBase() + gl_WorkGroupID.x];
}

// Pass2 has finished producing this count before the height pass starts. This decision is
// uniform for all height/geometry invocations and needs neither readback nor an extra dispatch.
bool useDenseAdaptiveGeometry()
{
    if ((Solver.Flags & (1u << 24u)) == 0u || !useSparseSimulationGeometry()) return false;
    if (!useSparseAccumulationHeight()) return true;
    return AccumulationHeights.Values[accumulationScheduleCommandBase()] * 4u >= sparseWorkgroupCount() * 3u;
}

uint geometryOriginalGroup()
{
    if (!useSparseSimulationGeometry() || useDenseAdaptiveGeometry()) return gl_WorkGroupID.x;
    return AccumulationHeights.Values[geometryScheduleListBase() + gl_WorkGroupID.x];
}

bool isChannelActive(uint ChannelIndex, uint OriginalGroup)
{
    if (!useActiveChannelMask() || seedPersistentActivity() || ChannelIndex >= 32u) return true;
    uint Mask = AccumulationHeights.Values[solverCurrentMaskIndex()];
    if ((Mask & (1u << ChannelIndex)) == 0u) return false;
    if (!usePerWorkgroupChannelMask()) return true;
    uint GroupMask = AccumulationHeights.Values[solverCurrentWorkgroupMaskBase() + OriginalGroup];
    return (GroupMask & (1u << ChannelIndex)) != 0u;
}

void scheduleNextSolverGroup(uint Group)
{
    if (!useSparseSolver()) return;
    uint Groups = sparseWorkgroupCount();
    if (Group >= Groups) return;
    if (atomicCompSwap(AccumulationHeights.Values[solverNextFlagBase() + Group], 0u, 1u) == 0u)
    {
        uint Slot = atomicAdd(AccumulationHeights.Values[solverNextCommandBase()], 1u);
        if (Slot < Groups) AccumulationHeights.Values[solverNextListBase() + Slot] = Group;
    }
}

void scheduleNextSolverTexel(uint TexelIndex, uint ChannelMask)
{
    if (!useSparseSolver() && !usePerWorkgroupChannelMask()) return;
    uint OwnGroup = TexelIndex / 64u;
    scheduleNextSolverGroup(OwnGroup);
    // Pass2 is a gather pass: target groups adjacent to an active source must also run next step.
    for (uint Direction = 0u; Direction < SurfaceNeighborCount; ++Direction)
    {
        uint Neighbor = neighborIndex(TexelIndex, Direction);
        if (Neighbor == InvalidTexelIndex || Neighbor >= Solver.LocalTexelCount) continue;
        uint NeighborGroup = Neighbor / 64u;
        if (NeighborGroup == OwnGroup) continue;
        scheduleNextSolverGroup(NeighborGroup);
        if (usePerWorkgroupChannelMask() && ChannelMask != 0u)
            atomicOr(AccumulationHeights.Values[solverNextWorkgroupMaskBase() + NeighborGroup], ChannelMask);
    }
}

void scheduleAccumulationGroup(uint Group)
{
    if (!useSparseAccumulationHeight() || !prepareAccumulationHeight()) return;
    uint Groups = sparseWorkgroupCount();
    if (Group >= Groups) return;
    if (atomicCompSwap(AccumulationHeights.Values[accumulationScheduleFlagBase() + Group], 0u, 1u) == 0u)
    {
        uint Slot = atomicAdd(AccumulationHeights.Values[accumulationScheduleCommandBase()], 1u);
        if (Slot < Groups) AccumulationHeights.Values[accumulationScheduleListBase() + Slot] = Group;
    }
}
