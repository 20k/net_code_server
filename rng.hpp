#ifndef RNG_HPP_INCLUDED
#define RNG_HPP_INCLUDED

#include <string>
#include <vector>
#include <random>
#include <algorithm>

std::string random_binary_string(int len);

inline
uint32_t get_random_uint32_t()
{
    std::string str = random_binary_string(4);

    uint32_t ret;

    memcpy((char*)&ret, str.c_str(), sizeof(uint32_t));

    return ret;
}

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

template<typename T>
inline
T random_select_one(const std::vector<T>& of)
{
    return random_select_of(1, of).front();
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

template<typename T>
inline
std::string random_select_of_det(int len, const std::string& of, T& t)
{
    std::string ret;

    for(int i=0; i < len; i++)
    {
        ret.push_back(of[t() % of.size()]);
    }

    return ret;
}

template<typename T, template<typename, typename> typename U>
inline
T random_select_of_rarity(U<T, float>& rarities, float rarity)
{
    float max_rare = 0;

    for(auto& i : rarities)
        max_rare += i.second;

    float accum = 0;

    for(auto& i : rarities)
    {
        if(rarity <= accum / max_rare)
            return i.first;

        accum += i.second;
    }

    return rarities.front()->first;
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

template<typename U, typename T>
inline
void shuffle_csprng_seed(T& t)
{
    uint32_t seed = get_random_uint32_t();

    U gen(seed);

    std::shuffle(t.begin(), t.end(), gen);
}

#endif // RNG_HPP_INCLUDED
