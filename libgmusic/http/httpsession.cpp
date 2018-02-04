#include "http/httpsession.hpp"
#include "utilities.hpp"

#include <boost/algorithm/string.hpp>
#include <curl/curl.h>
#include <future>
#include <iostream>
#include <queue>

namespace gmusic
{

void HttpRequest::addParameter(const std::string &key, const std::string &value)
{
    if (!paramString.empty()) {
        paramString.append("&");
    }
    paramString.append(NetUtils::urlEncode(key));
    paramString.append("=");
    paramString.append(NetUtils::urlEncode(value));
}

void HttpRequest::setBody(const std::vector<KVPair> &pairs)
{
    std::string newBody;
    for (auto &pair : pairs) {
        if (!newBody.empty()) {
            newBody.append("&");
        }
        newBody.append(NetUtils::urlEncode(pair.first));
        newBody.append("=");
        newBody.append(NetUtils::urlEncode(pair.second));
    }
    body = std::move(newBody);
}

static size_t processWithCallback(char *data,
                                  size_t size,
                                  size_t nmemb,
                                  HttpSession *writerData)
{
    if (writerData == nullptr) {
        return 0;
    }
    size_t len = size * nmemb;
    return writerData->getDataCallback()(data, len);
}

static int
writer(char *data, size_t size, size_t nmemb, std::string *writerData)
{
    if (writerData == nullptr) {
        return 0;
    }
    size_t len = size * nmemb;
    writerData->append(data, len);
    return static_cast<int>(len);
}

static int progress_callback(HttpSession *client_p,
                             curl_off_t dltotal,
                             curl_off_t dlnow,
                             curl_off_t,
                             curl_off_t)
{
    if (client_p == nullptr) {
        return 0;
    }
    return client_p->getProgressCallback()(dltotal, dlnow, client_p);
}

static size_t
header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    using Dict       = std::map<std::string, std::string>;
    Dict *headerData = static_cast<Dict *>(userdata);
    size_t len       = size * nitems;

    std::string headerLine(buffer, len);
    std::vector<std::string> headerLineParts;
    boost::split(headerLineParts,
                 headerLine,
                 boost::is_any_of("\n "),
                 boost::token_compress_on);
    if (headerLineParts.size() >= 2 &&
        boost::find_first(headerLineParts[0], "Location")) {
        auto &locValue = headerLineParts[1];
        boost::trim(locValue);
        (*headerData)["Location"] = locValue;
    }
    return len;
}

HttpSession::HttpSession() { handle = curl_easy_init(); }
HttpSession::~HttpSession()
{
    if (currentHeaderNode != nullptr) {
        curl_slist_free_all(currentHeaderNode);
    }
    curl_easy_cleanup(handle);
}

HttpSession::HttpSession(HttpSession &&other)
    : handle(other.handle), currentHeaderNode(other.currentHeaderNode),
      dataCallback(std::move(other.dataCallback))
{
    other.handle            = nullptr;
    other.currentHeaderNode = nullptr;
}

HttpSession &HttpSession::operator=(HttpSession &&other)
{
    handle                  = other.handle;
    currentHeaderNode       = other.currentHeaderNode;
    dataCallback            = std::move(other.dataCallback);
    other.handle            = nullptr;
    other.currentHeaderNode = nullptr;
    return *this;
}

void HttpSession::resume() { curl_easy_pause(handle, CURLPAUSE_CONT); }

void HttpSession::setHeaderParam(const std::string &key,
                                 const std::string &value)
{
    std::string headerString = key + ": " + value;
    currentHeaderNode =
        curl_slist_append(currentHeaderNode, headerString.c_str());
    std::lock_guard<std::mutex> lock{mutex};
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, currentHeaderNode);
}

void HttpSession::setByteRange(long minValue)
{
    std::string minValueStr = std::to_string(minValue);
    curl_easy_setopt(handle, CURLOPT_RANGE, (minValueStr + "-").c_str());
}

HttpResponse HttpSession::makeRequest(const HttpRequest &request)
{
    std::lock_guard<std::mutex> lock{mutex};

    std::string requestParams = request.getParamString();
    std::string requestUrl    = request.getUrl();
    std::string requestBody   = request.getBody();

    switch (request.getMethod()) {
    case HttpMethod::GET:
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "GET");
        break;
    case HttpMethod::POST:
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "POST");
        if (!request.getBody().empty()) {
            curl_easy_setopt(handle, CURLOPT_POSTFIELDS, requestBody.c_str());
            curl_easy_setopt(
                handle, CURLOPT_POSTFIELDSIZE, requestBody.length());
        } else {
            curl_easy_setopt(handle, CURLOPT_COPYPOSTFIELDS, "");
            curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, 0);
        }
        break;
    }

    if (!requestParams.empty()) {
        requestUrl.append("?");
        requestUrl.append(requestParams);
    }

    std::string responseText;
    std::map<std::string, std::string> headerData;
    curl_easy_setopt(handle, CURLOPT_URL, requestUrl.c_str());
    if (dataCallback) {
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, processWithCallback);
    } else {
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &responseText);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writer);
    }
    if (progressCallback) {
        curl_easy_setopt(handle, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0);
    }
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &headerData);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);

    CURLcode result = curl_easy_perform(handle);

    long statusCode;
    char *url;
    curl_easy_getinfo(handle, CURLINFO_EFFECTIVE_URL, &url);
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &statusCode);

    const char *errorDescription = curl_easy_strerror(result);

    HttpError error = HttpError::createFromCurlCode(static_cast<int>(result),
                                                    errorDescription);
    return HttpResponse(statusCode, url, headerData, responseText, error);
}
}
