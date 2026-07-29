#pragma once

#include <stdexcept>
#include <QString>

namespace bili {

class BiliException : public std::runtime_error {
public:
    explicit BiliException(const QString& message);
    explicit BiliException(const std::string& message);
    QString message() const;

private:
    QString m_message;
};

class NetworkException : public BiliException {
public:
    explicit NetworkException(const QString& message, int statusCode = 0);
    int statusCode() const;

private:
    int m_statusCode = 0;
};

class CookieException : public NetworkException {
public:
    explicit CookieException(const QString& message);
};

class ApiException : public NetworkException {
public:
    explicit ApiException(const QString& message, int apiCode = 0);
    int apiCode() const;

private:
    int m_apiCode = 0;
};

class RetryExhaustedException : public NetworkException {
public:
    explicit RetryExhaustedException(const QString& message);
};

class ParseException : public BiliException {
public:
    explicit ParseException(const QString& message);
};

class StorageException : public BiliException {
public:
    explicit StorageException(const QString& message);
};

class ConfigException : public BiliException {
public:
    explicit ConfigException(const QString& message);
};

class LicenseException : public BiliException {
public:
    explicit LicenseException(const QString& message);
};

} // namespace bili
