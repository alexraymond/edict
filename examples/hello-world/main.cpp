/*
* Edict is a blackboard messaging system -- have fun!
* Copyright (c) 2018 Alex Raymond, Kier Dugan.
*/

#include <edict/edict.h>
using namespace edict;

#include <iostream>
#include <regex>
using namespace std;


void helloHandler(const string &message_)
{
    cout << message_ << ", Handler!" << endl;
}

void printer(const string &message_)
{
    cout << "printer: " << message_ << endl;
}

int main(int argc, char **argv)
{
    edict::Broadcaster broadcaster;

    broadcaster.subscribe("/edict/hello", &helloHandler);
    broadcaster.subscribe("/edict/hello", &printer);
    broadcaster.subscribe(regex("(\\+|-)?[[:digit:]]+"), &helloHandler);
    broadcaster.subscribe([](const string &topic_) { return topic_.size() < 6; }, &printer);

    broadcaster.publish("/edict/hello", "Hello");
    broadcaster.publish("1234", "Bye");

    return 0;
}
