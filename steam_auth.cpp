#include "steam_auth.hpp"
#include "command_handler_state.hpp"
#include "command_handler.hpp"

#include <stdint.h>
#include <steamworks_sdk_142/sdk/public/steam/steamencryptedappticket.h>

#include <vector>
#include "time.hpp"

bool successful_steam_auth(std::shared_ptr<shared_command_handler_state> all_shared, const std::string& hex_auth_data)
{
    std::vector<uint8_t> decrypted;
    decrypted.resize(1024);

    std::vector<uint8_t> secret_key{1,2,3,4};

    uint32_t real_size = 0;

    std::string binary_data = hex_to_binary(hex_auth_data);

    if(!SteamEncryptedAppTicket_BDecryptTicket((const uint8*)binary_data.c_str(), binary_data.size(), &decrypted[0], &real_size, &secret_key[0], secret_key.size()))
    {
        printf("Failed to decrypt ticket\n");
        return false;
    }

    decrypted.resize(real_size);

    if(decrypted.size() == 0)
    {
        printf("Decrypted size == 0\n");
        return false;
    }

    if(SteamEncryptedAppTicket_BIsTicketForApp(&decrypted[0], decrypted.size(), 814820))
    {
        printf("Ticket is for wrong appid\n");
        return false;
    }

    RTime32 ticket_time = SteamEncryptedAppTicket_GetTicketIssueTime(&decrypted[0], decrypted.size());

    if(ticket_time == 0)
    {
        printf("Bad Ticket Time");
        return false;
    }

    ///I have no idea what a good time here is
    size_t timeout = 120*1000;

    if((size_t)ticket_time + timeout < get_wall_time())
    {
        printf("Ticket is too old\n");
        return false;
    }

    CSteamID steam_id;

    SteamEncryptedAppTicket_GetTicketSteamID(&decrypted[0], decrypted.size(), &steam_id);

    if(steam_id == k_steamIDNil)
    {
        printf("Invalid ticket (bad steam id)\n");
        return false;
    }

    std::cout << "steam id " << steam_id.ConvertToUint64() << std::endl;

    return true;
}
