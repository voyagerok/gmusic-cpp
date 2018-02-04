#ifndef UTILS_HPP_
#define UTILS_HPP_

#include <fstream>
#include <future>
#include <string>
#include <vector>

namespace gmusic
{
namespace SysUtils
{

std::string timeStringFromSeconds(int seconds);
}

namespace FSUtils
{
// std::string getDataDir();
// std::string getCacheDir();
// enum class EntryType { Regular, Other };
// struct DirectoryEntry {
//    std::string path;
//    std::string name;
//    long size;
//    EntryType type;
//};
// std::vector<DirectoryEntry> contentsOfDirectory(const std::string &dir);
// void createFile(const std::string &path);
// bool tryCreateFile(const std::string &path);
void createDirectoryIfNeeded(const std::string &path);
bool tryCreateDirectory(const std::string &path);
void deleteFile(const std::string &path);
bool isFileExists(const std::string &path);
}

namespace CryptoUtils
{

using Byte = unsigned char;
std::string base64_encode(Byte *source, int slen, bool urlsafe = false);
std::vector<Byte> base64_decode(const char *source, int slen);

std::string encryptLoginAndPasswd(const std::string &login,
                                  const std::string &passwd);
std::pair<std::string, std::string> encryptTrackId(const std::string &trackId);
}

namespace NetUtils
{
std::string urlEncode(const std::string &str);
// std::string getMacAddress();
}

namespace StringUtils
{
std::vector<std::string> split(const std::string &str, char delim);
uint64_t unsignedLongFromString(const std::string &str);
}

namespace ThreadUtils
{
template <class Func, class... Args>
std::future<std::result_of_t<std::decay_t<Func>(std::decay_t<Args>...)>>
deferred_async(Func &&f, Args &&... args)
{
    return std::async(std::launch::deferred,
                      std::forward<Func>(f),
                      std::forward<Args>(args)...);
}

template <class T, class Func>
std::future<void> then(std::future<T> &&task,
                       Func &&func,
                       std::launch policy = std::launch::async)
{
    return std::async(
        policy, [ task = std::move(task), func ] { func(std::move(task)); });
}

template <class T, class Func>
auto then(const std::shared_future<T> &task,
          Func &&func,
          std::launch policy = std::launch::async)
    -> std::future<decltype(func(task))>
{
    return std::async(policy, [task, func] { return func(task); });
}
}

class LogStream
{
  public:
    LogStream(std::ostream &os) : os{os} {}
    template <typename T> const LogStream &operator<<(const T &v) const
    {
        std::lock_guard<std::mutex> lock{mutex};
        os << v;
        return *this;
    }
    const LogStream &operator<<(std::ostream &(*func)(std::ostream &)) const
    {
        std::lock_guard<std::mutex> lock{mutex};
        func(os);
        return *this;
    }

  private:
    std::ostream &os;
    mutable std::mutex mutex;
};

struct Logger {
    static LogStream &getStdLogger();
    static LogStream &getErrorLogger();
};

#define STDLOG Logger::getStdLogger()
#define ERRLOG Logger::getErrorLogger()
#define LOCATE(os)                                                             \
    os << "In file " << __FILE__ << ", at line " << __LINE__ << ": "
}

#endif // UTILS_HPP_
