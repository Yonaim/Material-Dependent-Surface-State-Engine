#include "SurfaceStateSystem/State/SimulationClock.h"
#include "SurfaceStateSystem/Mapping/SurfaceMappingBuilder.h"
#include "SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.h"
#include "SurfaceStateSystem/GPU/SurfaceGPUResourceLayout.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace
{
    void Check(bool Condition, const char* Message)
    {
        if (!Condition) throw std::runtime_error(Message);
    }

    void TestClock()
    {
        using namespace MDSS;
        for (int FPS : {15, 30, 60, 120})
        {
            TSimulationClock Clock;
            std::size_t Steps = 0;
            for (int Frame = 0; Frame < FPS; ++Frame)
            {
                Clock.Accumulate(1.0 / FPS, 1.0, false);
                Steps += Clock.Consume(FixedSimulationStepSeconds, true, false, false).size();
            }
            Check(Steps == 60 && std::abs(Clock.GetSimulatedSeconds() - 1.0) < 1.0e-6,
                  "15/30/60/120 FPS must advance the same simulation time");
        }
        TSimulationClock Clock;
        Clock.Accumulate(1.0, 1.0, false);
        Check(Clock.Consume(FixedSimulationStepSeconds, true, false, false).size() == MaxSimulationStepsPerFrame,
              "catch-up work must be bounded");
        Check(Clock.GetPendingSeconds() > 0.8, "catch-up limit must retain elapsed time");
        Clock.Accumulate(50.0, 1.0, true);
        const double Backlog = Clock.GetPendingSeconds();
        Check(Clock.Consume(FixedSimulationStepSeconds, true, true, false).empty(), "pause must not advance");
        Check(Clock.Consume(FixedSimulationStepSeconds, true, true, true).size() == 1,
              "paused single-step must execute exactly once");
        Check(Clock.GetPendingSeconds() == Backlog, "paused time and manual step must not consume backlog");
        Clock.Reset();
        Clock.Accumulate(0.1, 2.0, false);
        Check(Clock.Consume(0.01F, true, false, false).size() == 8 && Clock.GetPendingSeconds() > 0.11,
              "time scale must apply to elapsed budget while small safe steps retain backlog");
        Clock.Reset();
        Clock.Accumulate(0.005, 1.0, false);
        Check(Clock.Consume(FixedSimulationStepSeconds, true, false, false).empty(), "fixed mode must retain remainder");
        const auto Partial = Clock.Consume(FixedSimulationStepSeconds, false, false, false);
        Check(Partial.size() == 1 && std::abs(Partial[0] - 0.005F) < 1.0e-7, "variable mode must consume partial step");
    }

    void TestArea()
    {
        using namespace MDSS;
        const std::vector<TVertex> Vertices{
            {{0,0,0}, {0,0,1}, {0,0}}, {{2,0,0}, {0,0,1}, {1,0}},
            {{2,3,1}, {0,0,1}, {1,1}}, {{0,3,1}, {0,0,1}, {0,1}}};
        const std::vector<TMeshTriangleSource> Triangles{
            {{0,1,2}, {0,1,2}, {0,1,2}, 0}, {{0,2,3}, {0,2,3}, {0,2,3}, 0}};
        const glm::mat4 Model = glm::scale(glm::mat4(1), glm::vec3(2,3,4));
        const double Expected = glm::length(glm::cross(glm::vec3(4,0,0), glm::vec3(0,9,4)));
        double PreviousCapacity = 0.0;
        for (std::uint32_t Resolution : {128U, 256U, 512U})
        {
            const auto Mapping = TSurfaceMappingBuilder::Build(Vertices, Triangles,
                {{0, {Resolution, Resolution}}});
            auto Geometry = TSurfaceGeometryBuilder::Build(Mapping,
                std::vector<TSurfaceProfileIndex>(Mapping.Texels.size(), 0), 1);
            const auto Areas = BuildSurfaceGPUWorldTexelAreas(Geometry, Model);
            const double TotalArea = std::accumulate(Areas.begin(), Areas.end(), 0.0);
            Check(std::abs(TotalArea - Expected) < Expected * 1.0e-5,
                  "tilted UV plane must retain world area at 128/256/512 including nonuniform scale");
            const double TotalCapacity = TotalArea / SurfaceStateReferenceArea;
            if (PreviousCapacity != 0.0)
                Check(std::abs(TotalCapacity - PreviousCapacity) < PreviousCapacity * 1.0e-5,
                      "same surface must have the same integrated Capacity");
            PreviousCapacity = TotalCapacity;
            Check(std::abs(GetSurfaceWorldTexelArea(Geometry.GetTexels()[0], glm::scale(Model, glm::vec3(-1,1,1))) -
                           Areas[0]) < Areas[0] * 1.0e-5F, "reflection must retain positive physical area");
        }
    }
}

int main()
{
    try { TestClock(); TestArea(); std::cout << "Simulation clock and world-area tests passed\n"; return 0; }
    catch (const std::exception& Error) { std::cerr << Error.what() << '\n'; return 1; }
}
