#include "source_maps.hpp"
#include <map>
#include <vector>
#include <iostream>
#include <array>
#include <libncclient/nc_util.hpp>
#include <json/json.hpp>

#define BITS_PER_VLQ 5

#define MASK_VLQ ((1 << BITS_PER_VLQ) - 1)

#define CONT_VLQ 32

int32_t get_val_of_base64(char base_64)
{
    static std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static std::map<char, int> values;

    static bool init;

    if(!init)
    {
        int kidx = 0;

        for(auto& i : characters)
        {
            values[i] = kidx++;
        }

        init = true;
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

struct intermediate_source_segment
{
    /*std::optional<int> generated_column;
    std::optional<int> source_index;
    std::optional<int> original_line;
    std::optional<int> original_column;
    std::optional<int> names_index;*/

    std::array<std::optional<int>, 5> vals;

    std::string input;
};

struct intermediate_source_line
{
    std::vector<intermediate_source_segment> segments;

    ///pretty easy to get lines, just split by ;
    void decode_from_whole_line(intermediate_source_segment& last_line, const std::string& in)
    {
        std::vector<std::string> parse_segments = no_ss_split(in, ",");

        //for(std::string& segment : segments)

        bool first = true;

        for(int i=0; i < (int)parse_segments.size(); i++)
        {
            intermediate_source_segment last;

            if(i == 0)
                last = last_line;
            else
                last = segments[i-1];

            std::string seg = parse_segments[i];

            std::vector<int> decoded = decode_segment(seg);

            if(decoded.size() == 0)
                continue;

            intermediate_source_segment found;

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

            for(int kk=(int)decoded.size(); kk < 5; kk++)
            {
                found.vals[kk] = last.vals[kk];
            }

            found.input = seg;

            segments.push_back(found);

            first = false;
        }

        if(segments.size() > 0)
            last_line = segments.back();
    }

    std::string dump()
    {
        std::string ret = "Num segments " + std::to_string(segments.size()) + "\n";

        for(intermediate_source_segment& seg : segments)
        {
            for(int i=0; i < 5; i++)
            {
                if(seg.vals[i].has_value())
                {
                    ret += "i: " + std::to_string(i) + " val: " + std::to_string(seg.vals[i].value()) + "\n";
                }
                else
                {
                    continue;
                    //ret += "No value here\n";
                }
            }
        }

        return ret;
    }
};

struct full_intermediate_map
{
    std::vector<intermediate_source_line> lines;

    void decode_from(const std::string& in)
    {
        auto vals = no_ss_split(in, ";");

        intermediate_source_segment last;

        for(auto& i : vals)
        {
            intermediate_source_line line;
            line.decode_from_whole_line(last, i);

            lines.push_back(line);
        }
    }

    std::string dump()
    {
        std::string ret;

        for(auto& i : lines)
        {
            ret += i.dump() + "\n";
        }

        return ret;
    }
};

void source_map::decode(const std::string& code_in, const std::string& code_out, const std::string& json_obj)
{
    original_code = code_in;
    parsed_code = code_out;

    nlohmann::json obj = nlohmann::json::parse(json_obj);

    std::vector<std::string> names = obj["names"];

    std::string mappings = obj["mappings"];

    full_intermediate_map full_map;
    full_map.decode_from(mappings);

    intermediate_source_line last;

    for(intermediate_source_line& j : full_map.lines)
    {
        source_line line;

        for(intermediate_source_segment& i : j.segments)
        {
            source_segment seg;

            for(auto kk = 0; kk < 5; kk++)
            {
                if(!i.vals[kk].has_value())
                {
                    seg.vals[kk] = 0;
                }
                else
                {
                    seg.vals[kk] = i.vals[kk].value();
                }
            }

            line.segments.push_back(seg);
        }


        lines.push_back(line);
    }
}

source_position source_map::map(const source_position& pos)
{
    if(pos.line < 0)
        return pos;

    if(pos.line >= (int)lines.size())
        return pos;

    source_line& line = lines[pos.line];

    for(source_segment& seg : line.segments)
    {
        int generated_column = seg.vals[source_segment::column_gen];

        int original_line = seg.vals[source_segment::line_original];
        int original_column = seg.vals[source_segment::column_original];

        if(pos.column >= generated_column)
        {
            source_position new_pos;
            new_pos.column = (pos.column - generated_column) + original_column;
            new_pos.line = original_line;

            return new_pos;
        }
    }

    return source_position();
}

std::string source_map::get_caret_text_of(const source_position& pos)
{
    std::vector<std::string> by_line;

    std::string accum;

    for(int i=0; i < (int)original_code.size(); i++)
    {
        if(original_code[i] == '\n')
        {
            by_line.push_back(accum);
            accum.clear();
        }
        else
        {
            accum += original_code[i];
        }
    }

    if(accum != "")
        by_line.push_back(accum);

    std::vector<std::string> pre_contexts;
    std::vector<std::string> post_contexts;

    for(int i=-3; i < 4; i++)
    {
        int idx = i + pos.line;

        if(idx < 0 || idx >= (int)by_line.size())
            continue;

        if(i <= 0)
            pre_contexts.push_back(by_line[idx]);
        else
            post_contexts.push_back(by_line[idx]);
    }

    std::string line = "";

    for(auto& i : pre_contexts)
    {
        line += i + "\n";
    }

    if(line.size() > 0)
        line.pop_back();

    std::string post_line;

    for(auto& i : post_contexts)
    {
        post_line += i + "\n";
    }

    //std::string prepad = "Source: ";

    std::string prepad;

    std::string arrow;

    for(int i=0; i < pos.column + (int)prepad.size(); i++)
    {
        arrow += " ";
    }

    arrow += "^";

    std::string formatted_error = "Script Upload Error: Line " + std::to_string(pos.line + 1) + " column " + std::to_string(pos.column + 1) + "\n" +
                                  "Source:\n" + line + "\n" + arrow + "\n" + post_line;

    return formatted_error;
}

void source_map_init()
{
    get_val_of_base64('A');

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


    /*source_line decoded_line;
    source_segment seg;
    decoded_line.decode_from_whole_line(seg, "AAgBC,SAAQ,CAAEA");

    std::cout << decoded_line.dump();*/

    /*full_intermediate_map full_map;
    full_map.decode_from("AAgBC,SAAQ,CAAEA");

    std::cout << full_map.dump() << std::endl;*/

    //"sourceMapText":"{\"version\":3,\"file\":\"module.js\",\"sourceRoot\":\"\",\"sources\":[\"module.ts\"],\"names\":[],\"mappings\":\"AAAA,CAAC,UAAS,OAAO,EAAE,IAAI;IAAE,kBAAkB,OAAO,EAAE,IAAI;QAEpD,YAAY;QACZ,CAAC,EAAE,EAAE,EAAA,KAAK,CAAA,CAAA;QACV,MAAM,CAAC,aAAa,CAAA;IACxB,CAAC;IACD,MAAM,CAAC,QAAQ,CAAC,OAAO,EAAE,IAAI,CAAC,CAAC;AAAA,CAAC,CAAC,CAAC,OAAO,EAAE,IAAI,CAAC,CAAA\"}"

    std::string test_frag = "AAAA,CAAC,UAAS,OAAO,EAAE,IAAI;IAAE,kBAAkB,OAAO,EAAE,IAAI;QAEpD,YAAY;QACZ,CAAC,EAAE,EAAE,EAAA,KAAK,CAAA,CAAA;QACV,MAAM,CAAC,aAAa,CAAA;IACxB,CAAC;IACD,MAAM,CAAC,QAAQ,CAAC,OAAO,EAAE,IAAI,CAAC,CAAC;AAAA,CAAC,CAAC,CAAC,OAAO,EAAE,IAAI,CAAC,CAAA";

    full_intermediate_map full_map;
    full_map.decode_from(test_frag);

    //std::cout << full_map.dump() << std::endl;

    //exit(0);
}
