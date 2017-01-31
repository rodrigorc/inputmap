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
#include <algorithm>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/epoll.h>

#include "inifile.h"
#include "inputsteam.h"
#include "outputdev.h"
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
    InputFinder(IT begin, IT end)
        :m_begin(begin), m_end(end)
    {
    }
    virtual std::shared_ptr<InputDevice> find_input(const std::string &name)
    {
        auto it = std::find_if(m_begin, m_end, [&name](std::shared_ptr<InputDevice> &x) { return x->name() == name; });
        if (it != m_end)
            return *it;
        return std::shared_ptr<InputDevice>();
    }
private:
    IT m_begin, m_end;
};

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

    for (auto &s : ini.find_multi_section("input"))
    {
        std::string id = s->find_single_value("ID");
        printf("id='%s'\n", id.c_str());

        if (id == "steam" || id == "Steam" || id == "STEAM")
        {
            auto dev = std::make_shared<InputDeviceSteam>(*s);
            inputs.push_back(dev);
        }
        else
        {
            auto dev = InputDeviceEventCreate(*s, id);
            inputs.push_back(dev);
        }
    }

    InputFinder<decltype(inputs.begin())> inputFinder(inputs.begin(), inputs.end());

    for (auto &s : ini.find_multi_section("output"))
    {
        std::string id = s->find_single_value("name");
        printf("name='%s'\n", id.c_str());
        outputs.emplace_back(*s, inputFinder);
    }

    if (inputs.empty())
    {
        fprintf(stderr, "no inputs");
        exit(EXIT_FAILURE);
    }
    if (outputs.empty())
    {
        fprintf(stderr, "no outputs");
        exit(EXIT_FAILURE);
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
            inputs.remove(d);

        if (!synced.empty())
        {
            for (auto &d : outputs)
                d.sync();
            for (auto &d : synced)
                d->flush();
        }

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
