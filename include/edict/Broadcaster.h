/*
* Edict is a blackboard messaging system -- have fun!
* Copyright (c) 2018 Alex Raymond, Kier Dugan.
*/

#pragma once

#include <iostream>
#include <map>
#include <string>


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
		auto range = m_subscriptions.equal_range(topic_);
		for (auto it = range.first; it != range.second; ++it)
		{
			it->second(data_); // Calls each subscribed function
		}
		return true;
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
};

}
