#include "db/database.hpp"
#include "utilities.hpp"

namespace gmusic
{
namespace db
{

Database::Database(const std::string &dbPath)
    : dbPath{dbPath}, artistTable{this}, albumTable{this}, trackTable{this}
{
    initialize();
}

void Database::clear()
{
    FSUtils::deleteFile(dbPath);
    initialize();
}

void Database::initialize()
{
    perform<void, WriteLock>([](Connection *con) {
        Statement::executeQuery(con, "BEGIN");
        Statement::executeQuery(con,
                                "CREATE TABLE IF NOT EXISTS Artist(id TEXT "
                                "PRIMARY KEY, name TEXT, artUrl TEXT, bio "
                                "TEXT)");
        Statement::executeQuery(con,
                                "CREATE TABLE IF NOT EXISTS Album(id TEXT "
                                "PRIMARY KEY, name TEXT, artUrl TEXT, descr "
                                "TEXT, year INTEGER)");
        Statement::executeQuery(con,
                                "CREATE TABLE IF NOT EXISTS "
                                "Artist2Album(artistId REFERENCES Artist(id), "
                                "albumId REFERENCES Album(id))");
        Statement::executeQuery(
            con,
            "CREATE TABLE IF NOT EXISTS Track(id TEXT PRIMARY KEY,"
            "albumId REFERENCES Album(id),"
            "name TEXT,"
            "genre TEXT,"
            "duration INTEGER,"
            "trackNumber INTEGER,"
            "year INTEGER,"
            "trackType TEXT)");
        Statement::executeQuery(con,
                                "CREATE TABLE IF NOT EXISTS "
                                "Track2Artist(trackId REFERENCES Track(id), "
                                "artistId REFERENCES Artist(id))");
        Statement::executeQuery(con, "COMMIT");
    });
}

void Database::prepareConnection(Connection *con) const
{
    Statement::executeQuery(con, "PRAGMA foreign_keys = ON");
}

static void insertArtist(db::Connection *con, const Artist &item)
{
    auto insertArtistQuery =
        "insert or replace into Artist(id, name, artUrl, bio) values(?,?,?,?)";
    Statement::executeQuery(con,
                            insertArtistQuery,
                            item.artistId,
                            item.name,
                            item.artUrl,
                            item.bio);
}

void ArtistTable::insert(const Artist &artist)
{
    getDatabase()->perform<void, WriteLock>(
        [&artist](Connection *con) { insertArtist(con, artist); });
}

void ArtistTable::insert(const std::vector<Artist> &artists)
{
    getDatabase()->perform<void, WriteLock>([&artists](Connection *con) {
        Statement::executeQuery(con, "BEGIN");
        for (const auto &artist : artists)
            insertArtist(con, artist);
        Statement::executeQuery(con, "COMMIT");
    });
}

void ArtistTable::remove(const Artist &artist)
{
    getDatabase()->perform<void, WriteLock>([&artist](Connection *con) {
        Statement::executeQuery(con, "BEGIN");
        Statement::executeQuery(con,
                                "delete from Artist2Album where artistId = ?",
                                artist.artistId);
        Statement::executeQuery(con,
                                "delete from Track2Artist where artistId = ?",
                                artist.artistId);
        Statement::executeQuery(
            con, "delete from Artist where id = ?", artist.artistId);
        Statement::executeQuery(con, "COMMIT");
    });
}

Artist ArtistTable::get(const std::string &id) const
{
    return getDatabase()->perform<Artist, ReadLock>(
        [&id](Connection *con) -> Artist {
            Artist artist;

            Statement::executeQuery(con, "BEGIN");

            Statement st{con,
                         "select a.id, a.name, a.artUrl, a.bio, aa.albumId "
                         "from Artist a "
                         "join Artist2Album aa on a.id = aa.artistId "
                         "where a.id = ?"};
            st.bind(id);
            if (st.executeStep()) {
                artist.artistId = st.get<std::string>(0);
                artist.name     = st.get<std::string>(1);
                artist.artUrl   = st.get<std::string>(2);
                artist.bio      = st.get<std::string>(3);
                artist.albums.push_back(st.get<std::string>(4));
            }
            while (st.executeStep()) {
                artist.albums.push_back(st.get<std::string>(4));
            }

            Statement::executeQuery(con, "COMMIT");

            return artist;
        });
}

std::vector<Artist> ArtistTable::getAll() const
{
    using ArtistList = std::vector<Artist>;
    return getDatabase()->perform<ArtistList, ReadLock>(
        [](Connection *con) -> ArtistList {
            std::vector<Artist> result;

            Statement::executeQuery(con, "BEGIN");

            Statement st{con,
                         "select id, name, artUrl, bio "
                         "from Artist a"};
            Statement albumsSt{
                con, "select albumId from Artist2Album where artistId = ?"};
            while (st.executeStep()) {
                Artist artist{st.get<std::string>(0),
                              st.get<std::string>(1),
                              st.get<std::string>(2),
                              st.get<std::string>(3),
                              std::vector<std::string>()};
                albumsSt.bind(artist.artistId);
                while (albumsSt.executeStep()) {
                    artist.albums.push_back(albumsSt.get<std::string>(0));
                }
                albumsSt.reset();

                result.push_back(artist);
            }

            Statement::executeQuery(con, "COMMIT");

            return result;
        });
}

static void insertAlbum(db::Connection *con, const Album &album)
{
    Statement::executeQuery(con,
                            "insert or replace into Album(id, name, artUrl, "
                            "descr, year) values(?,?,?,?,?)",
                            album.albumId,
                            album.name,
                            album.artUrl,
                            album.descr,
                            album.year);

    Statement st(con,
                 "insert into Artist2Album (albumId, artistId) values(?, ?)");
    for (const auto &artistId : album.artistIds) {
        st.bind(album.albumId, artistId);
        st.execute();
        st.reset();
    }
}

void AlbumTable::insert(const Album &album)
{
    getDatabase()->perform<void, WriteLock>([&album](Connection *con) {
        Statement::executeQuery(con, "BEGIN");
        insertAlbum(con, album);
        Statement::executeQuery(con, "COMMIT");
    });
}

void AlbumTable::insert(const std::vector<Album> &albums)
{
    getDatabase()->perform<void, WriteLock>([&albums](Connection *con) {
        Statement::executeQuery(con, "BEGIN");
        for (const auto &album : albums) {
            insertAlbum(con, album);
        }
        Statement::executeQuery(con, "COMMIT");
    });
}

void AlbumTable::remove(const Album &album)
{
    getDatabase()->perform<void, WriteLock>([&album](Connection *con) {
        Statement::executeQuery(con, "BEGIN");
        Statement::executeQuery(
            con, "delete from Artist2Album where albumId = ?", album.albumId);
        Statement::executeQuery(
            con, "delete from Track where albumId = ?", album.albumId);
        Statement::executeQuery(
            con, "delete from Album where id = ?", album.albumId);
        Statement::executeQuery(con, "COMMIT");
    });
}

Album AlbumTable::get(const std::string &id) const
{
    return getDatabase()->perform<Album, ReadLock>(
        [&id](Connection *con) -> Album {
            Statement::executeQuery(con, "BEGIN");
            Album album;
            Statement st(
                con,
                "select id, name, artUrl, descr, year from Album where id = ?");
            st.bind(id);
            while (st.executeStep()) {
                album.albumId = st.get<std::string>(0);
                album.name    = st.get<std::string>(1);
                album.artUrl  = st.get<std::string>(2);
                album.descr   = st.get<std::string>(3);
                album.year    = st.get<int>(4);
            }
            Statement selectArtistsSt(
                con,
                "select distinct artistId from Artist2Album where albumId = ?");
            selectArtistsSt.bind(id);
            while (selectArtistsSt.executeStep()) {
                album.artistIds.push_back(selectArtistsSt.get<std::string>(0));
            }
            Statement::executeQuery(con, "COMMIT");
            return album;
        });
}

std::vector<Album> AlbumTable::getAll() const
{
    using AlbumList = std::vector<Album>;
    return getDatabase()->perform<AlbumList, ReadLock>(
        [](Connection *con) -> AlbumList {
            std::vector<Album> albums;

            Statement::executeQuery(con, "BEGIN");

            Statement st(con,
                         "select id, name, artUrl, descr, year from Album");
            while (st.executeStep()) {
                Album album;
                album.albumId = st.get<std::string>(0);
                album.name    = st.get<std::string>(1);
                album.artUrl  = st.get<std::string>(2);
                album.descr   = st.get<std::string>(3);
                album.year    = st.get<int>(4);
                albums.push_back(std::move(album));
            }

            Statement::executeQuery(con, "COMMIT");

            return albums;
        });
}

std::vector<Album>
AlbumTable::getAllForArtist(const std::string &artistId) const
{
    using AlbumList = std::vector<Album>;

    return getDatabase()->perform<AlbumList, ReadLock>(
        [&artistId](Connection *con) -> AlbumList {
            std::vector<Album> albums;

            Statement::executeQuery(con, "BEGIN");

            Statement st(con,
                         "select a.id, a.name, a.artUrl, a.descr, a.year, "
                         "b.artistId from Album a join Artist2Album b on (id = "
                         "albumId) where b.artistId = ?");
            st.bind(artistId);
            while (st.executeStep()) {
                Album album;
                album.albumId = st.get<std::string>(0);
                album.name    = st.get<std::string>(1);
                album.artUrl  = st.get<std::string>(2);
                album.descr   = st.get<std::string>(3);
                album.year    = st.get<int>(4);
                albums.push_back(std::move(album));
            }

            Statement::executeQuery(con, "COMMIT");

            return albums;
        });
}

static void insertTrack(db::Connection *con, const Track &track)
{
    Statement::executeQuery(con,
                            "insert or replace into Track(id, albumId, name, "
                            "genre, duration, trackNumber, year, trackType) "
                            "values(?, ?, ?, ?, ?, ?, ?, ?)",
                            track.trackId,
                            track.albumId,
                            track.name,
                            track.genre,
                            track.msDuration,
                            track.trackNumber,
                            track.year,
                            track.trackType);
    Statement st(
        con,
        "insert or replace into Track2Artist(trackId, artistId) values(?, ?)");
    for (const auto &artistId : track.artistIds) {
        st.bind(track.trackId, artistId);
        st.execute();
        st.reset();
    }
}

void TrackTable::insert(const Track &track)
{
    getDatabase()->perform<void, WriteLock>([&track](Connection *con) {
        Statement::executeQuery(con, "BEGIN");
        insertTrack(con, track);
        Statement::executeQuery(con, "COMMIT");
    });
}

void TrackTable::insert(const std::vector<Track> &tracks)
{
    getDatabase()->perform<void, WriteLock>([&tracks](Connection *con) {
        Statement::executeQuery(con, "BEGIN");
        for (const auto &track : tracks) {
            insertTrack(con, track);
        }
        Statement::executeQuery(con, "COMMIT");
    });
}

void TrackTable::remove(const Track &track)
{
    getDatabase()->perform<void, WriteLock>([&track](Connection *con) {
        Statement::executeQuery(con, "BEGIN");
        Statement::executeQuery(
            con, "delete from Track2Artist where trackId = ?", track.trackId);
        Statement::executeQuery(
            con, "delete from Track where id = ?", track.trackId);
        Statement::executeQuery(con, "COMMIT");
    });
}

Track TrackTable::get(const std::string &id) const
{
    using std::string;

    return getDatabase()->perform<Track, ReadLock>(
        [&id](Connection *con) -> Track {
            Track track;
            Statement::executeQuery(con, "BEGIN");

            Statement st(
                con,
                "select id, albumId, name, genre, duration, trackNumber, "
                "year, trackType from Track where id = ?");
            st.bind(id);
            while (st.executeStep()) {
                track.trackId     = st.get<string>(0);
                track.albumId     = st.get<string>(1);
                track.name        = st.get<string>(2);
                track.genre       = st.get<string>(3);
                track.msDuration  = static_cast<uint64_t>(st.get<int>(4));
                track.trackNumber = st.get<int>(5);
                track.year        = st.get<int>(6);
                track.trackType   = st.get<string>(7);
            }

            Statement artistSt(
                con, "select artistId from Track2Artist where trackId = ?");
            artistSt.bind(id);
            while (artistSt.executeStep()) {
                track.artistIds.push_back(artistSt.get<string>(0));
            }
            Statement::executeQuery(con, "COMMIT");

            return track;
        });
}

std::vector<Track> TrackTable::getAll(TrackType trackType) const
{
    using std::string;
    using TrackList = std::vector<Track>;

    return getDatabase()->perform<TrackList, ReadLock>(
        [trackType](Connection *con) -> TrackList {
            std::vector<Track> tracks;
            Statement::executeQuery(con, "BEGIN");

            std::string statementStr =
                "select id, albumId, name, genre, duration, "
                "trackNumber, year, trackType from Track";
            std::string trackTypeStr = toString(trackType);
            if (!trackTypeStr.empty()) {
                statementStr += " where trackType = " + trackTypeStr;
            }
            Statement st(con, statementStr);
            while (st.executeStep()) {
                Track track;
                track.trackId     = st.get<string>(0);
                track.albumId     = st.get<string>(1);
                track.name        = st.get<string>(2);
                track.genre       = st.get<string>(3);
                track.msDuration  = static_cast<uint64_t>(st.get<int>(4));
                track.trackNumber = st.get<int>(5);
                track.year        = st.get<int>(6);
                track.trackType   = st.get<string>(7);
                tracks.push_back(std::move(track));
            }
            Statement::executeQuery(con, "COMMIT");

            return tracks;
        });
}

std::vector<Track> TrackTable::getAllForAlbum(TrackType trackType,
                                              const std::string &albumId) const
{
    using std::string;
    using TrackList = std::vector<Track>;

    return getDatabase()->perform<TrackList, ReadLock>(
        [trackType, albumId](Connection *con) -> TrackList {
            std::vector<Track> tracks;
            Statement::executeQuery(con, "BEGIN");

            std::string statementStr = "select id, albumId, name, genre, "
                                       "duration, trackNumber, year, trackType "
                                       "from Track where albumId = ?";
            std::string trackTypeStr = toString(trackType);
            if (!trackTypeStr.empty()) {
                statementStr += " and trackType = " + trackTypeStr;
            }
            Statement st(con, statementStr);
            st.bind(albumId);
            while (st.executeStep()) {
                Track track;
                track.trackId     = st.get<string>(0);
                track.albumId     = st.get<string>(1);
                track.name        = st.get<string>(2);
                track.genre       = st.get<string>(3);
                track.msDuration  = static_cast<uint64_t>(st.get<int>(4));
                track.trackNumber = st.get<int>(5);
                track.year        = st.get<int>(6);
                track.trackType   = st.get<string>(7);
                tracks.push_back(std::move(track));
            }
            Statement::executeQuery(con, "COMMIT");

            return tracks;
        });
}

std::vector<Track>
TrackTable::getAllForArtist(TrackType trackType,
                            const std::string &artistId) const
{
    using std::string;
    using TrackList = std::vector<Track>;

    return getDatabase()->perform<TrackList, ReadLock>(
        [trackType, artistId](Connection *con) -> TrackList {
            std::vector<Track> tracks;
            Statement::executeQuery(con, "BEGIN");

            std::string statementStr = "select id, albumId, name, genre, "
                                       "duration, trackNumber, year, trackType "
                                       "from Track a join Track2Artist b on "
                                       "(a.id = b.trackId and b.artistId = ?)";
            std::string trackTypeStr = toString(trackType);
            if (!trackTypeStr.empty()) {
                statementStr += " where trackType = " + trackTypeStr;
            }
            Statement st(con, statementStr);
            st.bind(artistId);
            while (st.executeStep()) {
                Track track;
                track.trackId     = st.get<string>(0);
                track.albumId     = st.get<string>(1);
                track.name        = st.get<string>(2);
                track.genre       = st.get<string>(3);
                track.msDuration  = static_cast<uint64_t>(st.get<int>(4));
                track.trackNumber = st.get<int>(5);
                track.year        = st.get<int>(6);
                track.trackType   = st.get<string>(7);
                tracks.push_back(std::move(track));
            }
            Statement::executeQuery(con, "COMMIT");

            return tracks;
        });
}

std::vector<Track> TrackTable::getAll() const
{
    using std::string;
    using TrackList = std::vector<Track>;

    return getDatabase()->perform<TrackList, ReadLock>(
        [](Connection *con) -> TrackList {
            std::vector<Track> tracks;
            Statement::executeQuery(con, "BEGIN");

            Statement st(
                con,
                "select id, albumId, name, genre, duration, trackNumber, "
                "year, trackType from Track");
            while (st.executeStep()) {
                Track track;
                track.trackId     = st.get<string>(0);
                track.albumId     = st.get<string>(1);
                track.name        = st.get<string>(2);
                track.genre       = st.get<string>(3);
                track.msDuration  = static_cast<uint64_t>(st.get<int>(4));
                track.trackNumber = st.get<int>(5);
                track.year        = st.get<int>(6);
                track.trackType   = st.get<string>(7);
                tracks.push_back(std::move(track));
            }
            Statement::executeQuery(con, "COMMIT");

            return tracks;
        });
}

std::string TrackTable::toString(TrackType trackType)
{
    switch (trackType) {
    case TrackType::All:
        return "";
    case TrackType::Purchased:
        return "4";
    case TrackType::Regular:
        return "8";
    }
    return "";
}
}
}
