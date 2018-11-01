#include "source_maps.hpp"
#include <map>
#include <vector>
#include <iostream>
#include <array>
#include <libncclient/nc_util.hpp>

#define BITS_PER_VLQ 5

#define MASK_VLQ ((1 << BITS_PER_VLQ) - 1)

#define CONT_VLQ 32

int32_t get_val_of_base64(char base_64)
{
    std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::map<char, int> values;

    int kidx = 0;

    for(auto& i : characters)
    {
        values[i] = kidx++;
    }

    return values[base_64];
}

int32_t signed_vlq_decode(int32_t val)
{
    bool sign_bit = (val & 1);
    val = val >> 1;
    return sign_bit ? -val : val;
}

template<typename T>
int32_t decode_from_fragment(T& string_iterator)
{
    bool cont = true;
    int32_t shift = 0;
    int32_t result = 0;

    while(cont)
    {
        char c = *string_iterator;

        string_iterator++;

        int digit = get_val_of_base64(c);
        cont = (digit & CONT_VLQ) != 0;
        digit &= MASK_VLQ;
        result += (digit << shift);
        shift += BITS_PER_VLQ;
    }

    return signed_vlq_decode(result);
}

std::vector<int> decode_segment(const std::string& in)
{
    std::string fragment = in;

    auto it = fragment.begin();

    std::vector<int> ret;

    while(it != fragment.end() && *it != ',' && *it != ';')
    {
        ret.push_back(decode_from_fragment(it));
    }

    return ret;
}

struct source_segment
{
    /*std::optional<int> generated_column;
    std::optional<int> source_index;
    std::optional<int> original_line;
    std::optional<int> original_column;
    std::optional<int> names_index;*/

    std::array<std::optional<int>, 5> vals;
};

struct source_line
{
    std::vector<source_segment> segments;

    ///pretty easy to get lines, just split by ;
    void decode_from_whole_line(source_segment& last_line, const std::string& in)
    {
        std::vector<std::string> parse_segments = no_ss_split(in, ",");

        //for(std::string& segment : segments)

        bool first = true;

        for(int i=0; i < (int)parse_segments.size(); i++)
        {
            source_segment last;

            if(i == 0)
                last = last_line;
            else
                last = segments[i-1];

            std::string seg = parse_segments[i];

            std::vector<int> decoded = decode_segment(seg);

            if(decoded.size() == 0)
                continue;

            source_segment found;

            for(int kk=0; kk < (int)decoded.size(); kk++)
            {
                int dec_val = decoded[kk];

                ///special handling for the first time we see this
                if(kk == 0 && first)
                {
                    found.vals[0] = dec_val;
                }
                else
                {
                    if(!last.vals[kk].has_value())
                    {
                        found.vals[kk] = dec_val;
                    }
                    else
                    {
                        found.vals[kk] = last.vals[kk].value() + dec_val;
                    }
                }
            }

            segments.push_back(found);

            first = false;
        }

        if(segments.size() > 0)
            last_line = segments.back();
    }
};

void source_map::decode(const std::string& str)
{

}

void source_map_tests()
{
    std::string fragment = "AAgBC";

    /*auto it = fragment.begin();

    std::cout << "yay! " << decode_from_fragment(it) << std::endl;
    std::cout << "yay! " << decode_from_fragment(it) << std::endl;
    std::cout << "yay! " << decode_from_fragment(it) << std::endl;
    std::cout << "yay! " << decode_from_fragment(it) << std::endl;*/

    auto decoded = decode_segment(fragment);

    for(auto& i : decoded)
    {
        std::cout << "decoded " << i << std::endl;
    }

    exit(0);
}
