#include "ascii_helpers.hpp"
#include <secret/npc_manager.hpp>
#include <secret/low_level_structure.hpp>
#include <libncclient/nc_util.hpp>

std::vector<std::vector<std::string>> ascii_make_buffer(vec2i dim, bool add_newlines)
{
    std::vector<std::vector<std::string>> rbuffer;

    rbuffer.resize(dim.y());

    for(int i=0; i < dim.y(); i++)
    {
        rbuffer[i].resize(dim.x());

        for(int x=0; x < (int)rbuffer[i].size(); x++)
        {
            rbuffer[i][x] = ' ';
        }

        if(add_newlines)
            rbuffer[i][(int)rbuffer[i].size() -1] = '\n';
    }

    return rbuffer;
}

std::string ascii_stringify_buffer(const std::vector<std::vector<std::string>>& buffer)
{
    std::string output;

    for(auto& i : buffer)
    {
        for(auto& j : i)
        {
            output += j;
        }
    }

    return output;
}

void ascii_draw_line(vec2f start, vec2f finish, const std::string& colour, std::vector<std::vector<std::string>>& buffer)
{
    vec2f out_dir;
    int out_num;

    line_draw_helper(start, finish, out_dir, out_num);

    vec2f cur = start;

    for(int i=0; i < out_num; i++)
    {
        vec2f rpos = round(cur);

        int height = buffer.size();
        int clamped_y = clamp((int)rpos.y(), 0, height-1);
        int width = buffer[clamped_y].size();

        vec2i ipos = clamp((vec2i){rpos.x(), rpos.y()}, (vec2i){0,0}, (vec2i){width-2, height-1});

        //str[ipos.y() * w + ipos.x()] = "`" + col + ".`";

        buffer[ipos.y()][ipos.x()] = "`" + colour + ".`";

        cur += out_dir;
    }
}

std::string ascii_index_to_character(int idx)
{
    std::string str = "0123456789abcdefghijklmnopqrstuvwxyz";

    idx = idx % str.size();

    return std::string(1, str[idx]);
}

std::string ascii_index_to_full_character(int idx)
{
    std::string str = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    idx = idx % str.size();

    return std::string(1, str[idx]);
}

std::string stolower(const std::string& in)
{
    std::string ret = in;

    for(int i=0; i < (int)ret.size(); i++)
    {
        ret[i] = tolower(ret[i]);
    }

    return ret;
}

std::string id_to_roman_numeral(int x)
{
    if(x < 0)
        return "";

    if(x == 0)
        return "";

    if(x == 1)
        return "i";

    if(x == 2)
        return "ii";

    if(x == 3)
        return "iii";

    if(x == 4)
        return "iv";

    if(x == 5)
        return "v";

    if(x == 6)
        return "vi";

    if(x == 7)
        return "vii";

    if(x == 8)
        return "viii";

    if(x == 9)
        return "ix";

    if(x == 10)
        return "x";

    return "x" + id_to_roman_numeral(x-10);
}

std::string ascii_render_from_accessibility_info(network_accessibility_info& info, std::vector<std::vector<std::string>>& buffer, vec3f centre, bool average_camera, float mult, bool use_sys_connections)
{
    if(buffer.size() == 0)
        return "";

    if(buffer[0].size() == 0)
        return "";

    playspace_network_manager& playspace_network_manage = get_global_playspace_network_manager();
    low_level_structure_manager& low_level_structure_manage = get_global_low_level_structure_manager();

    int h = buffer.size();
    int w = buffer[0].size();

    vec3f cur_center = centre;

    vec3f accum = {0,0,0};

    for(auto& i : info.rings)
    {
        accum += info.global_pos[i.first];
    }

    if(info.rings.size() > 0)
    {
        accum = accum / (float)info.rings.size();
    }

    if(average_camera)
    {
        accum = (accum + cur_center)/2.f;
    }
    else
    {
        accum = cur_center;
    }

    for(auto& i : info.global_pos)
    {
        i.second = i.second * mult;
    }

    for(auto& i : info.global_pos)
    {
        i.second = i.second - accum * mult;
    }

    for(auto& i : info.global_pos)
    {
        i.second += (vec3f){w/2.f, h/2.f, 0.f};

        i.second = round(i.second);
    }

    std::map<std::string, vec2f> node_to_pos;

    for(auto& i : info.rings)
    {
        node_to_pos[i.first] = info.global_pos[i.first].xy();
    }

    for(auto& i : node_to_pos)
    {
        const std::string& name = i.first;
        vec2f pos = i.second;

        std::vector<std::string> connections;

        if(!use_sys_connections)
            connections = playspace_network_manage.get_links(name);
        else
        {
            std::optional<low_level_structure*> str_opt = low_level_structure_manage.get_system_from_name(name);

            low_level_structure& str = *str_opt.value();

            connections = str.get_connected_systems();
        }

        int colour_offset_count = 0;

        for(auto& conn : connections)
        {
            auto found = node_to_pos.find(conn);

            if(found == node_to_pos.end())
                continue;

            if(conn < name)
                continue;

            vec2f to_draw_pos = found->second;

            vec2f out_dir;
            int out_num;

            line_draw_helper(pos, to_draw_pos, out_dir, out_num);

            /*vec2i idiff = to_draw_pos - pos;

            vec2f fdiff = (vec2f){idiff.x(), idiff.y()};

            out_dir = fdiff.norm();
            out_num = fdiff.length();*/

            std::string col = string_to_colour(name);

            if((colour_offset_count % 2) == 1)
                col = string_to_colour(conn);

            vec2f cur = (vec2f){pos.x(), pos.y()};

            for(int i=0; i < out_num; i++)
            {
                vec2f rpos = round(cur);
                vec2i ipos = clamp((vec2i){rpos.x(), rpos.y()}, (vec2i){0,0}, (vec2i){w-1, h-1});

                buffer[ipos.y()][ipos.x()] = "`" + col + ".`";

                cur += out_dir;
            }

            colour_offset_count++;
        }
    }

    for(auto& i : node_to_pos)
    {
        vec2i clamped = clamp((vec2i){i.second.x(), i.second.y()}, (vec2i){0, 0}, (vec2i){w-1, h-1});

        std::string to_display = "`" + string_to_colour(i.first) + info.display_string[i.first] + "`";

        buffer[clamped.y()][clamped.x()] = to_display;
    }

    /*for(auto& i : keys)
    {
        i.second = i.second + " " + std::to_string((int)global_pos[i.first].x()) + " " + std::to_string((int)global_pos[i.first].y());
    }*/

    info.keys.insert(info.keys.begin(), {"", "Key"});

    std::string built;

    for(int y=0; y < h; y++)
    {
        for(int x=0; x < w; x++)
        {
            built += buffer[y][x];
        }

        if(y < (int)info.keys.size())
        {
            std::string col = string_to_colour(info.keys[y].first);

            //#define ITEM_DEBUG
            #ifdef ITEM_DEBUG
            std::optional user_and_nodes = get_user_and_nodes(keys[y].first, get_thread_id(ctx));

            if(user_and_nodes.has_value())
            {
                user_nodes& nodes = user_and_nodes->second;

                int num_items = 0;

                for(user_node& node : nodes.nodes)
                {
                    num_items += node.attached_locks.size();
                }

                num_items = user_and_nodes->first.num_items();

                if(num_items == 0)
                {
                    col = "L";
                }
                else
                {
                    col = "D";
                }
            }
            #endif // ITEM_DEBUG

            std::string name = info.keys[y].first;

            //std::string extra_str = std::to_string((int)global_pos[name].x()) + ", " + std::to_string((int)global_pos[name].y());

            built += "      `" + col + info.keys[y].second;

            if(info.keys[y].first.size() > 0)
                built += " | " + info.keys[y].first;// + " | [" + extra_str + "]";

            built += "`";
        }

        built += "\n";
    }

    return built;
}
