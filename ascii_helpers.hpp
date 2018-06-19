#ifndef ASCII_HELPERS_HPP_INCLUDED
#define ASCII_HELPERS_HPP_INCLUDED

#include <vec/vec.hpp>

std::vector<std::vector<std::string>> ascii_make_buffer(vec2i dim);
std::string ascii_stringify_buffer(const std::vector<std::vector<std::string>>& buffer);

void ascii_draw_line(vec2f start, vec2f finish, const std::string& colour, std::vector<std::vector<std::string>>& buffer_out);

#endif // ASCII_HELPERS_HPP_INCLUDED
