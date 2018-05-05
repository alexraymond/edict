/*
 * Edict is a blackboard messaging system -- have fun!
 * Copyright (c) 2018 Alex Raymond, Kier Dugan.
 */

#pragma once

#include <string>


namespace edict
{

class Callable
{
public:
    virtual ~Callable() {}

    virtual void operator() (const std::string &) const = 0;
};

class FreeFunctionPointer final : public Callable
{
public:
    using Receiver = void(*)(const std::string &);

    explicit FreeFunctionPointer(Receiver receiver_) :
        m_receiver(receiver_)
    {
    }
    FreeFunctionPointer(const FreeFunctionPointer &) = default;

    void operator() (const std::string &data_) const override
    {
        (*m_receiver)(data_);
    }

private:
    Receiver const m_receiver;
};

template <typename T>
class BoundFunctionPointer final : public Callable
{
public:
    using Receiver = void(T::*)(const std::string &);

    explicit BoundFunctionPointer(T &object_, Receiver receiver_) :
        m_object(object_),
        m_receiver(receiver_)
    {
    }

    vpod operator() (const std::string &data_) const override
    {
        (m_object.*m_receiver)(data_);
    }

private:
    T &m_object;
    Receiver const m_receiver;
};

}
