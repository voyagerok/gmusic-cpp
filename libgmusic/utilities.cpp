#include "utilities.hpp"

#include "config.h"
#include <algorithm>
#include <boost/filesystem.hpp>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <sstream>

namespace fs     = boost::filesystem;
namespace chrono = std::chrono;

namespace gmusic
{

namespace CryptoUtils
{

static const auto bio_deleter = [](BIO *bioHandle) { BIO_free(bioHandle); };

#define UNIQUE_PTR(type, ptr, deleter)                                         \
    std::unique_ptr<type, decltype(deleter)> { ptr, deleter }

#define UNIQUE_PTR_BIO(bio) UNIQUE_PTR(BIO, bio, bio_deleter)

std::string base64_encode(Byte *source, int slen, bool urlsafe)
{
    int resultSize = (slen % 3 == 0 ? slen / 3 : slen / 3 + 1) * 4;
    std::unique_ptr<char[]> resultBuf{new char[resultSize]};

    auto b64 = UNIQUE_PTR_BIO(BIO_new(BIO_f_base64()));
    BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);
    auto plain = UNIQUE_PTR_BIO(BIO_new(BIO_s_mem()));
    BIO_push(b64.get(), plain.get());

    int written = BIO_write(b64.get(), source, slen);
    if (written <= 0) {
        std::cerr << "performBase64Operation(): failed to prepare data"
                  << std::endl;
        return std::string();
    }
    BIO_flush(b64.get());
    int read = BIO_read(plain.get(), resultBuf.get(), resultSize);
    if (read <= 0) {
        std::cerr << "performBase64Operation(): failed to read data"
                  << std::endl;
        return std::string();
    }

    std::string result{&resultBuf[0], &resultBuf[read]};

    if (urlsafe) {
        std::for_each(result.begin(), result.end(), [](char &c) {
            switch (c) {
            case '+':
                c = '-';
                break;
            case '/':
                c = '_';
                break;
            default:
                break;
            }
        });
    }

    return result;
}

std::vector<CryptoUtils::Byte> base64_decode(const char *source, int slen)
{
    int resultSize = slen / 4 * 3;
    if (source[slen - 1] == '=')
        --resultSize;
    if (source[slen - 2] == '=')
        --resultSize;

    std::unique_ptr<Byte[]> resultBuf{
        new Byte[static_cast<unsigned long>(resultSize)]};

    auto b64 = UNIQUE_PTR_BIO(BIO_new(BIO_f_base64()));
    BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);
    auto plain = UNIQUE_PTR_BIO(BIO_new(BIO_s_mem()));
    BIO_push(b64.get(), plain.get());

    int written = BIO_write(plain.get(), source, slen);
    if (written <= 0) {
        std::cerr << "performBase64Operation(): failed to prepare data"
                  << std::endl;
        return std::vector<unsigned char>();
    }
    BIO_flush(plain.get());
    int read = BIO_read(b64.get(), resultBuf.get(), resultSize);
    if (read <= 0) {
        std::cerr << "performBase64Operation(): failed to read data"
                  << std::endl;
        return std::vector<Byte>();
    }

    return std::vector<Byte>{&resultBuf[0],
                             &resultBuf[static_cast<size_t>(read)]};
}

static int decodeKeyComponent(const std::vector<unsigned char> &bytes,
                              int startIndex,
                              BIGNUM **component);
static std::vector<unsigned char>
rsaEncrypt(const unsigned char *data, int dataSize, BIGNUM *mod, BIGNUM *exp);

std::string encryptLoginAndPasswd(const std::string &login,
                                  const std::string &passwd)
{
    static std::string encodedKey =
        "AAAAgMom/1a/v0lblO2Ubrt60J2gcuXSljGFQXgcyZWveWLEwo6prwgi3"
        "iJIZdodyhKZQrNWp5nKJ3srRXcUW+F1BD3baEVGcmEgqaLZUNBjm057pK"
        "RI16kB0YppeGx5qIQ5QjKzsR8ETQbKLNWgRY0QRNVz34kMJR3P/LgHax/"
        "6rmf5AAAAAwEAAQ==";

    auto decoded = base64_decode(encodedKey.c_str(),
                                 static_cast<int>(encodedKey.length()));

    BIGNUM *modulus = nullptr, *exponent = nullptr;
    int modulusSize = decodeKeyComponent(decoded, 0, &modulus);
    decodeKeyComponent(decoded, modulusSize + 4, &exponent);

    unsigned char hash[SHA512_DIGEST_LENGTH];
    SHA1(&decoded[0], decoded.size(), hash);

    unsigned char signature[5];
    signature[0] = 0;
    signature[1] = hash[0];
    signature[2] = hash[1];
    signature[3] = hash[2];
    signature[4] = hash[3];

    std::vector<unsigned char> combined;
    combined.insert(combined.end(), login.begin(), login.end());
    combined.push_back(0x00);
    combined.insert(combined.end(), passwd.begin(), passwd.end());

    auto encryptedData = rsaEncrypt(
        combined.data(), static_cast<int>(combined.size()), modulus, exponent);

    std::vector<unsigned char> output;
    output.insert(output.end(), &signature[0], &signature[5]);
    output.insert(output.end(), encryptedData.begin(), encryptedData.end());

    std::string outputb64 =
        base64_encode(output.data(), static_cast<int>(output.size()), true);

    return outputb64;
}

static int decodeKeyComponent(const std::vector<unsigned char> &bytes,
                              int startIndex,
                              BIGNUM **component)
{

    uint32_t componentSize = 0;
    componentSize |= bytes[startIndex] << 24;
    componentSize |= bytes[startIndex + 1] << 16;
    componentSize |= bytes[startIndex + 2] << 8;
    componentSize |= bytes[startIndex + 3];
    *component = BN_bin2bn(&bytes[startIndex + 4], componentSize, nullptr);
    return componentSize;
}

#define UNIQUE_PTR_RSA(rsa)

static std::vector<unsigned char>
rsaEncrypt(const unsigned char *data, int dataSize, BIGNUM *mod, BIGNUM *exp)
{
    auto rsa_deleter = [](RSA *rsa) { RSA_free(rsa); };
    auto publicKey   = UNIQUE_PTR(RSA, RSA_new(), rsa_deleter);
#ifdef LCRYPTO_HAS_RSA_SET0_KEY
    RSA_set0_key(publicKey.get(), mod, exp, nullptr);
#else
    publicKey->n = mod;
    publicKey->e = exp;
#endif

    int expectedEncryptedSize = RSA_size(publicKey.get());
    std::unique_ptr<unsigned char[]> encryptedData{
        new unsigned char[expectedEncryptedSize]};
    int actualEncryptedSize = RSA_public_encrypt(dataSize,
                                                 data,
                                                 encryptedData.get(),
                                                 publicKey.get(),
                                                 RSA_PKCS1_OAEP_PADDING);

    return std::vector<unsigned char>{&encryptedData[0],
                                      &encryptedData[actualEncryptedSize]};
}

std::pair<std::string, std::string> encryptTrackId(const std::string &trackId)
{
    using Byte = Byte;

    static char s1[] = "VzeC4H4h+T2f0VI180nVX8x+Mb5HiTtGnKgH52Otj8ZCGDz9jRW"
                       "yHb6QXK0JskSiOgzQfwTY5xgLLSdUSreaLVMsVVWfxfa8Rw==";
    static char s2[] = "ZAPnhUkYwQ6y5DdQxWThbvhJHN8msQ1rqJw0ggKdufQjelrKuiG"
                       "GJI30aswkgCWTDyHkTGK9ynlqTkJ5L4CiGGUabGeo8M6JTQ==";
    static unsigned char result_hash[EVP_MAX_MD_SIZE];

    std::vector<Byte> s1_decoded = base64_decode(s1, sizeof(s1) - 1);
    std::vector<Byte> s2_decoded = base64_decode(s2, sizeof(s2) - 1);

    assert(s1_decoded.size() == s2_decoded.size());

    std::vector<Byte> key(s1_decoded.size());
    for (int i = 0; i < s1_decoded.size(); ++i) {
        key[i] = s1_decoded[i] ^ s2_decoded[i];
    }

    static std::string salt;
    if (salt.empty()) {
        auto ms = chrono::duration_cast<chrono::milliseconds>(
            chrono::system_clock().now().time_since_epoch());
        salt = std::to_string(ms.count());
    }
#ifdef LCRYPTO_HAS_HMAC_CTX_NEW
    auto deleter = [](HMAC_CTX *ctx) { HMAC_CTX_free(ctx); };
    auto ctx     = UNIQUE_PTR(HMAC_CTX, HMAC_CTX_new(), deleter);
#else
    auto deleter = [](HMAC_CTX *ctx) { HMAC_CTX_cleanup(ctx); };
    HMAC_CTX rawCtx;
    HMAC_CTX_init(&rawCtx);
    auto ctx = std::unique_ptr<HMAC_CTX, decltype(deleter)>(&rawCtx, deleter);
#endif
    HMAC_Init_ex(ctx.get(),
                 key.data(),
                 static_cast<int>(key.size()),
                 EVP_sha1(),
                 nullptr);
    HMAC_Update(ctx.get(),
                reinterpret_cast<const unsigned char *>(trackId.c_str()),
                trackId.length());
    HMAC_Update(ctx.get(),
                reinterpret_cast<const unsigned char *>(salt.c_str()),
                salt.length());

    unsigned hash_actual_size;
    HMAC_Final(ctx.get(), result_hash, &hash_actual_size);

    std::string hashed_trackId =
        base64_encode(result_hash, hash_actual_size, true);
    return std::make_pair(hashed_trackId, salt);
}
}

namespace NetUtils
{
std::string urlEncode(const std::string &str)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }
        escaped << '%' << std::setw(2) << static_cast<unsigned>(c);
    }

    return escaped.str();
}

#define HEX(x)                                                                 \
    std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(x)
}

namespace StringUtils
{
std::vector<std::string> split(const std::string &str, char delim)
{
    std::vector<std::string> result;
    std::stringstream ss{str};
    std::string item;
    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }

    return result;
}

unsigned long unsignedLongFromString(const std::string &str)
{
    unsigned long result = 0;
    for (char c : str) {
        if (!isdigit(c)) {
            break;
        }
        result *= 10;
        result += c - '0';
    }
    return result;
}
}

namespace SysUtils
{
std::string timeStringFromSeconds(int seconds)
{
    int minutes     = seconds / 60;
    int leftSeconds = seconds % 60;

    std::ostringstream ostr;
    ostr << std::setw(2) << std::setfill('0') << minutes;
    ostr << ":" << std::setw(2) << std::setfill('0') << leftSeconds;

    return ostr.str();
}
}

namespace FSUtils
{

void deleteFile(const std::string &path) { boost::filesystem::remove(path); }

bool createDirectoryPrivate(const std::string &path, std::string &error)
{
    boost::system::error_code errCode;
    bool status = boost::filesystem::create_directory(path, errCode);
    if (!status) {
        error = errCode.message();
    }
    return status;
}

bool tryCreateDirectory(const std::string &path)
{
    std::string errMsg;
    if (!createDirectoryPrivate(path, errMsg)) {
        ERRLOG << errMsg << std::endl;
        return false;
    }
    return true;
}

void createDirectoryIfNeeded(const std::string &path)
{
    std::string errMsg;
    if (!createDirectoryPrivate(path, errMsg)) {
        throw std::runtime_error(errMsg);
    }
}

bool isFileExists(const std::string &path) { return fs::exists(path); }
}

LogStream &Logger::getErrorLogger()
{
    static LogStream logger{std::cerr};
    return logger;
}

LogStream &Logger::getStdLogger()
{
    static LogStream logger{std::cout};
    return logger;
}
}
