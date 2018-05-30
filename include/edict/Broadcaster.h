/*
 * Edict is a blackboard messaging system -- have fun!
 * Copyright (c) 2018 Alex Raymond, Kier Dugan.
 */

#pragma once

#include "edict/Callable.h"

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <utility>


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
        for (const auto &pair : m_callables)
        {
            auto predicate = pair.first;
            auto callable = pair.second;
            if (predicate(topic_))
                callable(data_);
        }
        for (const auto &pair : m_subscriptions)
        {
            const std::string &pattern = pair.first;
            auto callable = pair.second;
            if (topic_ == pattern)
                callable(data_);
        }
        return true;
    }

    bool subscribe(std::function<bool(const std::string &)> predicate_, Callable receiver_)
    {
        m_callables.push_back(std::make_pair(predicate_, receiver_));
        return true;
    }

    bool subscribe(const std::regex &regex_, Callable receiver_)
    {
        auto predicate = [=](const std::string &topic_) -> bool
        {
            return std::regex_match(topic_, regex_);
        };
        return subscribe(predicate, receiver_);
    }

    bool subscribe(const std::string &topic_, Callable receiver_)
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
    template <typename T>
    bool subscribe(const std::string &topic_, T &object_, Callable::BoundReceiver<T> receiver_)
    {
        return subscribe(topic_, Callable { object_, receiver_ });
    }
    template <typename T>
    bool subscribe(const std::string &topic_, T *object_, Callable::BoundReceiver<T *> receiver_)
    {
        return subscribe(topic_, Callable { object_, receiver_ });
    }

    bool unsubscribe(const std::string &topic_, Callable receiver_)
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

private:
    std::multimap<std::string, Callable> m_subscriptions;
    std::vector<std::pair<std::function<bool(const std::string &)>, Callable> > m_callables;
};

}
