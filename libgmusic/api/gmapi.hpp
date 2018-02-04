#ifndef GMAPI_HPP_
#define GMAPI_HPP_

#include <string>
#include <future>
//#include <boost/thread/future.hpp>
#include "http/httpsession.hpp"
#include "model/model.hpp"
#include "operation-queue.hpp"

namespace gmusic {

class GMApi;

struct ApiRequestException: std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct ApiRequestHttpException: ApiRequestException {
    ApiRequestHttpException(const HttpError &error);
    HttpError error;
};

struct AuthCredentials {
    std::string authToken;
    std::string email;
    std::string deviceId;
};

class LoginApi {
public:
    using LoginCallback = std::function<void(AuthCredentials)>;
    AuthCredentials login(const std::string &email, const std::string &passwd, const std::string &deviceId = std::string());
    std::future<AuthCredentials> loginAsync(const std::string &email, const std::string &passwd, const std::string &deviceId = std::string());
};

class AlbumApi {
public:
    using AlbumCallback = std::function<void(Album)>;
    AlbumApi(GMApi *baseApi): baseApi { baseApi } {}
    Album getAlbum(const std::string &albumId);
    std::future<Album> getAlbumAsync(const std::string &albumId);
private:
    GMApi *baseApi;
};

class ArtistApi {
public:
    ArtistApi(GMApi *baseApi): baseApi { baseApi } {}
    Artist getArtist(const std::string &artistId);
    std::future<Artist> getArtistAsync(const std::string &id);
private:
    GMApi *baseApi;
};


using DeviceList = std::vector<Device>;
class DMApi {
public:
    DMApi(GMApi *api): baseApi { api } {}
    DeviceList getRegisteredDevices();
    std::future<DeviceList> getRegisteredDevicesAsync();
private:
    GMApi *baseApi;
};

using TrackList = std::vector<Track>;
class TrackApi {
public:
    using TrackListCallback = std::function<void(TrackList)>;

    TrackApi(GMApi *baseApi);
    TrackList getTrackList();
    std::future<TrackList> getTrackListAsync();
    std::string getStreamUrl(const std::string &trackId);
private:
    GMApi *baseApi;
};

class GMApi {
public:
    GMApi();

    void updateCredentials(const AuthCredentials &credentials);
//    HttpSession *getApiSession();
    HttpSession getApiSession();
    void prepareRequest(HttpRequest &request);
    std::string getBaseUrl() const;
    void login(const std::string &email, const std::string &passwd, const std::string &deviceId);
    std::future<void> loginAsync(const std::string &email, const std::string &passwd, const std::string &deviceId = std::string());

    DMApi &getDeviceApi() { return dmApi; }
    LoginApi &getLoginApi() { return loginApi; }
    TrackApi &getTrackApi() { return trackApi; }
    AlbumApi &getAlbumApi() { return albumApi; }
    ArtistApi &getArtistApi() { return artistApi; }

    bool isLoggedIn() { return !credentials.authToken.empty(); }

    AuthCredentials &getCredentials() { return credentials; }
    void clearCredentials() { credentials = AuthCredentials(); }
private:
    std::mutex mutex;
    DMApi dmApi;
    LoginApi loginApi;
    TrackApi trackApi;
    AlbumApi albumApi;
    ArtistApi artistApi;
    bool isAuthorized = false;
//    HttpSession sharedSession;
    AuthCredentials credentials;
};

}

#endif //GMAPI_HPP_
