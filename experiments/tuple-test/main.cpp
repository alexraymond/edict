// TupleTest.cpp : Defines the entry point for the console application.
//

#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
using namespace std;

#include <QByteArray>
#include <QBuffer>
#include <QDataStream>


//=============================================================================
template <typename T>
void print(T arg)
{
    cout << arg << endl;
}

template <typename T, typename... Args>
void print(T first, Args... args)
{
    cout << first << ", ";
    print(args...);
}


//=============================================================================
// implementation details, users never invoke these directly
namespace detail
{
template <typename C, typename F, typename Tuple, bool Done, int Total, int... N>
struct call_member_impl
{
    static void call(C *obj_, F f_, Tuple &&t_)
    {
        //cout << __FUNCSIG__ << endl;
        call_member_impl<C, F, Tuple, Total == 1 + sizeof...(N), Total, N..., sizeof...(N)>::call(obj_, f_, std::forward<Tuple>(t_));
    }
};

template <typename C, typename F, typename Tuple, int Total, int... N>
struct call_member_impl<C, F, Tuple, true, Total, N...>
{
    static void call(C *obj_, F f_, Tuple &&t_)
    {
        //cout << __FUNCSIG__ << endl;
        (obj_->*f_)(std::get<N>(std::forward<Tuple>(t_))...);
    }
};

template <typename F, typename Tuple, bool Done, int Total, int... N>
struct call_impl
{
    static void call(F f, Tuple && t)
    {
        //cout << __FUNCSIG__ << endl;
        call_impl<F, Tuple, Total == 1 + sizeof...(N), Total, N..., sizeof...(N)>::call(f, std::forward<Tuple>(t));
    }
};

template <typename F, typename Tuple, int Total, int... N>
struct call_impl<F, Tuple, true, Total, N...>
{
    static void call(F f, Tuple && t)
    {
        //cout << __FUNCSIG__ << endl;
        f(std::get<N>(std::forward<Tuple>(t))...);
    }
};
}

template <typename C, typename F, typename Tuple>
void call(C *obj_, F f_, Tuple &&t_)
{
    typedef typename std::decay<Tuple>::type ttype;
    detail::call_member_impl<C, F, Tuple, 0 == std::tuple_size<ttype>::value, std::tuple_size<ttype>::value>::call(obj_, f_, std::forward<Tuple>(t_));
}

// user invokes this
template <typename F, typename Tuple>
void call(F f, Tuple && t)
{
    //cout << __FUNCSIG__ << endl;
    typedef typename std::decay<Tuple>::type ttype;
    detail::call_impl<F, Tuple, 0 == std::tuple_size<ttype>::value, std::tuple_size<ttype>::value>::call(f, std::forward<Tuple>(t));
}

void test(int n, double f, bool b)
{
    cout << "Caller test (" << b << "): " << n << " + " << f << " = " << n + f << endl;
}


//=============================================================================
namespace encode_detail
{

template <typename T>
void encode(QDataStream &stream_, const T &field_)
{
    //cout << __FUNCSIG__ << endl;

    stream_ << field_;
}

template <typename T, typename... Args>
void encode(QDataStream &stream_, const T &first_, Args... args_)
{
    //cout << __FUNCSIG__ << endl;

    stream_ << first_;
    encode(stream_, args_...);
}

template <typename T, typename... Args>
struct DecodeTag
{
    typedef typename remove_cv<typename remove_reference<T>::type>::type type;
};

unsigned ORDER = 0;

template <typename T>
tuple<typename decay<T>::type> decode(QDataStream &stream_, DecodeTag<T> tag_)
{
    //cout << __FUNCSIG__ << "  --  " << ORDER++ << endl;

    DecodeTag<T>::type field{};

    stream_ >> field;

    return make_tuple(field);
}

template <typename T, typename... Args>
tuple<typename decay<T>::type, typename decay<Args>::type...> decode(QDataStream &stream, DecodeTag<T, Args...> tag_)
{
    //cout << __FUNCSIG__ << "  --  " << ORDER++ << endl;

    auto t = decode(stream, DecodeTag<T>{});

    return tuple_cat(t, decode(stream, DecodeTag<Args...>{}));
}

}


//=============================================================================
template <typename... Args>
QByteArray encode(Args... args_)
{
    QBuffer buffer;

    buffer.open(QBuffer::WriteOnly);

    QDataStream stream{ &buffer };

    encode_detail::encode(stream, args_...);

    return buffer.data();
}

template <typename... Args>
tuple<typename decay<Args>::type...> decode(const QByteArray &data)
{
    QBuffer buffer(const_cast<QByteArray *>(&data));

    buffer.open(QBuffer::ReadOnly);

    QDataStream stream{ &buffer };

    return encode_detail::decode(stream, encode_detail::DecodeTag<Args...>{});
}

template <typename Ret, typename Class, typename... Args>
void call_decode(const QByteArray &data_, Class *obj_, Ret(Class::*handler_)(Args...))
{
    auto args = decode<Args...>(data_);

    call(obj_, handler_, args);
}

template <typename Ret, typename... Args>
void call_decode(const QByteArray &data_, Ret (*handler_)(Args...))
{
    auto args = decode<Args...>(data_);

    call(handler_, args);
}

void normal_handler(int n, double d, bool b, const QByteArray &s)
{
    cout << "In function:\n"
         << "  n = " << n << '\n'
         << "  d = " << d << '\n'
         << "  b = " << b << '\n'
         << "  s = " << s.toStdString() << endl;
}

class ClassHandler
{
    string m_name;

public:
    ClassHandler(const string &name_) :
        m_name(name_)
    {
    }

    void member_handler(int n, double d, bool b, const QByteArray &s)
    {
        cout << "In " << m_name << ".member_handler:\n"
            << "  n = " << n << '\n'
            << "  d = " << d << '\n'
            << "  b = " << b << '\n'
            << "  s = " << s.toStdString() << endl;
    }
};


//=============================================================================
int main()
{
    cout << "Simple test: ";

    print(1, 2, "hello", "world", 5, 9.08);

    auto t = make_tuple(1, 1.4, false);
    call(test, t);

    auto data = encode(1, 1.4, true, "testing");
    cout << "Encoded data (" << data.size() << " bytes): \n";
    for (auto c : data)
        cout << hex << c;
    cout << endl;

    auto t2 = decode<int, double, bool, QByteArray>(data);

    cout << "Decoded: " << get<0>(t2) << ", "
                        << get<1>(t2) << ", "
                        << get<2>(t2) << ", "
                        << get<3>(t2).toStdString() << endl;

    call_decode(data, &normal_handler);

    ClassHandler handler("test");

    call_decode(data, &handler, &ClassHandler::member_handler);

    /*call_decode(data, [] (int n, double d, bool b, const QByteArray &s) -> void
    {
        cout << "In lambda:\n"
             << "  n = " << n << '\n'
             << "  d = " << d << '\n'
             << "  b = " << b << '\n'
             << "  s = " << s.toStdString() << endl;
    });*/

    return 0;
}
