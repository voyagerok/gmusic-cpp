#include "kvstorage.hpp"
#include "utilities.hpp"
#include <boost/property_tree/ini_parser.hpp>
#include <iostream>

namespace pt = boost::property_tree;

namespace gmusic
{

KeyValueStorage::KeyValueStorage(const std::string &basePath)
    : keyFileName(basePath + "data.ini")
{
    std::fstream dataFstream(keyFileName);
    pt::read_ini(dataFstream, ptree);
    auto userSection = ptree.get_child_optional("User");
    if (userSection == boost::none) {
        ptree.put_child("User", pt::ptree());
    }
}

KeyValueStorage::~KeyValueStorage() { sync(); }

bool KeyValueStorage::removeKey(const std::string &key)
{
    auto &userStorage = getUserStorage();
    auto result       = userStorage.find(key);
    if (result != userStorage.not_found()) {
        userStorage.erase(ptree.to_iterator(result));
        return true;
    }
    return false;
}

bool KeyValueStorage::sync()
{
    try {
        pt::write_ini(keyFileName, ptree);
        return true;
    } catch (const pt::ini_parser_error &err) {
        ERRLOG << err.what() << std::endl;
        return false;
    } catch (...) {
        return false;
    }
}
}
