#include "rng.hpp"

#include <windows.h>
#include <Wincrypt.h>

std::string random_binary_string(int len)
{
    if(len <= 0)
        return std::string();

    unsigned char random_bytes[len] = {0};

    HCRYPTPROV provider = 0;

    if(!CryptAcquireContext(&provider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
        return "";

    if(!CryptGenRandom(provider, len, random_bytes))
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
