/**
 * @file main.cpp
 * @brief 엔진 응용 프로그램 진입점과 최상위 예외 처리.
 */

#include "Application/Application.h"
#include "Application/BenchmarkOptions.h"
#include "Logger/Logger.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

/**
 * @brief 엔진 응용 프로그램을 실행하고 처리되지 않은 초기화 오류를 기록한다.
 * @return 실행 성공 시 0, fatal error 발생 시 1.
 */
#pragma region Application_Entry_Point

int main(int Argc, char* Argv[])
{
    MDSS::TApplicationLaunchOptions LaunchOptions;
    try
    {
        LaunchOptions = MDSS::ParseApplicationLaunchOptions(Argc, Argv);
    }
    catch (const std::invalid_argument& Exception)
    {
        std::cerr << Exception.what() << '\n' << MDSS::GetApplicationUsage() << '\n';
        return 2;
    }

    try
    {
        MDSS::TApplication TApplication(std::move(LaunchOptions.Benchmark));
        TApplication.Run(LaunchOptions.FrameLimit);
    }
    catch (const std::exception& Exception)
    {
        MDSS::TLogger::Error("TApplication", std::string("Fatal error: ") + Exception.what());
        return 1;
    }

    return 0;
}
#pragma endregion
