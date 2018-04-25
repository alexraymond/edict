/*
* Edict is a blackboard messaging system -- have fun!
* Copyright (c) 2018 Alex Raymond, Kier Dugan.
*/

#include <edict/edict.h>
using namespace edict;

#include <iostream>
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

	broadcaster.publish("/edict/hello", "Hello");

	return 0;
}
