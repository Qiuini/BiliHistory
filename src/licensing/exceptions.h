#pragma once

#include <stdexcept>
#include <QString>

namespace bili {

class LicensingException : public std::runtime_error {
public:
    explicit LicensingException(const QString& message)
        : std::runtime_error(message.toStdString())
        , m_message(message)
    {
    }

    QString message() const { return m_message; }

private:
    QString m_message;
};

} // namespace bili
