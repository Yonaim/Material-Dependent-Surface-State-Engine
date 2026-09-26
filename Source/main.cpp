/**
 * @file main.cpp
 * @brief 엔진 응용 프로그램 진입점과 최상위 예외 처리.
 */

#include "Application/Application.h"
#include "Logger/Logger.h"

#include <charconv>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string_view>

/**
 * @brief 엔진 응용 프로그램을 실행하고 처리되지 않은 초기화 오류를 기록한다.
 * @return 실행 성공 시 0, fatal error 발생 시 1.
 */
int main(int Argc, char* Argv[])
{
    std::size_t FrameLimit = 0;
    if (Argc == 3 && std::string_view(Argv[1]) == "--frames")
    {
        const std::string_view FrameArgument(Argv[2]);
        const auto [End, Error] = std::from_chars(FrameArgument.data(),
                                                  FrameArgument.data() + FrameArgument.size(),
                                                  FrameLimit);
        if (Error != std::errc{} || End != FrameArgument.data() + FrameArgument.size() || FrameLimit == 0)
        {
            std::cerr << "--frames requires a positive integer.\n";
            return 2;
        }
    }
    else if (Argc != 1)
    {
        std::cerr << "Usage: MDSS [--frames COUNT]\n";
        return 2;
    }

    try
    {
        MDSS::TApplication TApplication;
        TApplication.Run(FrameLimit);
    }
    catch (const std::exception& Exception)
    {
        MDSS::TLogger::Error("TApplication", std::string("Fatal error: ") + Exception.what());
        return 1;
    }

    return 0;
}
