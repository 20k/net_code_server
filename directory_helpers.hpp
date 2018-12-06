#ifndef DIRECTORY_HELPERS_HPP_INCLUDED
#define DIRECTORY_HELPERS_HPP_INCLUDED

#include <tinydir/tinydir.h>

struct tinydir_autoclose
{
    tinydir_dir dir;

    tinydir_autoclose(const std::string& directory)
    {
        tinydir_open(&dir, directory.c_str());
    }

    ~tinydir_autoclose()
    {
        tinydir_close(&dir);
    }
};

#endif // DIRECTORY_HELPERS_HPP_INCLUDED
