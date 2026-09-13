#include "Application/Application.h"
#include "Logger/Logger.h"

#include <exception>

int main()
{
    try
    {
        MDSS::Application Application;
        Application.Run();
    }
    catch (const std::exception& Exception)
    {
        MDSS::Logger::Error("Application", std::string("Fatal error: ") + Exception.what());
        return 1;
    }

    return 0;
}
