#include <string>
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
#include "steamcontroller.h"

void help(const char *name)
{
    fprintf(stderr, "Usage %s file.ini\n", name);
    exit(EXIT_FAILURE);
}


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
        ev.data.ptr = input.get();
        test(epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD, input->fd(), &ev), "EPOLL_CTL_ADD");
    }

    for (;;)
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
            auto input = static_cast<InputDevice*>(ev.data.ptr);
            if (ev.events & EPOLLERR)
            {
                deletes.push_back(input->shared_from_this());
                continue;
            }
            auto res = input->on_poll(ev.events);
            switch (res)
            {
            case PollResult::None:
                break;
            case PollResult::Error:
                deletes.push_back(input->shared_from_this());
                break;
            case PollResult::Sync:
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
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
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
