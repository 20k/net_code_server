#ifndef GLOBAL_CACHING_HPP_INCLUDED
#define GLOBAL_CACHING_HPP_INCLUDED

#include <string>
#include <mutex>

template<typename T>
struct global_generic_cache
{
    std::map<std::string, T> cache;
    std::mutex mut;

    bool exists_in_cache(const std::string& name)
    {
        std::lock_guard guard(mut);

        return cache.find(name) != cache.end();
    }

    T load_from_cache(const std::string& name)
    {
        std::lock_guard guard(mut);

        return cache[name];
    }

    void overwrite_in_cache(const std::string& name, const T& data)
    {
        std::lock_guard guard(mut);

        cache[name] = data;
    }

    void delete_from_cache(const std::string& name)
    {
        std::lock_guard guard(mut);

        cache.erase(name);
    }

    void clear()
    {
        std::lock_guard guard(mut);

        cache.clear();
    }
};

#endif // GLOBAL_CACHING_HPP_INCLUDED
