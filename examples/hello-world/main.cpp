/*
 * Edict is a blackboard messaging system -- have fun!
 * Copyright (c) 2018 Alex Raymond, Kier Dugan.
 */

#include <edict/edict.h>
using namespace edict;

#include <iostream>
#include <memory>
#include <regex>
using namespace std;


// ================================================================================================
void helloHandler(const string &message_)
{
    cout << message_ << ", Handler!" << endl;
}

void printer(const string &message_)
{
    cout << "printer: " << message_ << endl;
}


// ================================================================================================
class DirectPrinter final
{
public:
    explicit DirectPrinter(const string &name_) :
        m_name(name_)
    {
    }

    void print(const string &message_)
    {
        cout << "DirectPrinter(\"" << m_name << "\"): " << message_ << endl;
    }

    bool operator== (const DirectPrinter &other_) const
    {
        return m_name == other_.m_name;
    }

private:
    const string m_name;
};


// ================================================================================================
class IndirectPrinter final
{
public:
    explicit IndirectPrinter(const string &name_) :
        m_name(name_)
    {
    }

    void print(const string &message_)
    {
        cout << "IndirectPrinter(\"" << m_name << "\"): " << message_ << endl;
    }

private:
    const string m_name;
};


// ================================================================================================
int main(int argc, char **argv)
{
    edict::Broadcaster broadcaster;

    broadcaster.subscribe("/edict/hello", &helloHandler);
    broadcaster.subscribe("/edict/hello", &printer);
    broadcaster.subscribe(regex("(\\+|-)?[[:digit:]]+"), &helloHandler);
    broadcaster.subscribe([](const string &topic_) { return topic_.size() < 6; }, &printer);

    DirectPrinter printer { "DotMatrix" };
    broadcaster.subscribe("/edict/hello", { printer, &DirectPrinter::print });

    auto pronter = make_unique<IndirectPrinter>("OnkJot");
    broadcaster.subscribe("/edict/hello", { pronter.get(), &IndirectPrinter::print });

	broadcaster.publish("/edict/hello", "Hello");
    broadcaster.publish("1234", "Bye");

    return 0;
}
