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

#ifndef UDEV_WRAPPER_H_INCLUDED
#define UDEV_WRAPPER_H_INCLUDED

#include <memory>
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

#endif /* UDEV-WRAPPER_H_INCLUDED */
