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
namespace detail
{

template <bool IsObjectConst, bool IsFunctionConst>
struct ObjectTraits
{
    static constexpr bool OK = true;
};
template <>
struct ObjectTraits<true, false>
{
    static constexpr bool OK = false;
};

template <typename T, typename F> struct FunctionTraits {};


template <typename T> struct ObjectTraits2 { using Type = T & ; };
template <typename T> struct ObjectTraits2<T *> {using Type = T * ; };

template <typename T1, typename T2>
struct FunctionTraits<T1, void(T2::*)(const std::string &)>
{
    static constexpr bool IsPointer = std::is_pointer<T1>::value;



    static constexpr bool IsConst = false;


    static constexpr bool OK = ObjectTraits<std::is_const<T1>::value, false>::OK;
    using ObjectType = typename std::remove_cv<T2>::type; //std::decay<T>::type;
    //using TargetType = ObjectType;
    using TargetType = typename ObjectTraits2<T1>::Type;
    using FuncType = void(T2::*)(const std::string &);
};

template <typename T1, typename T2>
struct FunctionTraits<T1, void(T2::*)(const std::string &) const>
{
    static constexpr bool IsConst = true;

    static constexpr bool OK = ObjectTraits<std::is_const<T1>::value, true>::OK;

    using ObjectType = typename std::remove_cv<T2>::type;// std::decay<T>::type;
    using TargetType = typename std::add_const<typename ObjectTraits2<T1>::Type>::type;
//    using TargetType = const typename ObjectTraits2<T1>::Type;
    using FuncType = void(T2::*)(const std::string &) const;
};

}


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
    template <
        typename T,
        typename F,
        typename Traits = detail::BoundFunctionTraits<T, F>,
        typename = typename Traits::FuncType>
    bool subscribe(const std::string &topic_, T &object_, F receiver_)
    {
        return subscribe(topic_, Callable::make<Traits>(object_, receiver_));
    }
    template <
        typename T,
        typename F,
        typename Traits = detail::BoundFunctionTraits<T *, F>,
        typename = typename Traits::FuncType>
    bool subscribe(const std::string &topic_, T *object_, F receiver_)
    {
        return subscribe(topic_, Callable::make<Traits>(object_, receiver_));
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
