/*
 * Edict is a blackboard messaging system -- have fun!
 * Copyright (c) 2018 Alex Raymond, Kier Dugan.
 */

#pragma once

#include <cassert>
#include <memory>
#include <string>


namespace edict
{
namespace detail
{

class CallableImpl
{
public:
    CallableImpl() {}
    CallableImpl(const CallableImpl &) = delete;
    CallableImpl(CallableImpl &&) = delete;
    bool operator= (const CallableImpl &) = delete;
    bool operator= (CallableImpl &&) = delete;
    virtual ~CallableImpl() {}

    virtual bool isEqual(CallableImpl *other_) const = 0;

    virtual void invoke(const std::string &data_) const = 0;
};

class FreeFunctionPointer final : public CallableImpl
{
public:
    using Receiver = void(*)(const std::string &);

    explicit FreeFunctionPointer(Receiver receiver_) :
        m_receiver(receiver_)
    {
    }

    bool isEqual(CallableImpl *other_) const override
    {
        if (auto *o = dynamic_cast<FreeFunctionPointer *>(other_))
            return m_receiver == o->m_receiver;

        return false;
    }
    void invoke(const std::string &data_) const override
    {
        (*m_receiver)(data_);
    }

private:
    const Receiver m_receiver;
};


template <typename T>
class BoundFunctionPointer final : public CallableImpl
{
public:
    using Receiver = void(T::*)(const std::string &);

    explicit BoundFunctionPointer(T &object_, Receiver receiver_) :
        m_object(object_),
        m_receiver(receiver_)
    {
    }

    bool isEqual(CallableImpl *other_) const override
    {
        if (auto *o = dynamic_cast<BoundFunctionPointer<T> *>(other_))
            return m_object == o->m_object && m_receiver == o->m_receiver;

        return false;
    }
    void invoke(const std::string &data_) const override
    {
        (m_object.*m_receiver)(data_);
    }

private:
    T &m_object;
    const Receiver m_receiver;
};

template <typename T>
class BoundFunctionPointer<T *> final : public CallableImpl
{
public:
    using Receiver = void(T::*)(const std::string &);

    explicit BoundFunctionPointer(T *object_, Receiver receiver_) :
        m_object(object_),
        m_receiver(receiver_)
    {
    }

    bool isEqual(CallableImpl *other_) const override
    {
        if (auto *o = dynamic_cast<BoundFunctionPointer<T *> *>(other_))
            return m_object == o->m_object && m_receiver == o->m_receiver;

        return false;
    }
    void invoke(const std::string &data_) const override
    {
        (m_object->*m_receiver)(data_);
    }

private:
    T * const m_object;
    const Receiver m_receiver;
};

}


class Callable final
{
public:
    using FreeReceiver = detail::FreeFunctionPointer::Receiver;
    template <typename T>
    using BoundReceiver = typename detail::BoundFunctionPointer<T>::Receiver;

    Callable() :
        m_d()
    {
    }
    explicit Callable(FreeReceiver receiver_) :
        m_d { std::make_shared<detail::FreeFunctionPointer>(receiver_) }
    {
    }
    template <typename T>
    explicit Callable(T &object_, BoundReceiver<T> receiver_) :
        m_d { std::make_shared<detail::BoundFunctionPointer<T>>(object_, receiver_) }
    {
    }
    template <typename T>
    explicit Callable(T *object_, BoundReceiver<T *> receiver_) :
        m_d { std::make_shared<detail::BoundFunctionPointer<T *>>(object_, receiver_) }
    {
    }
    Callable(const Callable &other_) :
        m_d { other_.m_d }
    {
    }
    Callable(Callable &&other_) :
        m_d { std::move(other_.m_d) }
    {
    }
    Callable &operator=(Callable other_)
    {
        m_d.swap(other_.m_d);
        return *this;
    }

    bool operator== (const Callable &other_) const
    {
        assert(m_d);

        return m_d->isEqual(other_.m_d.get());
    }
    bool operator!= (const Callable &other_) const
        { return !(*this == other_); }

    void operator() (const std::string &message_) const
    {
        assert(m_d);

        m_d->invoke(message_);
    }

private:
    std::shared_ptr<detail::CallableImpl> m_d;
};

}
