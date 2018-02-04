#ifndef HTTPERROR_HPP_
#define HTTPERROR_HPP_

#include <string>

namespace gmusic
{

enum class HttpErrorCode {
    OK = 0,
    CONNECTION_FAILURE,
    EMPTY_RESPONSE,
    HOST_RESOLUTION_FAILURE,
    INTERNAL_ERROR,
    INVALID_URL_FORMAT,
    NETWORK_RECEIVE_ERROR,
    NETWORK_SEND_FAILURE,
    OPERATION_TIMEDOUT,
    PROXY_RESOLUTION_FAILURE,
    SSL_CONNECT_ERROR,
    SSL_LOCAL_CERTIFICATE_ERROR,
    SSL_REMOTE_CERTIFICATE_ERROR,
    SSL_CACERT_ERROR,
    GENERIC_SSL_ERROR,
    UNSUPPORTED_PROTOCOL,
    UNAUTHORIZED,
    BAD_REQUEST,
    NOT_FOUND,
    UNKNOWN_ERROR = 1000
};

class HttpError
{
  public:
    HttpError() : code{HttpErrorCode::OK} {}

    HttpError(HttpErrorCode code, const std::string &errorMessage)
        : code{code}, message{errorMessage}
    {
    }

    static HttpError createFromCurlCode(int curlCode, const std::string &msg)
    {
        auto errorCode = getErrorCodeForCurlError(curlCode);
        return HttpError{errorCode, msg};
    }

    static HttpError createFromStatusCode(int statusCode,
                                          const std::string &msg)
    {
        auto errorCode = getErrorCodeFromHttpStatus(statusCode);
        return HttpError{errorCode, msg};
    }

    HttpErrorCode code;
    std::string message;

  private:
    static HttpErrorCode getErrorCodeForCurlError(unsigned curl_code);
    static HttpErrorCode getErrorCodeFromHttpStatus(int code);
};
}

#endif
