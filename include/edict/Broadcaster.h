/*
 * Edict is a blackboard messaging system -- have fun!
 * Copyright (c) 2018 Alex Raymond, Kier Dugan.
 */

#pragma once

#include "edict/Callable.h"

#include <iostream>
#include <map>
#include <string>


namespace edict
{

class Broadcaster final
{
public:
	Broadcaster() :
		m_subscriptions()
	{
    }

	bool publish(const std::string &topic_, const std::string &data_)
	{
		auto range = m_subscriptions.equal_range(topic_);
		for (auto it = range.first; it != range.second; ++it)
		{
			it->second(data_); // Calls each subscribed function
		}
		return true;
	}

	bool subscribe(const std::string &topic_, Callable::FreeReceiver receiver_)
	{
        return _subscribe(topic_, Callable(receiver_));
	}
    template <typename T>
    bool subscribe(const std::string &topic_, T *object_,
                   Callable::BoundReceiver<T *> receiver_)
    {
        return _subscribe(topic_, Callable(object_, receiver_));
    }
    template <typename T>
    bool subscribe(const std::string &topic_, T &object_, Callable::BoundReceiver<T> receiver_)
    {
        return _subscribe(topic_, Callable(object_, receiver_));
    }

    bool unsubscribe(const std::string &topic_, Callable::FreeReceiver receiver_)
    {
        return _unsubscribe(topic_, Callable(receiver_));
    }
    template <typename T>
    bool unsubscribe(const std::string &topic_, T *object_,
                     Callable::BoundReceiver<T *> receiver_)
    {
        return _unsubscribe(topic_, Callable(object_, receiver_));
    }
    template <typename T>
    bool unsubscribe(const std::string &topic_, T &object_,
                     Callable::BoundReceiver<T> receiver_)
    {
        return _unsubscribe(topic_, Callable(object_, receiver_));
    }

private:
    bool _subscribe(const std::string &topic_, Callable receiver_)
    {
        auto subscription = make_pair(topic_, receiver_);
        auto range = m_subscriptions.equal_range(topic_);
        for (auto it = range.first; it != range.second; ++it)
        {
            if (it->second == receiver_)
            {
                std::cerr << "Attempting to double subscribe!" << std::endl;
                return false;
            }
        }
        m_subscriptions.insert(subscription);
        return true;
    }

    bool _unsubscribe(const std::string &topic_, Callable receiver_)
    {
        unsigned removals = 0;
        auto range = m_subscriptions.equal_range(topic_);
        for (auto it = range.first; it != range.second;)
        {
            if (it->second == receiver_)
            {
                // Associative-container erase idiom
                m_subscriptions.erase(it++);
                ++removals;
            }
            else
            {
                ++it;
            }
        }
        return removals > 0;
    }

	std::multimap<std::string, Callable> m_subscriptions;
};

}
