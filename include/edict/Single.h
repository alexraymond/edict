/*
 * Edict is a blackboard messaging system -- have fun!
 * Copyright (c) 2018 Alex Raymond, Kier Dugan.
 */

#pragma once

namespace edict
{
template <typename T>
class Single final
{
public:
    static T& instance()
    {
        static T object;
        return object;
    }

    Single() = delete;
    Single(const Single &) = delete;
    Single(Single &&) = delete;
    bool operator= (const Single &) = delete;
    bool operator= (Single &&) = delete;
};

}
