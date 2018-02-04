#include "main-window.hpp"
#include "login-window.hpp"
#include "utilities.hpp"

#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <iomanip>

namespace gmapi
{
namespace gui
{

TreeViewModelColumns::TreeViewModelColumns()
{
    add(trackNum);
    add(trackName);
    add(artistName);
    add(albumName);
    add(genre);
    add(trackId);
    add(duration);
}

SideTreeViewModelColumns::SideTreeViewModelColumns()
{
    add(name);
    add(id);
    add(type);
}

#define TASK(...) session.taskBuilder.task<__VA_ARGS__>()

LogWindow::LogWindow(BaseObjectType *base, Glib::RefPtr<Gtk::Builder> &builder)
    : Gtk::Window(base), builder(builder)
{
    builder->get_widget("device-list-button", listAllDevicesButton);
    builder->get_widget("info-textview", infoTextView);
    Gtk::Button *getTracksButton = nullptr;
    builder->get_widget("tracks-button", getTracksButton);

    listAllDevicesButton->signal_clicked().connect([this] {
        assert(this->session != nullptr);
        this->session->taskBuilder.task<DeviceList>()
            .setJob([this](std::atomic<bool> *) -> DeviceList {
                return this->session->getApi()
                    ->getDeviceApi()
                    .getRegisteredDevices();
            })
            .setCompletionHandler(
                [this] { this->signal_deviceListReady.emit(); })
            .run();
    });
    getTracksButton->signal_clicked().connect(
        [this] { assert(this->session != nullptr); });

    this->signal_deviceListReady.connect([this] {
        assert(this->session != nullptr);
        try {
            auto deviceList =
                this->session->taskBuilder.task<DeviceList>().get();
            std::ostringstream ostr;
            for (const auto &device : deviceList) {
                ostr << "Device id: " << device.deviceId << ", "
                     << "device name: " << device.friendlyName << ", "
                     << "device type: " << device.deviceType << std::endl;
            }
            this->infoTextView->get_buffer()->set_text(ostr.str());
        } catch (const std::exception &err) {
            this->infoTextView->get_buffer()->set_text(err.what());
        } catch (...) {
            this->infoTextView->get_buffer()->set_text("Unknown error");
        }
    });
}

void LogWindow::present() { show_all(); }

static std::string getDataDir()
{
    static auto dataDir = Glib::get_user_data_dir() + "/" + Glib::get_prgname();
    STDLOG << "Data dir: " << dataDir << std::endl;
    if (!FSUtils::isFileExists(dataDir)) {
        FSUtils::createDirectoryIfNeeded(dataDir);
    }
    return dataDir;
}

MainWindow::MainWindow(BaseObjectType *base,
                       Glib::RefPtr<Gtk::Builder> &builder)
    : Gtk::ApplicationWindow(base), session(getDataDir()), builder(builder)
{
    set_default_size(800, 600);
    set_title("GMPlayer");

    builder->get_widget("tree-view", treeView);
    if (!treeView) {
        throw std::runtime_error("failed to load treeView widget");
    }
    builder->get_widget("side-tree-view", sideTreeView);
    if (!sideTreeView) {
        throw std::runtime_error("failed to load sideTreeView widget");
    }
    setupTreeView();
    setupSideTreeView();

    Gtk::Overlay *overlay = nullptr;
    builder->get_widget("main-window-overlay", overlay);
    overlay->add_overlay(spinner);
    overlay->set_overlay_pass_through(spinner);

    Gtk::Button *playButton, *pauseButton, *skipForwardButton,
        *skipBackwardButton;
    builder->get_widget("play-button", playButton);
    builder->get_widget("pause-button", pauseButton);
    builder->get_widget("skip-forward-button", skipForwardButton);
    builder->get_widget("skip-backward-button", skipBackwardButton);

    builder->get_widget("playback-progressbar", playbackProgressWidget);
    builder->get_widget("current-track-label", trackLabel);
    builder->get_widget("current-time-label", timeLabel);
    trackLabel->set_text("No active track");
    timeLabel->set_text("00:00 / 00:00");

    Gtk::VolumeButton *volButton;
    builder->get_widget("volume-button", volButton);
    volButton->set_value(0.5);
    volButton->signal_value_changed().connect([this](const double &val) {
        player.changeVolume(val);
        session.getStorage().saveValueForKey(val, "volume");
    });

    playButton->signal_clicked().connect([this] { player.resume(); });
    pauseButton->signal_clicked().connect([this] { player.pause(); });

    skipForwardButton->signal_clicked().connect([this] { playNext(); });

    skipBackwardButton->signal_clicked().connect([this] { playPrev(); });

    this->playbackProgressWidget->signal_change_value().connect(
        [this](Gtk::ScrollType, double newValue) -> bool {
            if (shouldHandleValueChanged) {
                auto adjustment = playbackProgressWidget->get_adjustment();
                auto value      = newValue / (adjustment->get_upper() -
                                         adjustment->get_lower());
                double seconds  = playedTrack.overallTimeSec * value;
                player.seek(seconds);
            }
            return true;
        });

    signal_realize().connect(
        sigc::mem_fun(this, &MainWindow::on_windowRealized));
    signal_streamUrl.connect(
        sigc::mem_fun(this, &MainWindow::om_streamUrlReceived));
    signal_localStorageUpdateCompleted.connect(
        sigc::mem_fun(this, &MainWindow::on_localDataUpdated));

    signal_playbackProgress.connect(
        sigc::mem_fun(this, &MainWindow::on_playbackProgressUpdated));
    signal_playbackStarted.connect(
        sigc::mem_fun(this, &MainWindow::on_playbackStarted));
    signal_playbackStopped.connect(
        sigc::mem_fun(this, &MainWindow::on_playbackFinished));

    signal_loginCompleted.connect([this] {
        using std::string;
        try {
            TASK(void, string, string, string).get();
            loadTracks();
        } catch (const std::exception &exc) {
            showErrorDialog(exc.what());
            login();
        } catch (...) {
            showErrorDialog("Something went wrong. Please try again");
            login();
        }
    });

    show_all();

    if (!session.isAuthorized()) {
        login();
    } else {
        loadTracks();
    }
    player.setDelegate(this);

    auto volume = session.getStorage().getValueForKey<double>("volume");
    if (!volume) {
        double defaultVolume = 0.5;
        session.getStorage().saveValueForKey(defaultVolume, "volume");
        volume = defaultVolume;
    }
    player.changeVolume(*volume);
    volButton->set_value(*volume);
}

void MainWindow::scaleSetValue(double value)
{
    shouldHandleValueChanged = false;
    playbackProgressWidget->set_value(value);
    shouldHandleValueChanged = true;
}

void MainWindow::on_playbackProgressUpdated()
{
    double value    = player.getLastProgressValue();
    auto adjustment = playbackProgressWidget->get_adjustment();
    auto actualValue =
        value * (adjustment->get_upper() - adjustment->get_lower());
    scaleSetValue(actualValue);

    int currentTime = static_cast<int>(value * playedTrack.overallTimeSec);
    timeLabel->set_text(SysUtils::timeStringFromSeconds(currentTime) + " / " +
                        playedTrack.overallTimeString);
}

void MainWindow::on_playbackStarted()
{
    auto artist = session.getDatabase()->getArtistTable().get(
        playedTrack.track.artistIds.at(0));
    auto album =
        session.getDatabase()->getAlbumTable().get(playedTrack.track.albumId);
    auto trackFullName = artist.name + " - " + playedTrack.track.name +
                         " (from " + album.name + ")";
    trackLabel->set_text(trackFullName);
    timeLabel->set_text("00:00 / 00:00");
    this->set_title(trackFullName);
    auto adjustment = playbackProgressWidget->get_adjustment();
    scaleSetValue(adjustment->get_lower());
    updateSelection(playedTrack.track.trackId);
}

void MainWindow::on_playbackFinished()
{
    if (!player.inProgress()) {
        trackLabel->set_text("No active track");
        timeLabel->set_text("00:00 / 00:00");
        this->set_title("GMPlayer");
        auto adjustment = playbackProgressWidget->get_adjustment();
        scaleSetValue(adjustment->get_lower());
        playNext();
    }
}

void MainWindow::on_windowRealized()
{
    auto firstColumnWidth = treeView->get_column(0)->get_width();
    int width, height;
    get_size(width, height);
    auto columnWidth = (width - firstColumnWidth) / 4;
    treeView->get_column(1)->set_fixed_width(columnWidth);
    treeView->get_column(2)->set_fixed_width(columnWidth);
    treeView->get_column(3)->set_fixed_width(columnWidth);
    Gtk::Paned *panedWidget = nullptr;
    this->builder->get_widget("paned-widget", panedWidget);
    panedWidget->set_position(columnWidth);
}

void MainWindow::om_streamUrlReceived()
{
    try {
        auto trackUrl = TASK(std::string, std::string).get();
        //        player.playURL(trackUrl);
        player.playTrack(trackUrl);
    } catch (const ApiRequestHttpException &exc) {
        if (exc.error.code == HttpErrorCode::UNAUTHORIZED) {
            showErrorDialog("You are not authorized. Please login.");
            login();
        } else {
            showErrorDialog(exc.error.message);
        }
    } catch (const std::exception &exc) {
        showErrorDialog(exc.what());
    }
}

void MainWindow::on_localDataUpdated()
{
    spinner.stop();
    try {
        TASK(void).get();
        auto artists = session.getDatabase()->getArtistTable().getAll();
        for (const auto &artist : artists) {
            auto row                       = *(sideTreeModel->append());
            row[sideTreeModelColumns.name] = artist.name;
            row[sideTreeModelColumns.id]   = artist.artistId;
            row[sideTreeModelColumns.type] = RowType::Artist;
            auto albums =
                session.getDatabase()->getAlbumTable().getAllForArtist(
                    artist.artistId);
            for (const auto &album : albums) {
                auto childRow = *(sideTreeModel->append(row.children()));
                childRow[sideTreeModelColumns.name] = album.name;
                childRow[sideTreeModelColumns.type] = RowType::Album;
                childRow[sideTreeModelColumns.id]   = album.albumId;
            }
        }
        currentTracks = session.getDatabase()->getTrackTable().getAll(
            db::TrackTable::TrackType::Regular);
        fillTrackTreeView();
    } catch (const std::exception &exc) {
        showErrorDialog(exc.what());
    }
}

void MainWindow::setupTreeView()
{
    filterFunc = [this](const Gtk::TreeModel::iterator &iter) -> bool {
        if (filterParams.pattern.empty()) {
            return true;
        }
        std::string name;
        switch (filterParams.rowType) {
        case RowType::Album:
            name = (*iter)[modelColumns.albumName];
            break;
        case RowType::Artist:
            name = (*iter)[modelColumns.artistName];
            break;
        }

        return name == filterParams.pattern;
    };

    treeModel       = Gtk::ListStore::create(modelColumns);
    treeModelFilter = Gtk::TreeModelFilter::create(treeModel);
    treeModelFilter->set_visible_func(filterFunc);
    treeView->set_model(treeModelFilter);

    treeView->append_column("Track", modelColumns.trackNum);
    treeView->append_column("Title", modelColumns.trackName);
    treeView->append_column("Album", modelColumns.albumName);
    treeView->append_column("Artist", modelColumns.artistName);
    treeView->append_column("Duration", modelColumns.duration);
    treeView->append_column("Genre", modelColumns.genre);

    treeView->get_column(0)->set_sort_column(modelColumns.trackNum);
    treeView->get_column(1)->set_sort_column(modelColumns.trackName);
    treeView->get_column(2)->set_sort_column(modelColumns.albumName);
    treeView->get_column(3)->set_sort_column(modelColumns.artistName);
    treeView->get_column(4)->set_sort_column(modelColumns.duration);

    treeView->get_column(1)->set_resizable();
    treeView->get_column(2)->set_resizable();
    treeView->get_column(3)->set_resizable();

    using ModelIter = Gtk::TreeModel::iterator;
    auto sort_func  = [this](const ModelIter &iter1,
                            const ModelIter &iter2) -> int {
        const std::string &artist1 = (*iter1)[modelColumns.artistName];
        const std::string &artist2 = (*iter2)[modelColumns.artistName];

        if (artist1 < artist2)
            return -1;
        if (artist1 > artist2)
            return 1;

        const std::string &album1 = (*iter1)[modelColumns.albumName];
        const std::string &album2 = (*iter2)[modelColumns.albumName];

        if (album1 < album2)
            return -1;
        if (album1 > album2)
            return 1;

        int trackNum1 = (*iter1)[modelColumns.trackNum];
        int trackNum2 = (*iter2)[modelColumns.trackNum];

        if (trackNum1 < trackNum2)
            return -1;
        if (trackNum1 > trackNum2)
            return 1;

        return 0;
    };

    treeModel->set_sort_func(modelColumns.trackNum, sort_func);
    treeModel->set_sort_column(modelColumns.trackNum, Gtk::SORT_ASCENDING);

    treeView->signal_row_activated().connect(
        [this](const Gtk::TreeModel::Path &treePath, Gtk::TreeViewColumn *) {
            using std::string;
            playlistWrapper.reset(
                new PlayListModelWrapper(treeModel, filterParams, filterFunc));
            auto iter = treeModelFilter->get_iter(treePath);
            playlistWrapper->start(
                treeModelFilter->convert_iter_to_child_iter(iter));
            play(treeModelFilter->convert_iter_to_child_iter(iter));
        });
}

void MainWindow::setupSideTreeView()
{
    sideTreeModel = Gtk::TreeStore::create(sideTreeModelColumns);
    sideTreeView->set_model(sideTreeModel);

    sideTreeView->append_column("Artists", sideTreeModelColumns.name);

    sideTreeView->get_column(0)->set_sort_column(sideTreeModelColumns.name);
    sideTreeModel->set_sort_column(0, Gtk::SORT_ASCENDING);

    sideTreeView->signal_row_activated().connect(
        [&](const Gtk::TreeModel::Path &treePath, Gtk::TreeViewColumn *) {
            auto iter            = sideTreeModel->get_iter(treePath);
            filterParams.rowType = (*iter)[sideTreeModelColumns.type];
            filterParams.pattern = (*iter)[sideTreeModelColumns.name];
            treeModelFilter->refilter();
            if (player.inProgress()) {
                updateSelection(playedTrack.track.trackId);
            }
        });
}

void MainWindow::showErrorDialog(const std::string &errMsg)
{
    Gtk::MessageDialog dlg(*this, errMsg, false, Gtk::MESSAGE_ERROR);
    dlg.run();
}

void MainWindow::login()
{
    LoginDialog dialog;
    dialog.set_transient_for(*this);
    dialog.add_button("OK", Gtk::RESPONSE_OK);
    dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);

    auto result = dialog.run();
    switch (result) {
    case Gtk::RESPONSE_OK:
        using std::string;
        auto &loginTask =
            session.taskBuilder.task<void, string, string, string>();
        loginTask
            .setJob([this](std::atomic_bool *,
                           string email,
                           string passwd,
                           string deviceId) {
                session.login(email, passwd, deviceId);
            })
            .setCompletionHandler([this] { signal_loginCompleted.emit(); })
            .run(dialog.getEmail(), dialog.getPassword(), dialog.getDeivceId());
        break;
    }
}

void MainWindow::loadTracks()
{
    spinner.start();
    TASK(void)
        .setJob(
            [this](std::atomic_bool *flag) { session.updateLocalData(flag); })
        .setCompletionHandler(
            [this] { signal_localStorageUpdateCompleted.emit(); })
        .run();
}

void MainWindow::updatePlaybackProgress() { signal_playbackProgress(); }

void MainWindow::playbackStarted() { signal_playbackStarted(); }

void MainWindow::playbackFinished() { signal_playbackStopped(); }

void MainWindow::updateCacheProgress() {}

void MainWindow::play(const Gtk::TreeIter &iter)
{
    using std::string;
    const string &trackId = (*iter)[modelColumns.trackId];
    playedTrack.update(session.getDatabase()->getTrackTable().get(trackId));
    TASK(string, string)
        .setJob([this](std::atomic_bool *, string trackId) -> string {
            return session.getApi()->getTrackApi().getStreamUrl(trackId);
        })
        .setCompletionHandler([this] { signal_streamUrl.emit(); })
        .run(trackId);
}

void MainWindow::playNext()
{
    Gtk::TreeIter iter;
    if (playlistWrapper && playlistWrapper->next(PlayListMode::Seq, iter)) {
        play(iter);
    }
}

void MainWindow::playPrev()
{
    Gtk::TreeIter iter;
    if (playlistWrapper && playlistWrapper->prev(PlayListMode::Seq, iter)) {
        play(iter);
    }
}

void MainWindow::fillTrackTreeView()
{
    treeModel->clear();
    for (const auto &track : currentTracks) {
        auto row                    = *(treeModel->append());
        row[modelColumns.trackNum]  = track.trackNumber;
        row[modelColumns.trackName] = track.name;
        row[modelColumns.trackId]   = track.trackId;
        row[modelColumns.genre]     = track.genre;
        row[modelColumns.duration]  = SysUtils::timeStringFromSeconds(
            static_cast<int>(track.msDuration / 1000));
        Album album = session.getDatabase()->getAlbumTable().get(track.albumId);
        row[modelColumns.albumName] = album.name;
        if (album.artistIds.size() > 0) {
            Artist artist =
                session.getDatabase()->getArtistTable().get(album.artistIds[0]);
            row[modelColumns.artistName] = artist.name;
        }
    }
}

void MainWindow::updateSelection(const std::string &trackId)
{
    auto children = treeModelFilter->children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        const std::string &id = (*it)[modelColumns.trackId];
        if (id == trackId) {
            treeView->get_selection()->select(it);
            return;
        }
    }
    treeView->get_selection()->unselect_all();
}
}
}
