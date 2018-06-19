#include "ascii_helpers.hpp"

std::vector<std::vector<std::string>> ascii_make_buffer(vec2i dim)
{
    std::vector<std::vector<std::string>> rbuffer;

    rbuffer.resize(dim.y());

    for(int i=0; i < dim.y(); i++)
    {
        rbuffer[i].resize(dim.x());

        for(int x=0; x < (int)rbuffer[i].size() - 1; x++)
        {
            rbuffer[i][x] = ' ';
        }

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

        vec2i ipos = clamp((vec2i){rpos.x(), rpos.y()}, (vec2i){0,0}, (vec2i){width-1, height-1});

        //str[ipos.y() * w + ipos.x()] = "`" + col + ".`";

        buffer[ipos.y()][ipos.x()] = "`" + colour + ".`";

        cur += out_dir;
    }
}