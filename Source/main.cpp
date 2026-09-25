/**
 * @file main.cpp
 * @brief 엔진 응용 프로그램 진입점과 최상위 예외 처리.
 */

#include "Application/Application.h"
#include "Logger/Logger.h"

#include <exception>

/**
 * @brief 엔진 응용 프로그램을 실행하고 처리되지 않은 초기화 오류를 기록한다.
 * @return 실행 성공 시 0, fatal error 발생 시 1.
 */
int main()
{
    try
    {
        MDSS::TApplication TApplication;
        TApplication.Run();
    }
    catch (const std::exception& Exception)
    {
        MDSS::TLogger::Error("TApplication", std::string("Fatal error: ") + Exception.what());
        return 1;
    }

    return 0;
}
