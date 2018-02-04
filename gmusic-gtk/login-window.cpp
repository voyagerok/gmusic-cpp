#include "login-window.hpp"

namespace gmusic
{
namespace gui
{

LoginDialog::LoginDialog() : btnUseMac("Don't use android id")
{
    set_default_size(480, 320);
    set_title("GMPLayer auth");

    entryPasswd.set_visibility(false);
    entryPasswd.set_placeholder_text("Password");
    entryEmail.set_placeholder_text("Email");
    androidId.set_placeholder_text("Android device id");

    btnUseMac.signal_clicked().connect(
        sigc::mem_fun(*this, &LoginDialog::onUseMacClicked));

    auto layoutBox = get_content_area();
    layoutBox->set_spacing(6);
    layoutBox->set_halign(Gtk::ALIGN_CENTER);
    layoutBox->set_valign(Gtk::ALIGN_CENTER);

    layoutBox->pack_start(entryEmail);
    layoutBox->pack_start(entryPasswd);
    layoutBox->pack_start(androidId);
    layoutBox->pack_start(btnUseMac);

    show_all_children();
}

void LoginDialog::onUseMacClicked()
{
    if (btnUseMac.get_active()) {
        androidId.set_sensitive(false);
    } else {
        androidId.set_sensitive(true);
    }
}
}
}
