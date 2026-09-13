# MDSS Engine Terminology

This document defines project-specific terms used throughout **MDSS Engine**.

# MDSS

**MDSS** stands for **Material-Dependent Surface State**.

The project name is **MDSS Engine**.

# Surface State

A **Surface State** is a time-varying value stored over a surface.

Current target states include:

```text
Wetness
Heat
Burn
Mud
```

# SRProfile

**SRProfile** is the source-code abbreviation for **Surface Response Profile**.

An SRProfile defines material-dependent response parameters used by the Surface State System.

Asset type:

```text
SRProfileAsset
```

Loader:

```text
SRProfileLoader
```

File extension:

```text
.srprofile
```

# Shared Surface Geometry Data

**Shared Surface Geometry Data** contains surface geometry information that can be reused by instances sharing the same mesh and surface geometry.

It includes geometry-derived fields used by the Surface State System.

# Surface Instance State Data

**Surface Instance State Data** contains mutable surface-state data owned by a specific mesh instance.

It includes current state values and temporary overflow values.

# Surface Input

**Surface Input** is the normalized input passed into the Surface State System when an external event changes a surface state.

Input sources may include raycasts or future collision events.

# Surface State Solver

The **Surface State Solver** updates Surface State values over the UV-space texel grid.

The common update structure consists of:

```text
Input
Transport
Decay
Transition
```

# Mobility

**Mobility** is an SRProfile parameter controlling how readily a state is transported across neighboring texels.

# Capacity

**Capacity** is an SRProfile parameter defining how much of a state a surface can retain before excess amount is handled separately.

# Saturation

**Saturation** is a runtime value derived during simulation from the current state relative to its capacity.

It is a calculated value rather than an independent SRProfile parameter.

# Overflow

**Overflow** is temporary state storage used when an amount cannot be represented directly in the primary state value.

It can also contribute to accumulation calculations.

# Accumulation

**Accumulation** converts state-related amounts into a geometric surface buildup.

It is a geometry calculation performed after state updating rather than an Input, Transport, or Decay term.

# Accumulation Height

**Accumulation Height** is the geometric height produced from accumulated state.

It may contain:

```text
Cavity Filling Height
Surface Following Height
```

# Surface Geometry Update

**Surface Geometry Update** applies geometry-related results derived from Surface State data, including accumulation-dependent surface changes.

# UV Space Texel Grid

The **UV Space Texel Grid** is the simulation domain used by the Surface State System.

Surface states and geometry-related fields are evaluated per texel rather than per mesh vertex.

# Debug Data

**Debug Data** contains runtime information used to inspect Surface State distributions, transport behavior, geometry fields, and solver results.
