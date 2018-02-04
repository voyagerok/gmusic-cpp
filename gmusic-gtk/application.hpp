#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <gtkmm.h>

#include "main-window.hpp"

namespace gmusic
{
namespace gui
{

class Application : public Gtk::Application
{
  public:
    static Glib::RefPtr<Application> create(int, char **);

  protected:
    Application(int, char **);
    ~Application() override;
    void on_startup() override;

  private:
    Glib::RefPtr<Gtk::Builder> builder;
    MainWindow *mainWindow;
};
}
}

#endif
