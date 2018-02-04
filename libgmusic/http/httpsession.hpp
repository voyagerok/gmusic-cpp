#ifndef HTTPCLIENT_HPP_
#define HTTPCLIENT_HPP_

#include <curl/curl.h>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "http/httperror.hpp"

namespace gmusic
{

enum class HttpMethod { GET, POST };

using KVPair = std::pair<std::string, std::string>;

class HttpRequest
{
  public:
    HttpRequest(HttpMethod method, const std::string &url)
        : method{method}, url{url}
    {
    }

    void addParameter(const std::string &key, const std::string &value);

    void setBody(const std::string &body) { this->body = body; }
    void setBody(std::string &&body) { this->body = std::move(body); }
    void setBody(const std::vector<KVPair> &pairs);

    std::string getParamString() const { return paramString; }
    std::string getBody() const { return body; }
    HttpMethod getMethod() const { return method; }
    std::string getUrl() const { return url; }

  private:
    std::string paramString;
    std::string body;
    HttpMethod method;
    std::string url;
};

struct HttpResponse {
    using HeaderDict = std::map<std::string, std::string>;
    HttpResponse(long statusCode,
                 std::string url,
                 HeaderDict headerDict,
                 std::string text = std::string(),
                 HttpError error  = HttpError())
        : text{text}, status{statusCode},
          headerDict(headerDict), url{url}, error{error}
    {
    }

    std::string text;
    long status;
    HeaderDict headerDict;
    std::string url;
    HttpError error;
};

enum class HttpHeaderKey { USER_AGENT };

// class WorkerScheduler;
// class HttpSessionPrivate;

class HttpSession
{
  public:
    using RequestCompletionHandler = std::function<void(const HttpResponse &)>;

    HttpSession();
    ~HttpSession();

    HttpSession(const HttpSession &) = delete;
    HttpSession &operator=(const HttpSession &) = delete;

    HttpSession(HttpSession &&session);
    HttpSession &operator=(HttpSession &&);

    HttpResponse makeRequest(const HttpRequest &request);
    void resume();
    void setHeaderParam(const std::string &key, const std::string &value);
    void setByteRange(long minValue);

    void
    setDataCallback(const std::function<size_t(char *, size_t)> &dataCallback)
    {
        this->dataCallback = dataCallback;
    }

    void
    setProgressCallback(const std::function<int(size_t, size_t, HttpSession *)>
                            &progressCallback)
    {
        this->progressCallback = progressCallback;
    }

    const std::function<size_t(char *, size_t)> &getDataCallback() const
    {
        return dataCallback;
    }

    const std::function<int(size_t, size_t, HttpSession *)> &
    getProgressCallback() const
    {
        return progressCallback;
    }

    static HttpResponse performRequest(const HttpRequest &request)
    {
        return HttpSession().makeRequest(request);
    }

  private:
    CURL *handle;
    std::mutex mutex;
    struct curl_slist *currentHeaderNode = nullptr;
    std::function<size_t(char *, size_t)> dataCallback;
    std::function<int(size_t, size_t, HttpSession *)> progressCallback;
};
}

#endif // HTTPCLIENT_HPP_
