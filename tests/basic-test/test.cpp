#include <edict/Broadcaster.h>
#include <edict/Single.h>
using namespace edict;
using namespace std;

#include <catch.hpp>

#include <regex>


class TestPublisher
{
public:
    TestPublisher()
    {
        m_broadcaster.subscribe("change/text", {this, &TestPublisher::changeText});
        m_broadcaster.subscribe(regex("edict/[a-z]+"), {this, &TestPublisher::changeText});
        auto palindrome = [](const std::string &text_) -> bool
        {
            return text_ == std::string(text_.rbegin(), text_.rend());
        };
        m_broadcaster.subscribe(palindrome, {this, &TestPublisher::changeText});
    }

    string testPublish(const string &topic_, const string &text_)
    {
        m_broadcaster.publish(topic_, text_);
        return text_;
    }

    void changeText(const string &text_)
    {
        m_text = text_;
    }

    string text() const
    {
        return m_text;
    }

private:
    Broadcaster m_broadcaster;
    string m_text;
};

TestPublisher t;

TEST_CASE("Simple Publish", "[publish]")
{
    REQUIRE(t.text() == t.testPublish("change/text", "test"));
    REQUIRE(t.text() == t.testPublish("change/text", "abc"));
}

TEST_CASE("Regex Publish", "[publish]")
{
    REQUIRE(t.text() == t.testPublish("edict/hello", "world"));
    REQUIRE(t.text() == t.testPublish("edict/keep", "testing"));
    REQUIRE(t.text() != t.testPublish("edictedict", "mustFail"));
}

TEST_CASE("Predicate Publish", "[publish]")
{
    REQUIRE(t.text() == t.testPublish("racecar", "palindrome1"));
    REQUIRE(t.text() != t.testPublish("notapalindrome", "mustFail"));
    REQUIRE(t.text() == t.testPublish("redivider", "palindrome2"));
}


