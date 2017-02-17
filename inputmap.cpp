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

#include <string>
#include <signal.h>
#include <iostream>
#include <list>
#include <map>
#include <algorithm>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/epoll.h>
#include <pwd.h>

#include "inifile.h"
#include "inputsteam.h"
#include "outputdev.h"
#include "steam/udev-wrapper.h"
#include "steam/fd.h"
#include "steam/steamcontroller.h"

void help(const char *name)
{
    fprintf(stderr, "Usage %s file.ini\n", name);
    exit(EXIT_FAILURE);
}

volatile bool g_exit = false;

template<typename IT>
class InputFinder : public IInputByName
{
public:
    InputFinder(IT begin, IT end, std::map<std::string, Variable> &variables)
        :m_begin(begin), m_end(end), m_variables(variables)
    {
    }
    std::shared_ptr<InputDevice> find_input(const std::string &name) override
    {
        auto it = std::find_if(m_begin, m_end, [&name](std::shared_ptr<InputDevice> &x) { return x->name() == name; });
        if (it != m_end)
            return *it;
        return std::shared_ptr<InputDevice>();
    }
    Variable *find_variable(const std::string &name) override
    {
        auto it = m_variables.find(name);
        if (it == m_variables.end())
            return nullptr;
        return &it->second;
    }
private:
    IT m_begin, m_end;
    std::map<std::string, Variable> &m_variables;
};

struct FoundInputDevice
{
    std::string dev;
    input_id iid;
};

std::vector<FoundInputDevice> list_input_devices()
{
    std::vector<FoundInputDevice> res;
    udev_ptr ud { udev_new() };
    auto udevs = find_udev_devices(ud.get(), nullptr, "input", nullptr, nullptr);
    for (auto udev : udevs)
    {
        udev_device_ptr dx { udev_device_new_from_syspath(ud.get(), udev.c_str()) };
        const char *dev = udev_device_get_devnode(dx.get());

        if (!dev)
            continue;
        auto slash = udev.rfind('/');
        if (slash == std::string::npos)
            continue;
        if (udev.substr(slash + 1, 5) != "event")
            continue;

        FD fd { open(dev, O_RDONLY|O_CLOEXEC) };
        if (!fd)
            continue;

        FoundInputDevice fid;
        fid.dev = dev;
        if (ioctl(fd.get(), EVIOCGID, &fid.iid) < 0)
            continue;

        res.push_back(fid);
    }
    return res;
}

const FoundInputDevice *find_input_device(const std::vector<FoundInputDevice> &fids, int bus, const std::string &vp)
{
    auto colon = vp.find(':');
    if (colon == std::string::npos)
        throw std::runtime_error("invalid vendor:product pair: " + vp);

    int vendor = parse_hex_int(vp.substr(0, colon), -1);
    int product = parse_hex_int(vp.substr(colon + 1), -1);
    if (vendor == -1 || product == -1)
        throw std::runtime_error("invalid vendor:product pair: " + vp);

    for (auto &fid: fids)
    {
        if (bus == fid.iid.bustype && vendor == fid.iid.vendor && product == fid.iid.product)
            return &fid;
    }
    throw std::runtime_error("device " + vp + " not found");
}

std::string find_input_device_from_section(const std::vector<FoundInputDevice> &fids, const IniSection *s)
{
    std::string sdev = s->find_single_value("dev");
    if (!sdev.empty())
        return sdev;

    std::string sbyid = s->find_single_value("by-id");
    if (!sbyid.empty())
        return "/dev/input/by-id/" + sbyid;

    std::string sbypath = s->find_single_value("by-path");
    if (!sbypath.empty())
        return "/dev/input/by-path/" + sbypath;

    struct
    {
        const char *name;
        int id;
    } buses[] =
    {
        {"usb", BUS_USB},
        {"pci", BUS_PCI},
        {"isapnp", BUS_ISAPNP},
        {"usb", BUS_USB},
        {"hil", BUS_HIL},
        {"bluetooth", BUS_BLUETOOTH},
        {"virtual", BUS_VIRTUAL},
        {"isa", BUS_ISA},
        {"i8042", BUS_I8042},
        {"xtkbd", BUS_XTKBD},
        {"rs232", BUS_RS232},
        {"gameport", BUS_GAMEPORT},
        {"parport", BUS_PARPORT},
        {"amiga", BUS_AMIGA},
        {"adb", BUS_ADB},
        {"i2c", BUS_I2C},
        {"host", BUS_HOST},
        {"gsc", BUS_GSC},
        {"atari", BUS_ATARI},
        {"spi", BUS_SPI},
        {"rmi", BUS_RMI},
        {"cec", BUS_CEC},
    };
    for (const auto &bus : buses)
    {
        std::string sbus = s->find_single_value(bus.name);
        if (!sbus.empty())
        {
            const FoundInputDevice *fid = find_input_device(fids, bus.id, sbus);
            return fid->dev;
        }
    }

    throw std::runtime_error("input section without device: " + s->name());
}

int main2(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "")) != -1)
    {
        switch (opt)
        {
        default:
            help(argv[0]);
        }
    }

    if (optind + 1!= argc)
        help(argv[0]);

    std::string name = argv[optind];
    IniFile ini(name);
    //ini.Dump(std::cout);

    std::list<std::shared_ptr<InputDevice>> inputs;
    std::list<OutputDevice> outputs;

    std::vector<FoundInputDevice> fids = list_input_devices();

    for (auto &s : ini.find_multi_section("steam"))
    {
        auto dev = std::make_shared<InputDeviceSteam>(*s);
        inputs.push_back(dev);
    }
    for (auto &s : ini.find_multi_section("input"))
    {
        std::string devname = find_input_device_from_section(fids, s);
        printf("dev='%s'\n", devname.c_str());
        auto dev = InputDeviceEventCreate(*s, devname);
        inputs.push_back(dev);
    }

    std::map<std::string, Variable> variables;

    InputFinder<decltype(inputs.begin())> inputFinder(inputs.begin(), inputs.end(), variables);

    if (const IniSection *vars = ini.find_single_section("variables"))
    {
        for (auto &entry : *vars)
        {
            std::unique_ptr<ValueExpr> exp = parse_ref(entry.value(), inputFinder);
            variables.emplace(entry.name(), Variable(std::move(exp)));
        }
    }

    for (auto &s : ini.find_multi_section("output"))
    {
        std::string id = s->find_single_value("name");
        printf("name='%s'\n", id.c_str());
        outputs.emplace_back(*s, inputFinder);
    }

    if (inputs.empty())
    {
        fprintf(stderr, "warning: no inputs");
    }
    if (outputs.empty())
    {
        fprintf(stderr, "warning: no outputs");
    }

    FD epoll_fd { epoll_create1(O_CLOEXEC) };

    for (auto &input : inputs)
    {
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.ptr = static_cast<IPollable*>(input.get());
        test(epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, input->fd(), &ev), "EPOLL_CTL_ADD");
    }
    for (auto &output : outputs)
    {
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.ptr = static_cast<IPollable*>(&output);
        test(epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, output.fd(), &ev), "EPOLL_CTL_ADD");
    }

    nice(-10);

    //Drop privileges if run as a root set-user-id
    setgid(getgid());
    setuid(getuid());
    if (getuid() == 0) //still root? maybe used sudo or su
    {                  //then drop to nobody, if possible
        char buf[1024];
        passwd pwd, *nobody;
        getpwnam_r("nobody", &pwd, buf, sizeof(buf), &nobody);
        if (nobody)
        {
            setgid(nobody->pw_gid);
            setuid(nobody->pw_uid);
        }
        else
        {
            fprintf(stderr, "Warning! nobody user not found, still running as root\n");
        }
    }

    while (!g_exit)
    {
        epoll_event epoll_evs[1];
        int res = epoll_wait(epoll_fd.get(), epoll_evs, countof(epoll_evs), 10); //TODO conf timeout
        if (res == -1)
        {
            if (errno == EINTR)
                continue;
            perror("epoll");
            exit(EXIT_FAILURE);
        }

        std::vector<std::shared_ptr<InputDevice>> deletes, synced;
        for (int i = 0; i < res; ++i)
        {
            epoll_event &ev = epoll_evs[i];
            auto pollable = static_cast<IPollable*>(ev.data.ptr);
            if (ev.events & EPOLLERR)
            {
                if (auto input = dynamic_cast<InputDevice*>(pollable))
                    deletes.push_back(input->shared_from_this());
                continue;
            }
            auto res = pollable->on_poll(ev.events);
            switch (res)
            {
            case PollResult::None:
                break;
            case PollResult::Error:
                if (auto input = dynamic_cast<InputDevice*>(pollable))
                    deletes.push_back(input->shared_from_this());
                break;
            case PollResult::Sync:
                if (auto input = dynamic_cast<InputDevice*>(pollable))
                    synced.push_back(input->shared_from_this());
                break;
            }
        }
        for (auto &d : deletes)
            inputs.remove(d);

        for (auto &v : variables)
            v.second.evaluate();

        for (auto &d : outputs)
            d.sync();
        for (auto &d : synced)
            d->flush();
    }
    printf("Exiting...\n");
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    struct sigaction sac {};
    sac.sa_handler = [](int signo) { g_exit = true; };
    sigaction(SIGINT, &sac, nullptr);
    sigaction(SIGHUP, &sac, nullptr);
    sigaction(SIGTERM, &sac, nullptr);

    try
    {
        return main2(argc, argv);
    }
    catch (std::exception &e)
    {
        fprintf(stderr, "\n *** Fatal error: %s\n", e.what());
        return EXIT_FAILURE;
    }
}
