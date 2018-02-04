#include <iostream>

#include "application.hpp"
#include "main-window.hpp"
#include "session.hpp"
#include "utilities.hpp"
#include <mpg123.h>

int main(int argc, char *argv[])
{
    auto app = gmusic::gui::Application::create(argc, argv);
    return app->run(argc, argv);
}
