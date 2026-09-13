#include "Application/Application.h"

#include <exception>
#include <iostream>

int main()
{
    try
    {
        MDSS::Application Application;
        Application.Run();
    }
    catch (const std::exception& Exception)
    {
        std::cerr << "Fatal error: " << Exception.what() << '\n';
        return 1;
    }

    return 0;
}
