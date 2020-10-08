#include "rng.hpp"
#include <assert.h>

#ifdef __WIN32__

#include <windows.h>
#include <Wincrypt.h>

std::string random_binary_string(int len)
{
    if(len <= 0)
        return std::string();

    //unsigned char random_bytes[len] = {0};

    std::vector<unsigned char> random_bytes;
    random_bytes.resize(len);

    HCRYPTPROV provider = 0;

    if(!CryptAcquireContext(&provider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
        return "";

    if(!CryptGenRandom(provider, len, &random_bytes[0]))
        return "";

    CryptReleaseContext(provider, 0);

    std::string to_ret;
    to_ret.resize(len);

    for(int i=0; i < len; i++)
    {
        to_ret[i] = random_bytes[i];
    }

    return to_ret;
}

#else

#include <stdio.h>
#include <stdexcept>

std::string random_binary_string(int len)
{
    if(len == 0)
        return "";

    FILE* fp = fopen("/dev/urandom", "rb");

    if(fp == nullptr)
        throw std::runtime_error("Could not read urandom");

    std::string ret;
    ret.resize(len);

    int rval = fread(&ret[0], 1, len * sizeof(char), fp);

    fclose(fp);

    assert(rval == len);

    return ret;
}

#endif
