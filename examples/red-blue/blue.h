/*
* Edict is a blackboard messaging system -- have fun!
* Copyright (c) 2018 Alex Raymond, Kier Dugan.
*/

#include <edict/edict.h>
#include <edict/Single.h>

#include <iostream>
#include <string>

class Blue
{
public:

   Blue() :
       m_broadcaster(edict::Single<edict::Broadcaster>::instance())
   {
       m_broadcaster.subscribe("colour", {this, &Blue::printColour});
       m_broadcaster.subscribe("rainbow", {this, &Blue::hello});
       auto palindrome = [](const std::string &text_) -> bool
       {
           return text_ == std::string(text_.rbegin(), text_.rend());
       };
       m_broadcaster.subscribe(palindrome, {this, &Blue::bonus});
   }

   void hello(const std::string &message_)
   {
       std::cout << "(Blue) Rainbow says: " << message_ << std::endl;
   }

   void printColour(const std::string &colour_)
   {
       std::cout << "(Blue) Blue thinks " << colour_;
       if(colour_ != "blue")
       {
           std::cout << " is not a real colour!";
       }
       else
       {
           std::cout << " is awesome!";
       }
       std::cout << std::endl;
   }

   void bonus(const std::string &text_)
   {
       std::cout << "(Blue) Palindrome topic! :)" << std::endl;
   }

private:
   edict::Broadcaster &m_broadcaster;
};
