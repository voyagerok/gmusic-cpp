#ifndef DATABASE_HPP
#define DATABASE_HPP

#include "db/db-engine.hpp"
#include "model/model.hpp"
#include "operation-queue.hpp"
#include <functional>
#include <mutex>

namespace gmusic
{
namespace db
{

class Database;

template <class Database> class TableBase
{
  public:
    TableBase(Database *db) : db(db) {}

    Connection getConnection() const
    {
        Connection con(db->getPath());
        db->prepareConnection(&con);
        return con;
    }

    Database *getDatabase() const { return db; }

  private:
    Database *db;
};

class ArtistTable : protected TableBase<Database>
{
  public:
    using TableBase<Database>::TableBase;
    void insert(const Artist &artist);
    void insert(const std::vector<Artist> &models);
    void remove(const Artist &model);
    std::vector<Artist> getAll() const;
    Artist get(const std::string &id) const;
};

class AlbumTable : protected TableBase<Database>
{
  public:
    using TableBase<Database>::TableBase;
    void insert(const Album &album);
    void insert(const std::vector<Album> &models);
    void remove(const Album &model);
    std::vector<Album> getAll() const;
    std::vector<Album> getAllForArtist(const std::string &artistId) const;
    Album get(const std::string &id) const;
};

class TrackTable : protected TableBase<Database>
{
  public:
    enum class TrackType { All, Regular, Purchased };
    using TableBase<Database>::TableBase;
    void insert(const Track &artist);
    void insert(const std::vector<Track> &models);
    void remove(const Track &model);
    std::vector<Track> getAll() const;
    Track get(const std::string &id) const;
    std::vector<Track> getAll(TrackType trackType) const;
    std::vector<Track> getAllForArtist(TrackType trackType,
                                       const std::string &artistId) const;
    std::vector<Track> getAllForAlbum(TrackType trackType,
                                      const std::string &albumId) const;

  private:
    static std::string toString(TrackType trackType);
};

class Database
{
  public:
    Database(const std::string &dbPath);
    void prepareConnection(Connection *con) const;
    void clear();
    std::string getPath() const { return dbPath; }
    ArtistTable &getArtistTable() { return artistTable; }
    AlbumTable &getAlbumTable() { return albumTable; }
    TrackTable &getTrackTable() { return trackTable; }

    template <class Ret, class RWLockType>
    Ret perform(const std::function<Ret(Connection *)> &func)
    {
        RWLockType lock(&rwLockHandle);
        Connection con(getPath());
        prepareConnection(&con);
        return func(&con);
    }

  private:
    RWLockHandle rwLockHandle;
    void initialize();
    std::string dbPath;
    ArtistTable artistTable;
    AlbumTable albumTable;
    TrackTable trackTable;
    std::mutex mutex;
};
}
}

#endif // DATABASE_HPP
