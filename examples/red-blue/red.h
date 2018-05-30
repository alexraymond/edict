/*
* Edict is a blackboard messaging system -- have fun!
* Copyright (c) 2018 Alex Raymond, Kier Dugan.
*/

#include <edict/edict.h>
#include <edict/Single.h>

#include <iostream>
#include <string>

class Red
{
public:
   Red() :
       m_broadcaster(edict::Single<edict::Broadcaster>::instance())
   {
       m_broadcaster.subscribe("colour", {this, &Red::printColour});
       m_broadcaster.subscribe("rainbow", {this, &Red::hello});
   }

   void hello(const std::string &message_)
   {
       std::cout << "(Red) Rainbow says: " << message_ << std::endl;
   }

   void printColour(const std::string &colour_)
   {
       std::cout << "(Red) Red thinks " << colour_;
       if(colour_ != "red")
       {
           std::cout << " is not a real colour!" << std::endl;
           m_broadcaster.publish("redder", "");
       }
       else
       {
           std::cout << " is awesome!" << std::endl;
       }
   }

private:
   edict::Broadcaster &m_broadcaster;
};
