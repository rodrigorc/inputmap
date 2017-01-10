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
