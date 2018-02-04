#include "api/gmapi.hpp"
#include "utilities.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <cassert>
#include <future>

namespace gmapi
{

namespace pt = boost::property_tree;

TrackApi::TrackApi(GMApi *baseApi) : baseApi(baseApi) {}

static const std::string baseUrl = "https://mclients.googleapis.com/sj/v2.5/";

inline static void setupApiSession(HttpSession &session,
                                   const std::string &authToken)
{
    session.setHeaderParam("User-Agent", "gm-player/1.0");
    session.setHeaderParam("Authorization", "GoogleLogin auth=" + authToken);
}

static HttpError checkPayloadForError(const pt::ptree &root)
{
    auto errorCode = root.get_child_optional("error.code");
    auto errorMsg  = root.get_child_optional("error.message");
    if (errorCode != boost::none && errorMsg != boost::none) {
        return HttpError::createFromStatusCode(
            errorCode.value().get_value<int>(),
            errorMsg.value().get_value<std::string>());
    }
    return HttpError();
}

ApiRequestHttpException::ApiRequestHttpException(const HttpError &error)
    : ApiRequestException(error.message), error{error}
{
}

GMApi::GMApi() : dmApi{this}, trackApi{this}, albumApi{this}, artistApi{this} {}

void GMApi::updateCredentials(const AuthCredentials &credentials)
{
    std::lock_guard<std::mutex> lock(mutex);
    this->credentials = credentials;
    //    setupApiSession(sharedSession, credentials.authToken);
    isAuthorized = true;
}

std::string GMApi::getBaseUrl() const { return baseUrl; }

void GMApi::prepareRequest(HttpRequest &request)
{
    request.addParameter("dv", "0");
    request.addParameter("hl", "en_US");
    request.addParameter("tier", "fr");
}

HttpSession GMApi::getApiSession()
{
    HttpSession session;
    setupApiSession(session, credentials.authToken);
    return session;
}

// HttpSession *GMApi::getApiSession() {
//    return &sharedSession;
//}

void GMApi::login(const std::string &email,
                  const std::string &passwd,
                  const std::string &deviceId)
{
    auto credentials = getLoginApi().login(email, passwd, deviceId);
    updateCredentials(credentials);
}

std::future<void> GMApi::loginAsync(const std::string &email,
                                    const std::string &passwd,
                                    const std::string &deviceId)
{
    return std::async(
        std::launch::async, &GMApi::login, this, email, passwd, deviceId);
}

std::future<Album> AlbumApi::getAlbumAsync(const std::string &id)
{
    return std::async(std::launch::async, &AlbumApi::getAlbum, this, id);
}

Album AlbumApi::getAlbum(const std::string &id)
{
    static std::string targetUrl = baseApi->getBaseUrl() + "fetchalbum";

    assert(!id.empty());

    HttpRequest request{HttpMethod::GET, targetUrl};
    request.addParameter("nid", id);
    request.addParameter("include-tracks", "false");
    baseApi->prepareRequest(request);

    HttpSession apiSession = baseApi->getApiSession();
    HttpResponse response  = apiSession.makeRequest(request);
    //    HttpResponse response =
    //    baseApi->getApiSession()->makeRequest(request);

    //    return baseApi->performAsyncRequest<Album>(request, [=](const
    //    HttpResponse &response){
    if (response.error.code != HttpErrorCode::OK) {
        throw ApiRequestException(response.error.message);
    }

    pt::ptree root;
    std::istringstream istr(response.text);
    pt::read_json(istr, root);

    auto resultError = checkPayloadForError(root);
    if (resultError.code != HttpErrorCode::OK) {
        throw ApiRequestHttpException(resultError);
    }

    Album album;

    album.albumId = root.get<std::string>("albumId");
    album.name =
        root.get_optional<std::string>("name").value_or("Untitled album");
    album.artUrl = root.get_optional<std::string>("albumArtRef").value_or("");
    auto &artistIds = root.get_child("artistId");
    for (const auto &artistId : artistIds) {
        album.artistIds.push_back(artistId.second.get_value<std::string>());
    }

    album.descr = root.get_optional<std::string>("description").value_or("");
    album.year  = root.get_optional<int>("year").value_or(1970);

    return album;
    //    });
}

std::future<Artist> ArtistApi::getArtistAsync(const std::string &id)
{
    return std::async(std::launch::async, &ArtistApi::getArtist, this, id);
}

Artist ArtistApi::getArtist(const std::string &id)
{
    static std::string targetUrl = baseApi->getBaseUrl() + "fetchartist";

    assert(!id.empty());

    HttpRequest request{HttpMethod::GET, targetUrl};
    request.addParameter("nid", id);
    request.addParameter("include-albums", "true");
    baseApi->prepareRequest(request);

    HttpSession apiSession = baseApi->getApiSession();
    HttpResponse response  = apiSession.makeRequest(request);
    if (response.error.code != HttpErrorCode::OK) {
        throw ApiRequestException(response.error.message);
    }

    pt::ptree root;
    std::istringstream istr(response.text);
    pt::read_json(istr, root);

    auto resultError = checkPayloadForError(root);
    if (resultError.code != HttpErrorCode::OK) {
        throw ApiRequestHttpException(resultError);
    }

    //        STD_LOG << root << std::endl;
    Artist artist;

    artist.name =
        root.get_optional<std::string>("name").value_or("Unknown artist");
    artist.bio    = root.get_optional<std::string>("artistBio").value_or("");
    artist.artUrl = root.get_optional<std::string>("artistArtRef").value_or("");
    artist.artistId = root.get<std::string>("artistId");

    return artist;
}

DeviceList DMApi::getRegisteredDevices()
{
    static std::string requestUrl =
        baseApi->getBaseUrl() + "devicemanagementinfo";

    HttpRequest request{HttpMethod::GET, requestUrl};
    baseApi->prepareRequest(request);

    HttpSession apiSession = baseApi->getApiSession();
    HttpResponse response  = apiSession.makeRequest(request);
    //    auto response = baseApi->getApiSession()->makeRequest(request);

    //    return baseApi->performAsyncRequest<DeviceList>(request, [=](const
    //    HttpResponse &response){
    if (response.error.code != HttpErrorCode::OK) {
        throw ApiRequestException(response.error.message);
    }

    std::vector<Device> devices;
    pt::ptree root;
    std::istringstream istr(response.text);
    pt::read_json(istr, root);

    auto resultError = checkPayloadForError(root);
    if (resultError.code != HttpErrorCode::OK) {
        throw ApiRequestHttpException(resultError);
    }
    const auto &items = root.get_child("data.items");

    for (const auto &deviceItem : items) {

        Device device;
        device.deviceId = deviceItem.second.get<std::string>("id");
        device.friendlyName =
            deviceItem.second.get_optional<std::string>("friendlyName")
                .value_or("Unknown device");
        device.deviceType =
            deviceItem.second.get_optional<std::string>("type").value_or("");
        auto lastAccessTime =
            deviceItem.second.get_optional<std::string>("lastAccessedTimeMs")
                .value_or("0");
        device.lastAccessTime =
            StringUtils::unsignedLongFromString(lastAccessTime);

        devices.push_back(std::move(device));
    }
    return devices;
    //    });
}

std::future<DeviceList> DMApi::getRegisteredDevicesAsync()
{
    return std::async(std::launch::async, &DMApi::getRegisteredDevices, this);
}

std::future<TrackList> TrackApi::getTrackListAsync()
{
    return std::async(std::launch::async, &TrackApi::getTrackList, this);
}

TrackList TrackApi::getTrackList()
{
    static std::string targetUrl = baseApi->getBaseUrl() + "trackfeed";

    HttpRequest request{HttpMethod::POST, targetUrl};
    baseApi->prepareRequest(request);

    HttpSession apiSession = baseApi->getApiSession();
    HttpResponse response  = apiSession.makeRequest(request);

    //    HttpResponse response =
    //    baseApi->getApiSession()->makeRequest(request);

    //    return baseApi->performAsyncRequest<TrackList>(request, [=](const
    //    HttpResponse &response){
    if (response.error.code != HttpErrorCode::OK) {
        throw ApiRequestException(response.error.message);
    }

    pt::ptree root;
    std::istringstream istr(response.text);
    pt::read_json(istr, root);

    const auto &resultError = checkPayloadForError(root);
    if (resultError.code != HttpErrorCode::OK) {
        throw ApiRequestHttpException(resultError);
    }

    const auto &items = root.get_child("data.items");

    TrackList trackList;
    STDLOG << "Tracks count: " << items.size() << std::endl;
    for (const auto &item : items) {
        Track track;

        track.name = item.second.get_optional<std::string>("title").value_or(
            "Untitled track");
        track.trackId = item.second.get<std::string>("id");

        track.albumId =
            item.second.get_optional<std::string>("albumId").value_or("");
        track.genre =
            item.second.get_optional<std::string>("genre").value_or("");
        auto duration = item.second.get_optional<std::string>("durationMillis")
                            .value_or("0");
        track.msDuration = StringUtils::unsignedLongFromString(duration);
        track.trackNumber =
            item.second.get_optional<int>("trackNumber").value_or(1);
        track.year = item.second.get_optional<int>("year").value_or(1970);
        track.trackType =
            item.second.get_optional<std::string>("trackType").value_or("8");
        auto trackSize = item.second.get_optional<std::string>("estimatedSize")
                             .value_or("0");
        track.size = StringUtils::unsignedLongFromString(trackSize);

        const auto &artistIds = item.second.get_child_optional("artistId");
        if (artistIds) {
            auto &artistIdsArray = artistIds.value();
            for (const auto &artistId : artistIdsArray) {
                track.artistIds.push_back(
                    artistId.second.get_value<std::string>());
            }
        }

        trackList.push_back(std::move(track));
    }
    return trackList;
    //    });
}

std::string TrackApi::getStreamUrl(const std::string &trackId)
{
    static std::string targetUrl =
        "https://mclients.googleapis.com/music/mplay";

    //    auto encryptedId = CryptoUtils::encryptTrackId(trackId);
    auto result = CryptoUtils::encryptTrackId(trackId);
    auto &sig   = result.first;
    auto &salt  = result.second;

    HttpRequest request{HttpMethod::GET, targetUrl};
    request.addParameter("opt", "hi");
    request.addParameter("net", "mob");
    request.addParameter("pt", "e");
    request.addParameter("sig", std::string(sig.begin(), sig.end() - 1));
    request.addParameter("slt", salt);
    request.addParameter("songid", trackId);
    //    baseApi->prepareRequest(request);

    HttpSession session;
    session.setHeaderParam("Authorization",
                           "GoogleLogin auth=" +
                               baseApi->getCredentials().authToken);
    session.setHeaderParam("X-Device-ID", baseApi->getCredentials().deviceId);

    auto response        = session.makeRequest(request);
    auto locationUrlIter = response.headerDict.find("Location");
    if (locationUrlIter != response.headerDict.end()) {
        return locationUrlIter->second;
    }

    return std::string();
}

static HttpResponse performAuthRequest(const std::vector<KVPair> &body);
static bool parseResponseText(const std::string &responseText,
                              std::map<std::string, std::string> &result);
static std::string getMasterToken(const std::string &login,
                                  const std::string &passwd,
                                  const std::string &deviceId);
static std::string getAuthToken(const std::string &login,
                                const std::string &masterToken,
                                const std::string &deviceId);

AuthCredentials LoginApi::login(const std::string &email,
                                const std::string &passwd,
                                const std::string &androidId)
{
    //    return std::async(policy, [=]{
    std::string deviceId = androidId;
    std::string encryptedLogPasswd =
        CryptoUtils::encryptLoginAndPasswd(email, passwd);
    std::string masterToken =
        getMasterToken(email, encryptedLogPasswd, deviceId);
    std::string authToken = getAuthToken(email, masterToken, deviceId);
    return AuthCredentials{authToken, email, androidId};
    //    });
}

std::future<AuthCredentials> LoginApi::loginAsync(const std::string &email,
                                                  const std::string &passwd,
                                                  const std::string &androidId)
{
    return std::async(
        std::launch::async, &LoginApi::login, this, email, passwd, androidId);
}

std::string getMasterToken(const std::string &login,
                           const std::string &passwd,
                           const std::string &deviceId)
{
    std::vector<KVPair> body;
    body.emplace_back("accountType", "HOSTED_OR_GOOGLE");
    body.emplace_back("has_permission", "1");
    body.emplace_back("add_account", "1");
    body.emplace_back("service", "ac2dm");
    body.emplace_back("source", "android");
    body.emplace_back("device_country", "us");
    body.emplace_back("operatorCountry", "us");
    body.emplace_back("lang", "en");
    body.emplace_back("sdk_version", "17");
    body.emplace_back("Email", login);
    body.emplace_back("EncryptedPasswd", passwd);
    body.emplace_back("androidId", deviceId);

    HttpResponse response = performAuthRequest(body);

    if (response.error.code != HttpErrorCode::OK) {
        throw std::runtime_error(response.error.message);
    }

    std::map<std::string, std::string> kvMap;
    if (!parseResponseText(response.text, kvMap)) {
        throw std::runtime_error("Failed to parse master login response");
    }

    auto token = kvMap.find("Token");
    if (token == kvMap.end()) {
        throw std::runtime_error("Failed to read auth token");
    }

    return token->second;
}

std::string getAuthToken(const std::string &login,
                         const std::string &masterToken,
                         const std::string &deviceId)
{
    std::vector<KVPair> body;
    body.emplace_back("accountType", "HOSTED_OR_GOOGLE");
    body.emplace_back("has_permission", "1");
    body.emplace_back("service", "sj");
    body.emplace_back("source", "android");
    body.emplace_back("app", "com.google.android.music");
    body.emplace_back("client_sig", "38918a453d07199354f8b19af05ec6562ced5788");
    body.emplace_back("device_country", "us");
    body.emplace_back("operatorCountry", "us");
    body.emplace_back("lang", "en");
    body.emplace_back("sdk_version", "17");
    body.emplace_back("Email", login);
    body.emplace_back("EncryptedPasswd", masterToken);
    body.emplace_back("androidId", deviceId);

    HttpResponse response = performAuthRequest(body);

    if (response.error.code != HttpErrorCode::OK) {
        throw std::runtime_error(response.error.message);
    }

    std::map<std::string, std::string> kvMap;
    if (!parseResponseText(response.text, kvMap)) {
        throw std::runtime_error("Failed to parse oauth login response");
    }

    auto token = kvMap.find("Auth");
    if (token == kvMap.end()) {
        throw std::runtime_error("Failed to read auth token");
    }

    return token->second;
}

static HttpResponse performAuthRequest(const std::vector<KVPair> &body)
{
    HttpRequest request{HttpMethod::POST,
                        "https://android.clients.google.com/auth"};
    request.setBody(body);
    return HttpSession::performRequest(request);
}

bool parseResponseText(const std::string &responseText,
                       std::map<std::string, std::string> &result)
{
    auto lines = StringUtils::split(responseText, '\n');
    for (auto &line : lines) {
        auto kvPair = StringUtils::split(line, '=');
        if (kvPair.size() < 2) {
            continue;
        }
        result.insert({kvPair[0], kvPair[1]});
    }
    return true;
}
}
