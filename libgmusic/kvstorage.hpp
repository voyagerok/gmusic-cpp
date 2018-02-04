#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>

namespace gmusic
{

class KeyValueStorage
{
  public:
    KeyValueStorage(const std::string &basePath);
    ~KeyValueStorage();

    template <class T> void saveValueForKey(T &&value, const std::string &key)
    {
        getUserStorage().put(key, value);
    }

    template <class T> boost::optional<T> getValueForKey(const std::string &key)
    {
        return getUserStorage().get_optional<T>(key);
    }

    bool removeKey(const std::string &key);

    boost::property_tree::ptree &getUserStorage()
    {
        return ptree.get_child("User");
    }

    bool sync();

  private:
    boost::property_tree::ptree ptree;
    std::string keyFileName;
};
}

#endif
