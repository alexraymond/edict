/*
 * Edict is a blackboard messaging system -- have fun!
 * Copyright (c) 2018 Alex Raymond, Kier Dugan.
 */

#include "red.h"
#include "blue.h"

#include <edict/edict.h>
#include <edict/Single.h>
using namespace edict;

#include <iostream>
#include <memory>
#include <regex>
using namespace std;


// ================================================================================================
int main(int argc, char **argv)
{
    edict::Broadcaster &broadcaster = Single<Broadcaster>::instance();

    Red red;
    Blue blue;

    broadcaster.publish("rainbow", "rise and shine!");
    broadcaster.publish("colour", "red");
    broadcaster.publish("colour", "blue");
    return 0;
}
