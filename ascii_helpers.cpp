#include "ascii_helpers.hpp"

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

std::string stolower(const std::string& in)
{
    std::string ret = in;

    for(int i=0; i < (int)ret.size(); i++)
    {
        ret[i] = tolower(ret[i]);
    }

    return ret;
}
