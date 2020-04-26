#ifndef TLS_HPP_INCLUDED
#define TLS_HPP_INCLUDED

#include "safe_thread.hpp"

namespace tls_detail
{
    template<typename T, typename U>
    inline
    T* tls_fetch(pthread_key_t key, const U& u)
    {
        T* ptr = nullptr;

        if((ptr = (T*)pthread_getspecific(key)) == NULL)
        {
            ptr = new T();

            u(*ptr);

            pthread_setspecific(key, ptr);
        }

        return ptr;
    }

    template<typename T>
    inline
    void tls_freer(void* in)
    {
        if(in == nullptr)
            return;

        delete (T*)in;
    }

}

template<typename T, T init>
struct tls_variable
{
    pthread_key_t key;

    tls_variable()
    {
        assert(pthread_key_create(&key, tls_detail::tls_freer<T>) == 0);
    }

    T* get()
    {
        return tls_detail::tls_fetch<T>(key, [](T& i){i = init;});
    }
};

#endif // TLS_HPP_INCLUDED
