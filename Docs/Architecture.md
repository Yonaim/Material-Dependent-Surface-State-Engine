# MDSS Engine Architecture

This document defines the high-level module structure and ownership boundaries of **MDSS Engine**.

# Module Structure

```text
Application
├── VulkanContext
├── Renderer
├── Scene
├── AssetManager
├── InputSystem
├── DebugUI
└── SurfaceStateSystem
```

Each top-level module uses a directory with the same name.

```text
Source/
├── Application/
├── VulkanContext/
├── Renderer/
├── Scene/
├── AssetManager/
├── InputSystem/
├── DebugUI/
└── SurfaceStateSystem/
```

# Application

`Application` owns the engine-level execution flow.

It coordinates frame-level work across input, simulation, and rendering.

`Window` currently belongs to the `Application` module.

# VulkanContext

`VulkanContext` provides the Vulkan infrastructure shared by rendering and compute systems.

```text
VulkanContext/
├── VulkanContext.h
├── VulkanContext.cpp
├── Vulkan/
└── GPU/
```

## Vulkan

The `Vulkan/` group contains Vulkan execution and infrastructure objects.

```text
Vulkan/
├── VulkanInstance
├── VulkanDevice
├── VulkanQueue
└── VulkanCommand
```

## GPU

The `GPU/` group contains reusable engine-side GPU resource wrappers.

```text
GPU/
├── GPUBuffer
├── GPUImage
├── GPUImageView
└── GPUSampler
```

# Renderer

`Renderer` owns the graphics frame flow and renderer-specific Vulkan objects.

```text
Renderer/
├── Renderer
├── Swapchain
├── RenderPass
├── GraphicsPipeline
├── Framebuffer
└── RenderContext
```

`Renderer::RenderFrame()` defines the graphics pass order for a frame.

`RenderContext` stores the state required to execute the current frame, such as command buffers, synchronization objects, and frame indices.

# Scene

`Scene` stores renderable scene instances and camera state.

```text
Scene/
├── Scene
├── Camera
├── Transform
└── StaticMeshInstance
```

A `StaticMeshInstance` references shared mesh data through an asset handle and owns instance-specific transform and surface-state references.

# AssetManager

`AssetManager` owns and resolves reusable asset data.

```text
AssetManager/
├── AssetManager
├── Asset
├── MeshAsset
├── TextureAsset
├── MaterialAsset
├── SRProfileAsset
└── Loader/
```

## Loader

File-format-specific loading code is grouped under `Loader/`.

```text
Loader/
├── OBJLoader
├── MTLLoader
├── TextureLoader
├── SceneLoader
└── SRProfileLoader
```

# InputSystem

`InputSystem` handles user input and interaction queries.

```text
InputSystem/
├── InputSystem
└── Raycaster
```

# DebugUI

`DebugUI` provides runtime controls and debugging interfaces.

# SurfaceStateSystem

`SurfaceStateSystem` owns surface-state simulation data and update logic.

```text
SurfaceStateSystem/
├── SurfaceStateSystem
├── SharedSurfaceGeometryData
├── SurfaceInstanceStateData
├── SurfaceInput
├── SurfaceStateSolver
├── SurfaceGeometryUpdate
└── DebugData
```

## Shared Surface Geometry

`SharedSurfaceGeometryData` contains geometry-derived data that can be shared by instances using the same mesh and surface geometry.

## Instance State

`SurfaceInstanceStateData` contains state data that belongs to a specific mesh instance.

## Simulation

`SurfaceStateSolver` updates surface states using input, transport, decay, and transition logic.

`SurfaceGeometryUpdate` converts state-derived accumulation into geometry-related results used by rendering.

# Ownership Boundaries

MDSS Engine does not use a universal object hierarchy.

The major ownership categories are kept separate:

```text
Systems
Scene Data
Assets
GPU Resources
Surface State Data
```

The shared asset hierarchy is limited to asset types with common asset responsibilities.

```text
Asset
├── MeshAsset
├── TextureAsset
├── MaterialAsset
└── SRProfileAsset
```

# Frame Responsibility

The high-level frame responsibilities are separated as follows:

```text
Application
├── InputSystem update
├── SurfaceStateSystem update
└── Renderer frame rendering
```

Surface-state compute flow belongs to `SurfaceStateSystem`.

Graphics render-pass flow belongs to `Renderer`.
