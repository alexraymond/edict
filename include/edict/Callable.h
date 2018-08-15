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

// Abstract implementation for the Callabe type-erasure type =================================
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

// Specialisation to handle free functions ===================================================
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

// Helper to ensure some type is either a pointer or a reference =============================
template <typename T, bool IsPointer>
struct MaybePointer
{
    using Type = T & ;
};
template <typename T>
struct MaybePointer<T, true>
{
    using Type = T * ;
};

// "Extreme" variant of decay to remove *all* qualifiers from a type =========================
template <typename T>
struct Decompose
{
    using Type = typename std::remove_cv<typename std::remove_pointer<T>::type>::type;
};

// "Extreme" variant of is_const to ignore pointer/reference types ===========================
template <typename T>
struct IsConstType
{
    static constexpr bool Value =
        std::is_const<typename std::remove_pointer<typename std::remove_reference<T>::type>::type>::value;
};

// Traits for invoking bound functions =======================================================
template <typename T, typename F, typename... Args> struct BoundFunctionInvokeTraits {};

template <typename T, typename F, typename... Args>
struct BoundFunctionInvokeTraits<T &, F, Args...>
{
    static void invoke(T &object_, F func_, Args &&...args_)
    {
        (object_.*func_)(std::forward<Args>(args_)...);
    }
};

template <typename T, typename F, typename... Args>
struct BoundFunctionInvokeTraits<T *, F, Args...>
{
    static void invoke(T *object_, F func_, Args &&...args_)
    {
        (object_->*func_)(std::forward<Args>(args_)...);
    }
};

// Traits for bound functions ================================================================
template <bool IsSame, bool IsObjectConst, bool IsMethodConst, bool IsPointer,
          typename T, typename... Args>
struct BoundFunctionTraitsBase
{
    static_assert(!IsObjectConst || IsMethodConst,
                  "edict error: You can't bind non-const functions to const objects.");

    static_assert(IsSame, "edict error: Function pointer must reference a method of passed object.");
};

template <bool IsObjectConst, bool IsPointer, typename T, typename... Args>
struct BoundFunctionTraitsBase<true, IsObjectConst, true, IsPointer, T, Args...>
{
    using ObjectType = typename MaybePointer<const T, IsPointer>::Type;
    using FuncType   = void (T::*)(Args...) const;
};

template <bool IsPointer, typename T, typename... Args>
struct BoundFunctionTraitsBase<true, false, false, IsPointer, T, Args...>
{
    using ObjectType = typename MaybePointer<T, IsPointer>::Type;
    using FuncType   = void(T::*)(Args...);
};

template <typename T, typename F> struct BoundFunctionTraits {};

template <typename T1, typename T2, typename ...Args>
struct BoundFunctionTraits<T1, void(T2::*)(Args...)>
{
    using _BaseTraits = BoundFunctionTraitsBase<
        std::is_same<typename Decompose<T1>::Type, typename Decompose<T2>::Type>::value,
        IsConstType<T1>::Value, false, std::is_pointer<T1>::value,
        typename Decompose<T1>::Type, Args...>;

    using ObjectType = typename _BaseTraits::ObjectType;
    using FuncType   = typename _BaseTraits::FuncType;

    static void invoke(ObjectType object_, FuncType func_, Args &&... args_)
    {
        BoundFunctionInvokeTraits<ObjectType, FuncType, Args...>::invoke(
            object_, func_, std::forward<Args>(args_)...);
    }
};

template <typename T1, typename T2, typename ...Args>
struct BoundFunctionTraits<T1, void(T2::*)(Args...) const>
{
    using _BaseTraits = BoundFunctionTraitsBase<
        std::is_same<typename Decompose<T1>::Type, typename Decompose<T2>::Type>::value,
        IsConstType<T1>::Value, true, std::is_pointer<T1>::value,
        typename Decompose<T1>::Type, Args...>;

    using ObjectType = typename _BaseTraits::ObjectType;
    using FuncType = typename _BaseTraits::FuncType;

    static void invoke(ObjectType object_, FuncType func_, Args &&... args_)
    {
        BoundFunctionInvokeTraits<ObjectType, FuncType, Args...>::invoke(
            object_, func_, std::forward<Args>(args_)...);
    }
};

// Specialisation to handle bound functions ==================================================
template <typename Traits>
class BoundFunctionPointer final : public CallableImpl
{
public:
    using BoundType = typename Traits::ObjectType;
    using Receiver  = typename Traits::FuncType;

    explicit BoundFunctionPointer(BoundType object_, Receiver receiver_) :
        m_object(object_),
        m_receiver(receiver_)
    {
    }

    bool isEqual(CallableImpl *other_) const override
    {
        if (auto *o = dynamic_cast<BoundFunctionPointer<Traits> *>(other_))
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

    template <typename Traits>
    static Callable make(typename Traits::ObjectType object_,
                         typename Traits::FuncType receiver_)
    {
        Callable c;

        c.m_d = std::make_shared<detail::BoundFunctionPointer<Traits>>(object_, receiver_);

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
