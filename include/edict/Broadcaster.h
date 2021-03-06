/*
* Edict is a blackboard messaging system -- have fun!
* Copyright (c) 2018 Alex Raymond, Kier Dugan.
*/

#pragma once

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
    typedef void(*ReceiverFunc)(const std::string &);

    Broadcaster() :
        m_subscriptions()
    {}

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

    bool subscribe(std::function<bool(const std::string &)> predicate_, ReceiverFunc receiver_)
    {
        m_callables.push_back(std::make_pair(predicate_, receiver_));
        return true;
    }

    bool subscribe(const std::regex &regex_, ReceiverFunc receiver_)
    {
        auto predicate = [=](const std::string &topic_) -> bool
        {
            return std::regex_match(topic_, regex_);
        };
        return subscribe(predicate, receiver_);
    }

    bool subscribe(const std::string &topic_, ReceiverFunc receiver_)
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

    bool unsubscribe(const std::string &topic_, ReceiverFunc receiver_)
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
    std::multimap<std::string, ReceiverFunc> m_subscriptions;
    std::vector<std::pair<std::function<bool(const std::string &)>, ReceiverFunc> > m_callables;
};

}
