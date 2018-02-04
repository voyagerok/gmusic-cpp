#ifndef MODEL_HPP_
#define MODEL_HPP_

#include <string>
#include <vector>

namespace gmusic
{

struct Device {
    std::string deviceId;
    std::string friendlyName;
    std::string deviceType;
    uint64_t lastAccessTime;
};

struct Track {
    std::string name;
    std::string albumId;
    std::vector<std::string> artistIds;
    std::string trackId;
    std::string genre;
    std::string trackType;
    uint64_t msDuration;
    int trackNumber;
    int year;
    uint64_t size;
    bool operator==(const Track &other) const;
};

struct Album {
    std::string albumId;
    std::string name;
    std::vector<std::string> artistIds;
    std::string artUrl;
    std::vector<std::string> trackIds;
    int year;
    std::string descr;
};

struct Artist {
    std::string artistId;
    std::string name;
    std::string artUrl;
    std::string bio;
    std::vector<std::string> albums;
};

inline bool Track::operator==(const Track &other) const
{
    return trackId == other.trackId;
}
}

namespace std
{

template <> struct hash<gmusic::Track> {
    size_t operator()(const gmusic::Track &track) const
    {
        return std::hash<std::string>()(track.trackId);
    }
};
}

#endif // MODEL_HPP_
