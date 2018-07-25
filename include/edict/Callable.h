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

template <typename T, bool isConst = false>
struct BoundFunctionTraits
{
    using Receiver = void(T::*)(const std::string &);
};
template <typename T>
struct BoundFunctionTraits<T, true>
{
    using Receiver = void(T::*)(const std::string &) const;
};

template <typename T, bool isPointer = false>
struct BoundObjectTraits
{
    using BaseType = typename std::remove_reference<T>::type;
    using BoundType = typename BaseType &;
    using Receiver = typename BoundFunctionTraits<typename std::remove_const<BaseType>::type,
                                                  std::is_const<T>::value>::Receiver;

    static void invoke(BoundType obj_, Receiver receiver_, const std::string &data_)
    {
        (obj_.*receiver_)(data_);
    }
};
template <typename T>
struct BoundObjectTraits<T, true>
{
    using BaseType = typename std::remove_pointer<T>::type;
    using BoundType = typename BaseType * const;
    using Receiver = typename BoundFunctionTraits<typename std::remove_const<BaseType>::type,
                                                  std::is_const<T>::value>::Receiver;

    static void invoke(BoundType obj_, Receiver receiver_, const std::string &data_)
    {
        (obj_->*receiver_)(data_);
    }
};

template <typename T>
class BoundFunctionPointerTraits
{
    using _Traits = BoundObjectTraits<T, std::is_pointer<T>::value>;

public:
    using BoundType = typename _Traits::BoundType;
    using Receiver  = typename _Traits::Receiver;

    static void invoke(BoundType obj_, Receiver receiver_, const std::string &data_)
    {
        _Traits::invoke(obj_, receiver_, data_);
    }
};


template <typename T, typename Traits = BoundFunctionPointerTraits<T>>
class BoundFunctionPointer final : public CallableImpl
{
public:
    using BoundType = typename Traits::BoundType;
    using Receiver  = typename Traits::Receiver;

    explicit BoundFunctionPointer(BoundType object_, Receiver receiver_) :
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
        Traits::invoke(m_object, m_receiver, data_);
    }

private:
    BoundType m_object;
    const Receiver m_receiver;
};

}


class Callable final
{
public:
    using FreeReceiver = detail::FreeFunctionPointer::Receiver;
    template <typename T>
    using BoundType = typename detail::BoundFunctionPointer<T>::BoundType;
    template <typename T>
    using BoundReceiver = typename detail::BoundFunctionPointer<T>::Receiver;

    template <typename T /*, typename = std::enable_if<std::is_pointer<T>::value>::type*/>
    static Callable make(T *object_, BoundReceiver<T *> receiver_)
    {
        Callable c;

        c.m_d = std::make_shared<detail::BoundFunctionPointer<T *>>(object_, receiver_);

        return c;
    }
    template <typename T/*, typename = std::enable_if<!std::is_pointer<T>::value>::type*/>
    static Callable make(T &object_, BoundReceiver<T> receiver_)
    {
        Callable c;

        c.m_d = std::make_shared<detail::BoundFunctionPointer<T>>(object_, receiver_);

        return c;
    }


    Callable() :
        m_d()
    {
    }
    Callable(FreeReceiver receiver_) :
        m_d { std::make_shared<detail::FreeFunctionPointer>(receiver_) }
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
