#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include "player.hpp"
#include "session.hpp"
#include "utilities.hpp"
#include <future>
#include <gtkmm.h>

namespace gmusic
{
namespace gui
{

struct TreeViewModelColumns : public Gtk::TreeModel::ColumnRecord {
    TreeViewModelColumns();

    Gtk::TreeModelColumn<int> trackNum;
    Gtk::TreeModelColumn<std::string> trackName;
    Gtk::TreeModelColumn<std::string> artistName;
    Gtk::TreeModelColumn<std::string> albumName;
    Gtk::TreeModelColumn<std::string> genre;
    Gtk::TreeModelColumn<std::string> duration;
    Gtk::TreeModelColumn<std::string> trackId;
};

enum class RowType { Artist, Album };

struct SideTreeViewModelColumns : public Gtk::TreeModel::ColumnRecord {
    SideTreeViewModelColumns();

    Gtk::TreeModelColumn<std::string> name;
    Gtk::TreeModelColumn<RowType> type;
    Gtk::TreeModelColumn<std::string> id;
};

struct FilterParams {
    RowType rowType;
    std::string pattern;
};

struct PlayedTrack {
    Track track;
    std::string overallTimeString;
    int overallTimeSec;
    void update(const Track &track);
};

inline void PlayedTrack::update(const Track &track)
{
    this->track       = track;
    overallTimeSec    = static_cast<int>(track.msDuration / 1000);
    overallTimeString = SysUtils::timeStringFromSeconds(overallTimeSec);
}

enum class PlayListMode { Seq, Shuffle };

struct PlayListModelWrapper {
    using FilterFunc = std::function<bool(const Gtk::TreeIter &)>;
    PlayListModelWrapper(const Glib::RefPtr<Gtk::TreeModel> &childModel,
                         const FilterParams &filterParams,
                         const FilterFunc &filterFunc)
        : playListModel(Gtk::TreeModelFilter::create(childModel)),
          filterParams(filterParams)
    {
        playListModel->set_visible_func(filterFunc);
    }

    void start(const Gtk::TreeIter &childIter);
    bool next(PlayListMode mode, Gtk::TreeIter &next);
    bool prev(PlayListMode mode, Gtk::TreeIter &prev);

    Gtk::TreeIter getIter() const
    {
        return playListModel->convert_iter_to_child_iter(currentTrackIter);
    }
    bool isValid() const { return valid; }

  private:
    Glib::RefPtr<Gtk::TreeModelFilter> playListModel;
    FilterParams filterParams;
    Gtk::TreeIter currentTrackIter;
    bool valid = false;
};

inline void PlayListModelWrapper::start(const Gtk::TreeIter &childIter)
{
    auto iter        = playListModel->convert_child_iter_to_iter(childIter);
    currentTrackIter = iter;
    valid            = true;
}

inline bool PlayListModelWrapper::next(PlayListMode, Gtk::TreeIter &next)
{
    if (!valid) {
        return false;
    }
    auto iter = currentTrackIter;
    if (++iter != playListModel->children().end()) {
        currentTrackIter = iter;
        next             = playListModel->convert_iter_to_child_iter(iter);
        return true;
    }
    return false;
}

inline bool PlayListModelWrapper::prev(PlayListMode, Gtk::TreeIter &prev)
{
    if (!valid) {
        return false;
    }
    auto iter = currentTrackIter;
    if (iter != playListModel->children().begin()) {
        currentTrackIter = --iter;
        prev             = playListModel->convert_iter_to_child_iter(iter);
        return true;
    }
    return false;
}

class LogWindow;

class MainWindow : public Gtk::ApplicationWindow, public AudioPlayerDelegate
{
  private:
    Glib::RefPtr<Gtk::ListStore> treeModel;
    Glib::RefPtr<Gtk::TreeModelFilter> treeModelFilter;
    FilterParams filterParams;
    std::function<bool(const Gtk::TreeIter &)> filterFunc;
    TreeViewModelColumns modelColumns;

    Glib::RefPtr<Gtk::TreeStore> sideTreeModel;
    SideTreeViewModelColumns sideTreeModelColumns;

    std::unique_ptr<PlayListModelWrapper> playlistWrapper = nullptr;

    void loadTracks();
    void login();
    void showErrorDialog(const std::string &errMsg);
    void setupTreeView();
    void setupSideTreeView();

    Session session;
    //    AudioPlayer player;
    AudioPlayer player;
    //    LogWindow logWindow;
    std::unique_ptr<LogWindow> logWindow;

    Glib::Dispatcher signal_localStorageUpdateCompleted;
    Glib::Dispatcher signal_loginCompleted;
    Glib::Dispatcher signal_streamUrl;
    Glib::Dispatcher signal_playbackStarted;
    Glib::Dispatcher signal_playbackStopped;
    Glib::Dispatcher signal_playbackProgress;

    void om_streamUrlReceived();
    void on_localDataUpdated();
    void on_windowRealized();
    void on_playbackProgressUpdated();
    void on_playbackStarted();
    void on_playbackFinished();

    std::vector<Track> currentTracks;
    PlayedTrack playedTrack;
    void play(const Gtk::TreeIter &iter);
    void playNext();
    void playPrev();
    void fillTrackTreeView();
    void updateSelection(const std::string &trackId);

    Glib::RefPtr<Gtk::Builder> builder;
    Gtk::TreeView *treeView            = nullptr;
    Gtk::TreeView *sideTreeView        = nullptr;
    Gtk::Scale *playbackProgressWidget = nullptr;
    Gtk::Label *trackLabel             = nullptr;
    Gtk::Label *timeLabel              = nullptr;
    Gtk::Spinner spinner;

    bool shouldHandleValueChanged = true;
    void scaleSetValue(double value);

  public:
    MainWindow(BaseObjectType *base, Glib::RefPtr<Gtk::Builder> &builder);
    ~MainWindow() override = default;
    friend struct TreeViewVisisbilityPredicate;

    void updatePlaybackProgress() override;
    void playbackFinished() override;
    void playbackStarted() override;
    void updateCacheProgress() override;
};

class LogWindow : public Gtk::Window
{
  public:
    LogWindow(BaseObjectType *base, Glib::RefPtr<Gtk::Builder> &builder);
    void setSession(Session *session) { this->session = session; }
    void present();

  private:
    Glib::RefPtr<Gtk::Builder> builder;
    Session *session = nullptr;

    Gtk::TextView *infoTextView       = nullptr;
    Gtk::Button *listAllDevicesButton = nullptr;
    Glib::Dispatcher signal_deviceListReady;
    Glib::Dispatcher signal_trackListReady;
};
}
}

#endif // MAIN_WINDOW_HPP
