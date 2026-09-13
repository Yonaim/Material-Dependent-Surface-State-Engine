#include "Application/Application.h"

#include <exception>
#include <iostream>

int main()
{
    try
    {
        mdssp::Application application;
        application.run();
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
