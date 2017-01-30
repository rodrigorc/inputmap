/*

Copyright 2017, Rodrigo Rivas Costa <rodrigorivascosta@gmail.com>

This file is part of inputmap.

inputmap is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

inputmap is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with inputmap.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef FD_H_INCLUDED
#define FD_H_INCLUDED

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "unique_handle.h"

struct FileCloser
{
    typedef UniqueHandle<int, -1> pointer;
    void operator()(pointer p) noexcept
    {
        close(p);
    }
};
typedef std::unique_ptr<int, FileCloser> FD;

inline void test(int res, const char *txt)
{
    if (res < 0)
    {
        int e = errno;
        std::string msg = txt;
        msg += ": ";
        msg += strerror(e);
        throw std::runtime_error(msg);
    }
}

inline FD FD_open(const char *name, int flags)
{
    int fd = open(name, flags);
    test(fd, name);
    return FD(fd);
}

inline bool test_bit(int bit, const unsigned char *ptr)
{
    return (ptr[bit >> 3] >> (bit & 7)) & 1;
}

template <size_t N, typename T>
constexpr size_t countof(T (&a)[N])
{
    return N;
}

#endif /* FD_H_INCLUDED */
