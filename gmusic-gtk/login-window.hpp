#ifndef LOGIN_WINDOW_HPP
#define LOGIN_WINDOW_HPP

#include <gtkmm.h>

namespace gmusic
{
namespace gui
{

class LoginDialog : public Gtk::Dialog
{
  public:
    LoginDialog();
    std::string getEmail() const { return entryEmail.get_text(); }
    std::string getPassword() const { return entryPasswd.get_text(); }
    std::string getDeivceId() const { return androidId.get_text(); }

  protected:
    void onUseMacClicked();

  private:
    //    Gtk::Button btnSubmit;
    Gtk::Entry entryEmail;
    Gtk::Entry entryPasswd;
    Gtk::Entry androidId;
    Gtk::CheckButton btnUseMac;
    //    Gtk::Box layoutBox;
};
}
}

#endif
