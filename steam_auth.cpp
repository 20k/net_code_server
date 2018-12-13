#include "steam_auth.hpp"
#include "command_handler_state.hpp"
#include "command_handler.hpp"

#include <stdint.h>
#include <steamworks_sdk_142/sdk/public/steam/steamencryptedappticket.h>

#include <vector>
#include "time.hpp"
#include <libncclient/nc_util.hpp>

std::optional<steam_auth_data> get_steam_auth(const std::string& hex_auth_data)
{
    std::vector<uint8_t> decrypted;
    decrypted.resize(1024);

    static std::string secret_key;
    static bool loaded = false;

    ///814820
    ///try it with the hardcoded spacewar token
    if(!loaded)
    {
        static std::mutex lock;
        std::lock_guard guard(lock);

        std::string hex_key = read_file_bin("deps/secret/akey.ect");

        secret_key = hex_to_binary(hex_key, true);

        loaded = true;
    }

    //std::cout << "secret key length " << secret_key.size() << std::endl;
    //std::cout << "expected length " << k_nSteamEncryptedAppTicketSymmetricKeyLen << std::endl;

    //const uint8 spacewar_key[k_nSteamEncryptedAppTicketSymmetricKeyLen] = { 0xed, 0x93, 0x86, 0x07, 0x36, 0x47, 0xce, 0xa5, 0x8b, 0x77, 0x21, 0x49, 0x0d, 0x59, 0xed, 0x44, 0x57, 0x23, 0xf0, 0xf6, 0x6e, 0x74, 0x14, 0xe1, 0x53, 0x3b, 0xa3, 0x3c, 0xd8, 0x03, 0xbd, 0xbd };

    uint32_t real_size = 1024;

    std::string binary_data = hex_to_binary(hex_auth_data);

    std::cout << "ticket length " << binary_data.size() << std::endl;

    if(!SteamEncryptedAppTicket_BDecryptTicket((const uint8*)binary_data.c_str(), binary_data.size(), &decrypted[0], &real_size, (const uint8*)&secret_key[0], secret_key.size()))
    {
        printf("Failed to decrypt ticket\n");
        return std::nullopt;
    }

    decrypted.resize(real_size);

    if(decrypted.size() == 0)
    {
        printf("Decrypted size == 0\n");
        return std::nullopt;
    }

    //AppId_t found_appid = SteamEncryptedAppTicket_GetTicketAppID(&decrypted[0], decrypted.size());
    //std::cout << "appid is " << found_appid << std::endl;

    if(!SteamEncryptedAppTicket_BIsTicketForApp(&decrypted[0], decrypted.size(), 814820))
    {
        printf("Ticket is for wrong appid\n");
        return std::nullopt;
    }

    RTime32 ticket_time = SteamEncryptedAppTicket_GetTicketIssueTime(&decrypted[0], decrypted.size());

    if(ticket_time == 0)
    {
        printf("Bad Ticket Time");
        return std::nullopt;
    }

    ///I have no idea what a good time here is
    size_t timeout = 120*1000;

    ///steam api returns time since unix epoch in seconds
    if((size_t)ticket_time*1000 + timeout < get_wall_time())
    {
        std::cout << "Ticket is too old " << ticket_time << " wall " << get_wall_time() << std::endl;
        return std::nullopt;
    }

    CSteamID steam_id;

    SteamEncryptedAppTicket_GetTicketSteamID(&decrypted[0], decrypted.size(), &steam_id);

    if(steam_id == k_steamIDNil)
    {
        printf("Invalid ticket (bad steam id)\n");
        return std::nullopt;
    }


    uint32 UserDataLength = 0;

    const uint8* user_data_ptr = SteamEncryptedAppTicket_GetUserVariableData(&decrypted[0], decrypted.size(), &UserDataLength);

    std::cout << "steam id " << steam_id.ConvertToUint64() << std::endl;

    steam_auth_data ret;
    ret.steam_id = steam_id.ConvertToUint64();

    if(user_data_ptr != nullptr)
        ret.user_data = std::string((const char*)user_data_ptr, (uint32_t)UserDataLength);

    std::cout << "Received user data of length " << UserDataLength << std::endl;

    return ret;
}
