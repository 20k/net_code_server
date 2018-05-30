#include "scheduled_tasks.hpp"
#include <windows.h>
#include <secret/node.hpp>
#include <secret/npc_manager.hpp>
#include "rng.hpp"

void task_thread(scheduled_tasks& tasks)
{
    sf::Clock clk;

    while(1)
    {
        tasks.check_all_tasks(clk.getElapsedTime().asMicroseconds() / 1000. / 1000.);

        Sleep(1000);
    }
}

void on_finish_relink(int cnt, const std::vector<std::string>& data)
{
    /*if(data.size() != 2)
        return;

    std::cout << "FINISHED RELINK timeout yay! " << data[0] << " " << data[1] << std::endl;*/

    if(data.size() <= 1)
        return;

    auto u1 = get_user(data.front(), -2);
    auto u2 = get_user(data.back(), -2);

    if(!u1.has_value() || !u2.has_value())
        return;

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();

    if(playspace_network_manage.current_network_links(u2->name) >= playspace_network_manage.max_network_links(u2->name))
        return;

    float minimum_strength = playspace_network_manage.get_minimum_path_link_strength(data);

    float link_stability_cost = 50;

    if(minimum_strength < link_stability_cost)
        return;

    float total_path_stability = playspace_network_manage.get_total_path_link_strength(data);

    for(int i=0; i < (int)data.size() - 1; i++)
    {
        playspace_network_manage.modify_neighbour_link_strength(data[i], data[i+1], -link_stability_cost);
    }

    user& start = u1.value();
    user& fin = u2.value();

    vec3f start_pos = start.pos;
    vec3f fin_pos = fin.pos;

    vec3f direction_vector = (fin_pos - start_pos).norm();

    ///YEAH THIS ISNT SO GREAT OK
    vec3f new_position = fin_pos + direction_vector * (40.f + get_random_float() * 20.f);

    start.pos = new_position;

    playspace_network_manage.unlink_all(start.name);
    playspace_network_manage.link(start.name, fin.name);
    playspace_network_manage.set_neighbour_link_strength(start.name, fin.name, total_path_stability / ((int)data.size() - 1));

    {
        mongo_lock_proxy ctx = get_global_mongo_user_info_context(-2);

        start.overwrite_user_in_db(ctx);
    }

    ///ok
    ///need to rip up data[0]
    ///and move it over to data.back()

    ///need to also modify its position in global space

    printf("relinked\n");
}
