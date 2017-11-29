#pragma once

#include <QByteArray>
#include <QObject>

#include <string>
#include <functional>

using Handler = std::function<void(const QByteArray &)>;

class Broadcaster : public QObject
{
    Q_OBJECT
public:
    Broadcaster ();
    bool publish (const std::string &topic_, const QByteArray &data_);

    bool subscribe (const std::string &topic_, Handler &receiver_);

    bool unsubscribe (const std::string &topic_, Handler &receiver_);

private:
    class PrivateData;
    PrivateData * const m_d;
};



