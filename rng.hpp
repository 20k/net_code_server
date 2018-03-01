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

inline
std::string generate_user_port()
{
    std::vector<std::string> names
    {
        "c",
        "c_f",
        "core",
        "frag",
        "fragment",
        "c_fragment",
        "core_frag",
        "core_f",
        "f"
    };

    std::vector<std::string> port
    {
        "alpha",
        "beta",
        "gamma",
        "delta",
        "epsilon",
        "zeta",
        "eta",
        "theta",
        "iota",
        "kappa",
        "lambda",
        "mu",
        "nu",
        "xi",
        "omicron",
        "pi",
        "rho",
        "sigma",
        "tau",
        "upsilon",
        "phi",
        "chi",
        "psi"
        ///did i forget one?
    };

    return random_select_of(1, names)[0] + "_" + random_select_of(1, port)[0] + random_lowercase_ascii_string(4);
    ///so, c_f_phi23
    ///or core_fragment_alpha_9734
}

#endif // RNG_HPP_INCLUDED
