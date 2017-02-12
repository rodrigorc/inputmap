/*

Copyright 2017, Rodrigo Rivas Costa <rodrigorivascosta@gmail.com>

This file is part of inputmap.

inputmap is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

inputmap is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with inputmap.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef UDEV_WRAPPER_H_INCLUDED
#define UDEV_WRAPPER_H_INCLUDED

#include <memory>
#include <vector>
#include <string>
#include <libudev.h>

struct udev_closer
{
    void operator()(udev *p) noexcept { udev_unref(p); }
};
using udev_ptr = std::unique_ptr<udev, udev_closer>;
struct udev_device_closer
{
    void operator()(udev_device *p) noexcept { udev_device_unref(p); }
};
using udev_device_ptr = std::unique_ptr<udev_device, udev_device_closer>;
struct udev_enumerate_closer
{
    void operator()(udev_enumerate *p) noexcept { udev_enumerate_unref(p); }
};
using udev_enumerate_ptr = std::unique_ptr<udev_enumerate, udev_enumerate_closer>;

static inline std::vector<std::string> find_udev_devices(udev *ud, udev_device *parent, const char *subsystem, 
        const char *attr, const char *value,
        const char *attr2 = nullptr, const char *value2 = nullptr
        )
{
    std::vector<std::string> res;
    udev_enumerate_ptr ude { udev_enumerate_new(ud) };
    if (subsystem)
        udev_enumerate_add_match_subsystem(ude.get(), subsystem);
    if (attr)
        udev_enumerate_add_match_sysattr(ude.get(), attr, value);
    if (attr2)
        udev_enumerate_add_match_sysattr(ude.get(), attr2, value2);
    if (parent)
        udev_enumerate_add_match_parent(ude.get(), parent);
    udev_enumerate_scan_devices(ude.get());
    for (udev_list_entry *le = udev_enumerate_get_list_entry(ude.get()); le; le = udev_list_entry_get_next(le))
    {
        const char *name = udev_list_entry_get_name(le);
        res.push_back(name);
    }
    return res;
}


#endif /* UDEV-WRAPPER_H_INCLUDED */
