#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QStandardPaths>

#include "logindialog.h"
#include "utilities.hpp"

static std::string getDataDir()
{
    static auto dataDir = std::string(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            .toLocal8Bit()
            .constData());
    qDebug("Data dir: %s", dataDir.data());
    if (!gmapi::FSUtils::isFileExists(dataDir)) {
        gmapi::FSUtils::createDirectoryIfNeeded(dataDir);
    }
    return dataDir;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), session(getDataDir())
{
    ui->setupUi(this);

    //    if (!session.isAuthorized()) {
    LoginDialog loginDialog;
    loginDialog.exec();
    //    }
}

MainWindow::~MainWindow() { delete ui; }
