#include "session.hpp"
#include "utilities.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>
#include <unordered_set>

namespace gmapi
{

using db::Database;

static const char *sessionTokenKey = "sessionToken";
static const char *emailKey        = "email";
static const char *deviceIdKey     = "deviceId";

Session::Session(const std::string &basicPath) : storage(basicPath)
{
    auto dbPath = basicPath + "/storage.sqlite";

    this->database = new Database(dbPath);

    auto token = storage.getValueForKey<std::string>(sessionTokenKey);
    if (token) {
        AuthCredentials credentials;
        credentials.email =
            storage.getValueForKey<std::string>(emailKey).value_or("");
        credentials.deviceId =
            storage.getValueForKey<std::string>(deviceIdKey).value_or("");
        credentials.authToken = token.value();
        api.updateCredentials(credentials);
    }
}

Session::~Session()
{
    if (database != nullptr) {
        delete database;
    }
}

void Session::login(const std::string &email,
                    const std::string &passwd,
                    const std::string &deviceId)
{
    api.login(email, passwd, deviceId);
    storage.saveValueForKey(api.getCredentials().authToken, sessionTokenKey);
    storage.saveValueForKey(api.getCredentials().email, emailKey);
    storage.saveValueForKey(api.getCredentials().deviceId, deviceIdKey);
    storage.sync();
}

void Session::logout()
{
    storage.removeKey(sessionTokenKey);
    storage.removeKey(emailKey);
    storage.removeKey(deviceIdKey);
    database->clear();
    api.clearCredentials();
}

struct CheckedEntities {
  private:
    using ProtectedStorage =
        std::pair<std::unordered_set<std::string>, std::mutex>;
    ProtectedStorage artistsStorage;
    ProtectedStorage albumsStorage;
    bool checkStorage(ProtectedStorage &storage, const std::string &id);
    void saveId(ProtectedStorage &storage, const std::string &id)
    {
        std::lock_guard<std::mutex> lock(storage.second);
        storage.first.insert(id);
    }

  public:
    bool checkAlbum(const std::string &id)
    {
        return checkStorage(albumsStorage, id);
    }

    bool checkArtist(const std::string &id)
    {
        return checkStorage(artistsStorage, id);
    }

    void saveAlbum(const std::string &id) { saveId(albumsStorage, id); }

    void saveArtist(const std::string &id) { saveId(artistsStorage, id); }
};

bool CheckedEntities::checkStorage(ProtectedStorage &storage,
                                   const std::string &id)
{
    std::lock_guard<std::mutex> lock(storage.second);
    auto &checkedIds = storage.first;
    return checkedIds.find(id) != checkedIds.end();
}

void Session::handleTracks(TrackStorageIter begin,
                           TrackStorageIter end,
                           CheckedEntities &entities,
                           std::unordered_set<Track> &cachedTracks,
                           std::atomic_bool *cancelFlag)
{
    for (auto iter = begin; iter != end; ++iter) {
        if (cancelFlag && *cancelFlag) {
            return;
        }
        if (cachedTracks.find(*iter) != cachedTracks.end()) {
            continue;
        }
        try {
            for (const auto &artistId : iter->artistIds) {
                if (!entities.checkArtist(artistId)) {
                    auto artist = api.getArtistApi().getArtist(artistId);
                    database->getArtistTable().insert(artist);
                    entities.saveArtist(artistId);
                }
            }
            if (!entities.checkAlbum(iter->albumId)) {
                auto album = api.getAlbumApi().getAlbum(iter->albumId);
                for (const auto &artistId : album.artistIds) {
                    if (!entities.checkArtist(artistId)) {
                        auto artist = api.getArtistApi().getArtist(artistId);
                        database->getArtistTable().insert(artist);
                        entities.saveArtist(artistId);
                    }
                }
                database->getAlbumTable().insert(album);
                entities.saveAlbum(iter->albumId);
            }
            database->getTrackTable().insert(*iter);
        } catch (const ApiRequestException &exc) {
            ERRLOG << exc.what() << std::endl;
        } catch (const std::exception &exc) {
            ERRLOG << exc.what() << std::endl;
        }
    }
}

void Session::updateLocalDataPrivate(std::atomic_bool *cancelFlag)
{
    auto allTracks = database->getTrackTable().getAll();

    std::unordered_set<Track> cachedTracks;
    std::move(allTracks.begin(),
              allTracks.end(),
              std::inserter(cachedTracks, cachedTracks.end()));

    auto tracks = api.getTrackApi().getTrackList();
    CheckedEntities entities;

    int tasknum    = std::thread::hardware_concurrency();
    long chunkSize = tracks.size() / tasknum;
    std::vector<std::future<void>> tasks;
    tasks.resize(tasknum);
    for (int i = 0; i < tasknum - 1; ++i) {
        tasks[i] = std::async(std::launch::async,
                              &Session::handleTracks,
                              this,
                              tracks.begin() + chunkSize * i,
                              tracks.begin() + chunkSize * (i + 1),
                              std::ref(entities),
                              std::ref(cachedTracks),
                              cancelFlag);
    }
    tasks[tasknum - 1] = std::async(std::launch::async,
                                    &Session::handleTracks,
                                    this,
                                    tracks.begin() + chunkSize * (tasknum - 1),
                                    tracks.begin() + chunkSize * tasknum +
                                        tracks.size() % tasknum,
                                    std::ref(entities),
                                    std::ref(cachedTracks),
                                    cancelFlag);
}

void Session::updateLocalData(std::atomic_bool *cancelFlag)
{
    using namespace std::chrono;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    updateLocalDataPrivate(cancelFlag);
    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(t2 - t1).count();
    STDLOG << "updateLocalData: " << duration << "ms" << std::endl;
}
}
