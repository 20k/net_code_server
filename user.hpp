#ifndef USER_HPP_INCLUDED
#define USER_HPP_INCLUDED

#include "mongo.hpp"
#include "script_util.hpp"

///how do handle upgrades
///vector of ids?
struct user
{
    std::string name;
    double gc = 0;

    bool construct_new_user(const std::string name_)
    {
        name = name_;

        if(!is_valid_string(name))
            return false;

        return true;
    }
};

#endif // USER_HPP_INCLUDED
