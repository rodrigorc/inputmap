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
#include <fstream>
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


bool g_verbose = false;
bool g_daemonize = false;
const char *g_writepid;

void help(const char *name)
{
    printf("Usage %s <options> file.ini\n\n", name);
    printf("Available options:\n");
    printf("\t-v: Verbose output.\n");
    printf("\t-d: Run in background (daemonize) when all output devices have been created.\n");
    printf("\t-p <filename>: Write the PID into the given file. Useful to kill the program later.\n");
    printf("\t-m <key>=<value>: Define a macro to be replaced in the ini file: {<key>} will be replaced with <value>.\n");
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

struct
{
    const char *name;
    int id;
} g_buses[] =
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

const char *bus_name(int bus_id)
{
    for (auto &bus: g_buses)
    {
        if (bus.id == bus_id)
            return bus.name;
    }
    return "(null)";
}

int bus_id(const char *bus_name)
{
    for (auto &bus: g_buses)
    {
        if (strcasecmp(bus.name, bus_name) == 0)
            return bus.id;
    }
    throw std::runtime_error(std::string("unknown bus name: ") + bus_name);
}

struct FoundInputDevice
{
    FD fd;
    std::string dev, name, uniq;
    input_id iid;
};

std::vector<FoundInputDevice> list_input_devices()
{
    std::vector<FoundInputDevice> res;
    udev_ptr ud { udev_new() };
    auto udevs = find_udev_devices(ud.get(), nullptr, "input", nullptr, nullptr);
    if (g_verbose)
        printf("Looking for input devices...\n");
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

        FD fd { open(dev, O_RDONLY) };
        if (!fd)
        {
            if (g_verbose)
                fprintf(stderr, "%s: %s\n", dev, strerror(errno));
            continue;
        }

        FoundInputDevice fid;
        fid.dev = dev;
        if (ioctl(fd.get(), EVIOCGID, &fid.iid) < 0)
        {
            if (g_verbose)
                fprintf(stderr, "%s: %s\n", dev, strerror(errno));
            continue;
        }

        char buf[1024];
        //do not fail if the device has no name
        if (ioctl(fd.get(), EVIOCGNAME(sizeof(buf)), buf) >= 0)
            fid.name = trim(buf);
        if (ioctl(fd.get(), EVIOCGUNIQ(sizeof(buf)), buf) >= 0)
            fid.uniq = trim(buf);

        if (g_verbose)
        {
            std::string extra;
            if (!fid.uniq.empty())
                extra = " : <" + fid.uniq + ">";
            printf("%04x:%04x %5s %s '%s'%s\n", fid.iid.vendor, fid.iid.product, bus_name(fid.iid.bustype), dev, fid.name.c_str(), extra.c_str());
        }
        fid.fd = std::move(fd);
        res.push_back(std::move(fid));
    }
    return res;
}

FoundInputDevice *find_input_device(std::vector<FoundInputDevice> &fids, int bus, const std::string &vp)
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

FD find_input_device_from_section(std::vector<FoundInputDevice> &fids, const IniSection *s)
{
    std::string sdev = s->find_single_value("dev");
    if (!sdev.empty())
        return FD { open(sdev.c_str(), O_RDONLY) };

    std::string sbyid = s->find_single_value("by-id");
    if (!sbyid.empty())
        return FD { open(("/dev/input/by-id/" + sbyid).c_str(), O_RDONLY) };

    std::string sbypath = s->find_single_value("by-path");
    if (!sbypath.empty())
        return FD { open(("/dev/input/by-path/" + sbypath).c_str(), O_RDONLY) };

    std::string sbyuniq = s->find_single_value("by-uniq");
    if (!sbyuniq.empty())
    {
        for (auto &fid: fids)
        {
            if (fid.uniq == sbyuniq)
                return std::move(fid.fd);
        }
        throw std::runtime_error("uniq device '" + sbyuniq + "' not found");
    }

    std::string sbyname = s->find_single_value("by-name");
    if (!sbyname.empty())
    {
        for (auto &fid: fids)
        {
            if (fid.name == sbyname)
                return std::move(fid.fd);
        }
        throw std::runtime_error("name device '" + sbyname + "' not found");
    }

    for (const auto &bus : g_buses)
    {
        std::string sbus = s->find_single_value(bus.name);
        if (!sbus.empty())
        {
            FoundInputDevice *fid = find_input_device(fids, bus.id, sbus);
            return std::move(fid->fd);
        }
    }

    throw std::runtime_error("input section without device: " + s->name());
}

int main2(int argc, char **argv)
{
    int opt;
    std::map<std::string, std::string> defines;
    while ((opt = getopt(argc, argv, "vdp:m:")) != -1)
    {
        switch (opt)
        {
        case 'v':
            g_verbose = true;
            break;
        case 'd':
            g_daemonize = true;
            break;
        case 'p':
            g_writepid = optarg;
            break;
        case 'm':
            {
                std::string arg(optarg);
                auto eq = arg.find('=');
                if (eq == std::string::npos)
                {
                    fprintf(stderr, "%s error: argument to '-a' must have an '='\n", argv[0]);
                    exit(1);
                }
                std::string name = arg.substr(0, eq);
                std::string value = arg.substr(eq + 1);
                defines[name] = value;
            }
            break;

        default:
            help(argv[0]);
        }
    }

    if (optind + 1 != argc)
    {
        //if run with -v but without a .ini file, dump the device list instead of the help
        if (g_verbose)
        {
            list_input_devices();
            exit(1);
        }
        help(argv[0]);
    }

    std::string name = argv[optind];
    IniFile ini(name);
    if (!defines.empty())
    {
        ini.preprocess_values([&defines](std::string v)
        {
            auto vv = v;
            size_t start, end;
            start = v.find('{');
            while (start != std::string::npos)
            {
                end = v.find('}', start + 1);
                if (end == std::string::npos)
                    break;
                std::string name = v.substr(start + 1, end - start - 1);
                auto it = defines.find(name);
                if (it == defines.end())
                {
                    fprintf(stderr, "Warning: macro '%s' used but not defined\n", name.c_str());
                }
                else
                {
                    v = v.substr(0, start) + it->second + v.substr(end + 1);
                    end += it->second.size() - name.size() - 2; //2 for the '{' and '}'
                }
                start = v.find('{', end + 1);
            }
            return v;
        });
    }
    //ini.Dump(std::cout);

    std::list<std::shared_ptr<InputDevice>> inputs;
    std::list<OutputDevice> outputs;

    //NOTE: close() an input device may take quite some time, so closing the full list of devices
    //may add up to 1 second (that is 1000 ms!). Not a big deal unless you are waiting for the program
    //to start. The important thing anyone may be waiting for is the creation of the output devices, so we
    //delay the closing of the input devices until the output devices are created.
    std::vector<FoundInputDevice> fids = list_input_devices();

    for (auto &s : ini.find_multi_section("steam"))
    {
        auto dev = std::make_shared<InputDeviceSteam>(*s);
        inputs.push_back(dev);
    }
    for (auto &s : ini.find_multi_section("input"))
    {
        FD fd = find_input_device_from_section(fids, s);
        if (!fd)
            throw std::runtime_error("input device alreay in use: " + s->find_single_value("name"));
        //printf("dev='%s'\n", dev.name.c_str());
        auto dev = InputDeviceEventCreate(*s, std::move(fd));
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

    //Output devices are already created so now we can close the unused input devices (see note above).
    fids.clear();

    if (inputs.empty())
    {
        fprintf(stderr, "warning: no inputs");
    }
    if (outputs.empty())
    {
        fprintf(stderr, "warning: no outputs");
    }

    if (g_daemonize)
    {
        if (daemon(/*nochdir*/1, /*noclose*/1) < 0)
            perror("daemon");
    }
    if (g_writepid)
    {
        std::ofstream ofs(g_writepid);
        ofs << getpid();
    }

    FD epoll_fd { epoll_create1(0) };

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
        int res = epoll_wait(epoll_fd.get(), epoll_evs, countof(epoll_evs), -1);
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
        {
            inputs.remove(d);
            g_exit = true;
        }

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
