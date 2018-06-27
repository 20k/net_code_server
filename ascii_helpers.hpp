#ifndef ASCII_HELPERS_HPP_INCLUDED
#define ASCII_HELPERS_HPP_INCLUDED

#include <vec/vec.hpp>

std::vector<std::vector<std::string>> ascii_make_buffer(vec2i dim, bool add_newlines = true);
std::string ascii_stringify_buffer(const std::vector<std::vector<std::string>>& buffer);

void ascii_draw_line(vec2f start, vec2f finish, const std::string& colour, std::vector<std::vector<std::string>>& buffer_out);

std::string ascii_index_to_character(int idx);

std::string stolower(const std::string& in);

#endif // ASCII_HELPERS_HPP_INCLUDED
