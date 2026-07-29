#include "exceptions.h"

namespace bili {

BiliException::BiliException(const QString& message)
    : std::runtime_error(message.toStdString())
    , m_message(message)
{
}

BiliException::BiliException(const std::string& message)
    : std::runtime_error(message)
    , m_message(QString::fromStdString(message))
{
}

QString BiliException::message() const {
    return m_message;
}

NetworkException::NetworkException(const QString& message, int statusCode)
    : BiliException(message)
    , m_statusCode(statusCode)
{
}

int NetworkException::statusCode() const {
    return m_statusCode;
}

CookieException::CookieException(const QString& message)
    : NetworkException(message, 401)
{
}

ApiException::ApiException(const QString& message, int apiCode)
    : NetworkException(message, 0)
    , m_apiCode(apiCode)
{
}

int ApiException::apiCode() const {
    return m_apiCode;
}

RetryExhaustedException::RetryExhaustedException(const QString& message)
    : NetworkException(message)
{
}

ParseException::ParseException(const QString& message)
    : BiliException(message)
{
}

StorageException::StorageException(const QString& message)
    : BiliException(message)
{
}

ConfigException::ConfigException(const QString& message)
    : BiliException(message)
{
}

LicenseException::LicenseException(const QString& message)
    : BiliException(message)
{
}

} // namespace bili
