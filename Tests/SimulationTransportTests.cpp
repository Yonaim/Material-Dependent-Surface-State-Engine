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
        Check(DefaultFixedSimulationTimestep && !DefaultAutoSubstepping,
              "default policy must be fixed ticks with auto substepping disabled");
        for (int FPS : {15, 30, 60, 120})
        {
            TSimulationClock Clock;
            std::size_t Steps = 0;
            for (int Frame = 0; Frame < FPS; ++Frame)
            {
                Clock.Accumulate(1.0 / FPS, 1.0, false);
                const auto FrameSteps = Clock.Consume(0.001F, true, false, false, false);
                for (float Step : FrameSteps)
                    Check(Step == FixedSimulationStepSeconds, "fixed dt must ignore the transport limit when auto is off");
                Steps += FrameSteps.size();
            }
            Check(Steps == 60 && std::abs(Clock.GetSimulatedSeconds() - 1.0) < 1.0e-6,
                  "15/30/60/120 FPS must advance the same simulation time");
        }
        TSimulationClock Clock;
        Clock.Accumulate(1.0, 1.0, false);
        Check(Clock.Consume(FixedSimulationStepSeconds, true, false, false, false).size() == MaxSimulationStepsPerFrame,
              "catch-up work must be bounded");
        Check(Clock.GetPendingSeconds() > 0.8, "catch-up limit must retain elapsed time");
        Clock.Accumulate(50.0, 1.0, true);
        const double Backlog = Clock.GetPendingSeconds();
        Check(Clock.Consume(FixedSimulationStepSeconds, true, false, true, false).empty(), "pause must not advance");
        Check(Clock.Consume(0.001F, true, false, true, true) == std::vector<float>{FixedSimulationStepSeconds},
              "paused single-step must execute exactly once");
        Check(Clock.Consume(0.001F, true, true, true, true) == std::vector<float>{0.001F},
              "paused single-step must honor the transport limit only when auto is enabled");
        Check(Clock.GetPendingSeconds() == Backlog, "paused time and manual step must not consume backlog");
        Clock.Reset();
        Clock.Accumulate(0.1, 2.0, false);
        Check(Clock.Consume(0.01F, true, true, false, false).size() == 8 && Clock.GetPendingSeconds() > 0.11,
              "time scale must apply to elapsed budget while small safe steps retain backlog");
        Clock.Reset();
        Clock.Accumulate(0.005, 1.0, false);
        Check(Clock.Consume(0.001F, true, true, false, false).empty(),
              "auto must not start a fixed tick before the full 1/60 s has accumulated");
        const auto Partial = Clock.Consume(FixedSimulationStepSeconds, false, false, false, false);
        Check(Partial.size() == 1 && std::abs(Partial[0] - 0.005F) < 1.0e-7, "variable mode must consume partial step");

        Clock.Reset();
        Clock.Accumulate(0.05, 1.0, false);
        const auto Variable = Clock.Consume(0.001F, false, false, false, false);
        Check(Variable.size() == 1 && std::abs(Variable[0] - 0.05F) < 1e-7 && Clock.GetPendingSeconds() == 0,
              "variable mode without auto must consume elapsed time in one step");
        Clock.Reset();
        Clock.Accumulate(0.005, 1.0, false);
        const auto VariableSubsteps = Clock.Consume(0.001F, false, true, false, false);
        Check(VariableSubsteps.size() == 5 && Clock.GetPendingSeconds() == 0 &&
              std::abs(Clock.GetSimulatedSeconds() - 0.005) < 1e-8,
              "variable auto mode must split elapsed time including its final remainder");

        Clock.Reset();
        Clock.Accumulate(FixedSimulationStepSeconds, 1.0, false);
        const auto Uneven = Clock.Consume(0.01F, true, true, false, false);
        Check(Uneven.size() == 2 && Uneven[0] == 0.01F && Uneven[1] < 0.01F &&
              std::abs(Clock.GetSimulatedSeconds() - FixedSimulationStepSeconds) < 1e-8,
              "fixed auto mode must finish the tick with a shorter last substep");

        Clock.Reset();
        Clock.Accumulate(FixedSimulationStepSeconds, 1.0, false);
        const float SmallStep = FixedSimulationStepSeconds / 16;
        Check(Clock.Consume(SmallStep, true, true, false, false).size() == 8 && Clock.GetPendingSeconds() > 0,
              "substeps must obey the frame work limit even within a fixed tick");
        Check(Clock.Consume(SmallStep, true, true, false, false).size() == 8 && Clock.GetPendingSeconds() == 0,
              "an unfinished fixed tick must resume without waiting for another full tick");
        Check(std::abs(Clock.GetSimulatedSeconds() - FixedSimulationStepSeconds) < 1e-8,
              "resuming a tick must neither lose nor double its elapsed time");

        Clock.Reset();
        Clock.Accumulate(FixedSimulationStepSeconds, 1.0, false);
        (void)Clock.Consume(SmallStep, true, true, false, false);
        const double TransitionRemainder = Clock.GetPendingSeconds();
        const auto Finish = Clock.Consume(SmallStep, true, false, false, false);
        Check(Finish.size() == 1 && std::abs(Finish[0] - TransitionRemainder) < 1e-8 &&
              Clock.GetPendingSeconds() == 0,
              "disabling auto during a tick must finish its remainder without resetting time");

        Clock.Reset();
        Clock.Accumulate(FixedSimulationStepSeconds, 1.0, false);
        (void)Clock.Consume(SmallStep, true, true, false, false);
        const double VariableRemainder = Clock.GetPendingSeconds();
        const auto FinishVariable = Clock.Consume(SmallStep, false, false, false, false);
        Check(FinishVariable.size() == 1 && std::abs(FinishVariable[0] - VariableRemainder) < 1e-8 &&
              Clock.GetPendingSeconds() == 0,
              "switching both options off must consume the remaining budget exactly once");
        Clock.Accumulate(FixedSimulationStepSeconds, 1.0, false);
        Check(Clock.Consume(SmallStep, true, false, false, false) == std::vector<float>{FixedSimulationStepSeconds},
              "returning to fixed mode must not resurrect the previous tick remainder");

        Clock.Reset();
        Clock.Accumulate(FixedSimulationStepSeconds, 1.0, false);
        (void)Clock.Consume(SmallStep, true, true, false, false);
        Clock.Reset();
        Clock.Accumulate(0.005, 1.0, false);
        Check(Clock.Consume(SmallStep, true, true, false, false).empty(),
              "reset must discard the in-progress fixed tick as well as elapsed time");
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
