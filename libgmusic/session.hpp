#ifndef SESSION_HPP
#define SESSION_HPP

#include "api/gmapi.hpp"
#include "db/database.hpp"
#include "kvstorage.hpp"
#include "operation-queue.hpp"
#include <string>
#include <unordered_set>

namespace gmusic
{

struct CheckedEntities;

class Session
{
  public:
    using UpdateCallback = std::function<void(std::shared_future<void>)>;
    using OpenCallback   = std::function<void(std::shared_future<void>)>;

    Session(const std::string &basicPath);
    ~Session();
    void login(const std::string &email,
               const std::string &passwd,
               const std::string &deviceId = std::string());
    void logout();
    GMApi *getApi() { return &api; }
    db::Database *getDatabase() { return database; }
    bool isAuthorized() { return api.isLoggedIn(); }
    void updateLocalData(std::atomic_bool *cancelFlag);

    KeyValueStorage &getStorage() { return storage; }

    TaskBuilder taskBuilder;

  private:
    using TrackStorageIter = std::vector<Track>::iterator;
    void updateLocalDataPrivate(std::atomic_bool *cancelFlag);
    void handleTracks(TrackStorageIter begin,
                      TrackStorageIter end,
                      CheckedEntities &,
                      std::unordered_set<Track> &,
                      std::atomic_bool *cancelFlag);
    db::Database *database = nullptr;
    GMApi api;
    KeyValueStorage storage;
};
}

#endif
