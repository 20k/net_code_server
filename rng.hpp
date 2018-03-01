#ifndef RNG_HPP_INCLUDED
#define RNG_HPP_INCLUDED

#include <string>
#include <vector>

std::string random_binary_string(int len);

template<typename T>
inline
std::vector<T> random_select_of(int len, const std::vector<T>& of)
{
    std::string random = random_binary_string(len);

    std::vector<T> ret;

    for(int i=0; i < len; i++)
    {
        ret.push_back(of[((uint8_t)random[i]) % of.size()]);
    }

    return ret;
}

inline
std::string random_select_of(int len, const std::string& of)
{
    std::string random = random_binary_string(len);

    std::string ret;

    for(int i=0; i < len; i++)
    {
        ret.push_back(of[((uint8_t)random[i]) % of.size()]);
    }

    return ret;
}

inline
std::string random_lowercase_ascii_string(int len)
{
    return random_select_of(len, "abcdefghijklmnopqrstuvwxyz0123456789");
}

#endif // RNG_HPP_INCLUDED
