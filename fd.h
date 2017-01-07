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


#endif /* FD_H_INCLUDED */
