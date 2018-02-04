#include "http/httperror.hpp"
#include <curl/curl.h>

namespace gmusic
{

HttpErrorCode HttpError::getErrorCodeFromHttpStatus(int code)
{
    switch (code) {
    case 400:
        return HttpErrorCode::BAD_REQUEST;
    case 401:
        return HttpErrorCode::UNAUTHORIZED;
    case 404:
        return HttpErrorCode::NOT_FOUND;
    }
    return HttpErrorCode::UNKNOWN_ERROR;
}

HttpErrorCode HttpError::getErrorCodeForCurlError(unsigned curl_code)
{
    switch (curl_code) {
    case CURLE_OK:
        return HttpErrorCode::OK;
    case CURLE_UNSUPPORTED_PROTOCOL:
        return HttpErrorCode::UNSUPPORTED_PROTOCOL;
    case CURLE_URL_MALFORMAT:
        return HttpErrorCode::INVALID_URL_FORMAT;
    case CURLE_COULDNT_RESOLVE_PROXY:
        return HttpErrorCode::PROXY_RESOLUTION_FAILURE;
    case CURLE_COULDNT_RESOLVE_HOST:
        return HttpErrorCode::HOST_RESOLUTION_FAILURE;
    case CURLE_COULDNT_CONNECT:
        return HttpErrorCode::CONNECTION_FAILURE;
    case CURLE_OPERATION_TIMEDOUT:
        return HttpErrorCode::OPERATION_TIMEDOUT;
    case CURLE_SSL_CONNECT_ERROR:
        return HttpErrorCode::SSL_CONNECT_ERROR;
    case CURLE_PEER_FAILED_VERIFICATION:
        return HttpErrorCode::SSL_REMOTE_CERTIFICATE_ERROR;
    case CURLE_GOT_NOTHING:
        return HttpErrorCode::EMPTY_RESPONSE;
    case CURLE_SSL_ENGINE_NOTFOUND:
        return HttpErrorCode::GENERIC_SSL_ERROR;
    case CURLE_SSL_ENGINE_SETFAILED:
        return HttpErrorCode::GENERIC_SSL_ERROR;
    case CURLE_SEND_ERROR:
        return HttpErrorCode::NETWORK_SEND_FAILURE;
    case CURLE_RECV_ERROR:
        return HttpErrorCode::NETWORK_RECEIVE_ERROR;
    case CURLE_SSL_CERTPROBLEM:
        return HttpErrorCode::SSL_LOCAL_CERTIFICATE_ERROR;
    case CURLE_SSL_CIPHER:
        return HttpErrorCode::GENERIC_SSL_ERROR;
    case CURLE_SSL_CACERT:
        return HttpErrorCode::SSL_CACERT_ERROR;
    case CURLE_USE_SSL_FAILED:
        return HttpErrorCode::GENERIC_SSL_ERROR;
    case CURLE_SSL_ENGINE_INITFAILED:
        return HttpErrorCode::GENERIC_SSL_ERROR;
    case CURLE_SSL_CACERT_BADFILE:
        return HttpErrorCode::SSL_CACERT_ERROR;
    case CURLE_SSL_SHUTDOWN_FAILED:
        return HttpErrorCode::GENERIC_SSL_ERROR;
    case CURLE_SSL_CRL_BADFILE:
        return HttpErrorCode::SSL_CACERT_ERROR;
    case CURLE_SSL_ISSUER_ERROR:
        return HttpErrorCode::SSL_CACERT_ERROR;
    case CURLE_TOO_MANY_REDIRECTS:
        return HttpErrorCode::OK;
    default:
        return HttpErrorCode::INTERNAL_ERROR;
    }
}
}
