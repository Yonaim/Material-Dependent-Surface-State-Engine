/**
 * @file SurfaceGeometryUpdate.cpp
 * @brief CPU geometry update utilities are not required; the runtime update is GPU compute.
 *
 * Per-instance accumulation positions/normals are written by
 * Shaders/Simulation/SurfaceAccumulation.comp, followed by dynamic edge weights in
 * Shaders/Simulation/SurfaceGeometryUpdate.comp.
 */
