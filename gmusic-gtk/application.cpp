#include "application.hpp"
#include "resources.h"

namespace gmusic
{
namespace gui
{

Application::Application(int argc, char **argv)
    : Gtk::Application(argc, argv, "org.gmapi")
{
}

void Application::on_startup()
{
    Gtk::Application::on_startup();
    builder = Gtk::Builder::create_from_resource(
        "/org/gmusic/ui/main-window-3.18.ui");
    builder->get_widget_derived("main-window", this->mainWindow);
    if (!this->mainWindow) {
        throw std::runtime_error("failed to load main window from ui file");
    }
    add_window(*this->mainWindow);
}

Glib::RefPtr<Application> Application::create(int argc, char **argv)
{
    return Glib::RefPtr<Application>(new Application(argc, argv));
}

Application::~Application()
{
    if (this->mainWindow) {
        delete this->mainWindow;
    }
}
}
}
