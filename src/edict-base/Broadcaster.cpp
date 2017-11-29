#include "Broadcaster.h"

#include <map>
#include <utility>
using namespace std;

class Broadcaster::PrivateData
{
public:
    PrivateData() :
        subscriptions()
    {
    }

    multimap<const string &, Handler> subscriptions;
};

bool Broadcaster::publish(const string &topic_, const QByteArray &data_)
{
    auto range = m_d->subscriptions.equal_range(topic_);
    for(auto it = range.first; it != range.second; ++it)
    {
        it->second(data_); // Calls each subscribed function
    }
    return true;
}

bool Broadcaster::subscribe(const string &topic_, Handler &receiver_)
{
    auto subscription = make_pair(topic_, receiver_);
    auto range = m_d->subscriptions.equal_range(topic_);
    for(auto it = range.first; it != range.second; ++it)
    {
        const auto * ptr = it->second.target<void(QByteArray)> ();
        if (*ptr == receiver_.target<void(QByteArray)>())
        {
            qWarning ("Attempting to double subscribe!");
            return false;
        }
    }
    m_d->subscriptions.insert(subscription);
    return true;
}

bool Broadcaster::unsubscribe(const string &topic_, Handler &receiver_)
{
    unsigned removals = 0;
    auto range = m_d->subscriptions.equal_range (topic_);
    for (auto it = range.first; it != range.second;)
    {
        const auto * ptr = it->second.target<void(QByteArray)> ();
        if (*ptr == receiver_.target<void(QByteArray)>())
        {
            // Associative-container erase idiom
            m_d->subscriptions.erase (it++);
            ++removals;
        }
        else
        {
            ++it;
        }
    }
    return removals > 0;
}
