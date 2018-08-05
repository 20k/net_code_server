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

std::string ascii_render_from_accessibility_info(network_accessibility_info& info, std::vector<std::vector<std::string>>& buffer, vec3f centre, float mult, ascii::ascii_render_flags flags, std::string user_name)
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

    if((flags & ascii::AVERAGE) > 0)
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

    if((flags & ascii::FIT_TO_AREA) > 0)
    {
        vec2f tl = {FLT_MAX, FLT_MAX};
        vec2f br = {-FLT_MAX, -FLT_MAX};

        for(auto& i : info.global_pos)
        {
            tl = min(i.second.xy(), tl);
            br = max(i.second.xy(), br);
        }

        for(auto& i : info.global_pos)
        {
            i.second.xy() = i.second.xy() - tl;
        }

        int new_w = fabs(br.x() - tl.x()) + 2;
        int new_h = fabs(br.y() - tl.y()) + 2;

        new_w = clamp(new_w, 5, 200);
        new_h = clamp(new_h, 5, 100);

        w = new_w;
        h = new_h;

        buffer = ascii_make_buffer({w, h}, false);
    }

    std::map<int, std::string> id_to_override;
    std::map<std::string, std::string> name_to_override;

    std::map<std::string, vec2f> node_to_pos;

    for(auto& i : info.rings)
    {
        node_to_pos[i.first] = info.global_pos[i.first].xy();
    }

    int hacky_id = 1;

    for(auto& i : node_to_pos)
    {
        const std::string& name = i.first;
        vec2f pos = i.second;

        std::vector<std::string> connections;

        if((flags & ascii::USE_SYS) == 0)
            connections = playspace_network_manage.get_links(name);
        else
        {
            std::optional<low_level_structure*> str_opt = low_level_structure_manage.get_system_from_name(name);

            low_level_structure& str = *str_opt.value();

            connections = str.get_connected_systems();
        }

        vec2f rounded_pos = round(pos);

        int map_id = ((int)rounded_pos.y()) * w + (int)rounded_pos.x();

        if(id_to_override[map_id] == "")
        {
            id_to_override[map_id] = name;
            //name_to_override[name] = name;
        }
        else
        {
            name_to_override[name] = id_to_override[map_id];
        }

        //int colour_offset_count = 0;

        std::string col = string_to_colour(name);

        ///alright so
        ///need to keep track of the first time we've seen something
        ///in a position
        ///if we're the second+ instance of something (aka its non zero)
        ///we should replace our key
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

            vec2f cur = (vec2f){pos.x(), pos.y()};

            for(int i=0; i < out_num; i++)
            {
                vec2f rpos = round(cur);
                vec2i ipos = clamp((vec2i){rpos.x(), rpos.y()}, (vec2i){0,0}, (vec2i){w-1, h-1});

                buffer[ipos.y()][ipos.x()] = "`" + col + ".`";

                cur += out_dir;
            }

            //colour_offset_count++;
        }

        hacky_id++;
    }

    auto local_display_string = info.display_string;

    for(auto& i : node_to_pos)
    {
        const std::string& name = i.first;

        if(name_to_override[name] != name && name_to_override[name].size() > 0)
        {
            local_display_string[name] = info.display_string[name_to_override[name]];
            continue;
        }

        vec2i clamped = clamp((vec2i){i.second.x(), i.second.y()}, (vec2i){0, 0}, (vec2i){w-1, h-1});

        std::string to_display = "`" + string_to_colour(i.first) + info.display_string[i.first] + "`";

        buffer[clamped.y()][clamped.x()] = to_display;
    }

    if((flags & ascii::HIGHLIGHT_USER) > 0)
    {
        for(auto& i : node_to_pos)
        {
            std::string my_name = i.first;

            if(my_name != user_name)
                continue;

            char arr[9] = {' ', 'v', ' ',
                           '>', ' ', '<',
                           ' ', '^', ' '};

            //for(int y=-1; y <= 1; y++)
            {
                int y = 0;

                for(int x=-1; x <= 1; x++)
                {
                    if(abs(x) == abs(y))
                        continue;

                    int real_x = x + i.second.x();
                    int real_y = y + i.second.y();

                    if(real_x < 0 || real_x >= w-1 || real_y < 0 || real_y >= h-1)
                        continue;

                    int offset = (y+1) * 3 + (x+1);

                    buffer[real_y][real_x] = std::string(1, arr[offset]);
                }
            }
        }
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
            std::string col_name = string_to_colour(info.keys[y].first);
            std::string col_char = string_to_colour(info.keys[y].first);

            std::string extra_data = info.extra_data_map[info.keys[y].first];

            if(name_to_override[info.keys[y].first] != "")
            {
                col_char = string_to_colour(name_to_override[info.keys[y].first]);
            }

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

            std::string name = "`" + col_name + info.keys[y].first + "`";

            //std::string extra_str = std::to_string((int)global_pos[name].x()) + ", " + std::to_string((int)global_pos[name].y());

            built += "      `" + col_char + local_display_string[info.keys[y].first] + "`";

            if(info.keys[y].first.size() > 0)
                built += " | " + name;// + " | [" + extra_str + "]";

            if(extra_data.size() != 0)
            {
                built += " " + extra_data;
            }

            //built += "`";
        }

        built += "\n";
    }

    return built;
}
