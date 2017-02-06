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

#include <sys/epoll.h>
#include "inputdev.h"
#include "event-codes.h"

InputDevice::InputDevice(const IniSection &ini)
{
    m_name = ini.find_single_value("name");
    if (m_name.empty())
        throw std::runtime_error("input without name");
}

InputDeviceEvent::InputDeviceEvent(const IniSection &ini, FD the_fd)
    :InputDevice(ini), m_fd(std::move(the_fd)), m_num_evs(0)
{
    bool grab = parse_bool(ini.find_single_value("grab"), false);

    if (grab)
        test(ioctl(fd(), EVIOCGRAB, 1), "EVIOCGRAB");

    char buf[1024] = "";
    input_id iid;
    if (ioctl(fd(), EVIOCGID, &iid))
        printf("    iid=%d %04x:%04x %d\n", iid.bustype, iid.vendor, iid.product, iid.version);
    if (ioctl(fd(), EVIOCGNAME(sizeof(buf)), buf) >= 0)
        printf("    name='%s'\n", buf);
    if (ioctl(fd(), EVIOCGPHYS(sizeof(buf)), buf) >= 0)
        printf("    phys='%s'\n", buf);
    if (ioctl(fd(), EVIOCGUNIQ(sizeof(buf)), buf) >=0)
        printf("    uniq='%s'\n", buf);
    if (ioctl(fd(), EVIOCGPROP(sizeof(buf)), buf) >= 0)
        printf("    prop='%s'\n", buf);

    test(ioctl(fd(), EVIOCGBIT(EV_REL, sizeof(buf)), buf), "EV_REL");
    printf("    rel: ");
    for (const auto &kv : g_rel_names)
    {
        if (!kv.name)
            continue;
        if (test_bit(kv.id, (unsigned char*)buf))
            printf(" %s", kv.name);
    }
    printf("\n");

    test(ioctl(fd(), EVIOCGBIT(EV_ABS, sizeof(buf)), buf), "EV_ABS");
    printf("    abs: ");
    for (const auto &kv : g_abs_names)
    {
        if (!kv.name)
            continue;
        if (test_bit(kv.id, (unsigned char*)buf))
            printf(" %s", kv.name);
    }
    printf("\n");

    test(ioctl(fd(), EVIOCGBIT(EV_KEY, sizeof(buf)), buf), "EV_KEY");
    printf("    key: ");
    for (const auto &kv : g_key_names)
    {
        if (!kv.name)
            continue;
        if (test_bit(kv.id, (unsigned char*)buf))
            printf(" %s", kv.name);
    }
    printf("\n");
}

ValueId InputDeviceEvent::parse_value(const std::string &name)
{
    for (const auto &kv : g_key_names)
    {
        if (kv.name && kv.name == name)
            return ValueId(EV_KEY, kv.id);
    }
    for (const auto &kv : g_rel_names)
    {
        if (kv.name && kv.name == name)
            return ValueId(EV_REL, kv.id);
    }
    for (const auto &kv : g_abs_names)
    {
        if (kv.name && kv.name == name)
            return ValueId(EV_ABS, kv.id);
    }
    for (const auto &kv : g_ff_names)
    {
        if (kv.name && kv.name == name)
            return ValueId(EV_FF, kv.id);
    }
    throw std::runtime_error("unknown value name " + name);
}

PollResult InputDeviceEvent::on_poll(int event)
{
    if ((event & EPOLLIN) == 0)
        return PollResult::None;

    int free_evs = countof(m_evs) - m_num_evs;
    if (free_evs == 0)
        throw std::runtime_error("input buffer overflow");
    int res = read(fd(), &m_evs[m_num_evs], free_evs * sizeof(input_event));
    if (res == -1)
    {
        if (errno == EINTR)
            return PollResult::None;
        perror("input read");
        return PollResult::Error;
    }

    m_num_evs += res / sizeof(input_event);
    if (m_evs[m_num_evs - 1].type != EV_SYN)
    {
        printf("no EV_SYN, buffering\n");
        return PollResult::None;
    }

    for (int i = 0; i < m_num_evs; ++i)
        on_input(m_evs[i]);
    m_num_evs = 0;
    return PollResult::Sync;
}

void InputDeviceEvent::on_input(input_event &ev)
{
    switch (ev.type)
    {
    case EV_SYN:
        //printf("SYN %d %d\n", ev.code, ev.value);
        break;
    case EV_ABS:
        //printf("ABS %d %d\n", ev.code, ev.value);
        m_status.abs[ev.code] = ev.value;
        break;
    case EV_REL:
        //printf("REL %d %d\n", ev.code, ev.value);
        m_status.rel[ev.code] += ev.value;
        break;
    case EV_KEY:
        //printf("KEY %d %d\n", ev.code, ev.value);
        m_status.key[ev.code] = ev.value;
        break;
    case EV_MSC:
        //printf("MSC %d %d\n", ev.code, ev.value);
        break;
    default:
        //printf("??? %d %d %d\n", ev.type, ev.code, ev.value);
        break;
    }
}

int InputDeviceEvent::get_value(const ValueId &id)
{
    switch (id.type)
    {
    case EV_REL:
        return m_status.rel[id.code];
    case EV_KEY:
        return m_status.key[id.code];
    case EV_ABS:
        return m_status.abs[id.code];
    default:
        return 0;
    }
}

input_absinfo InputDeviceEvent::get_absinfo(int code)
{
    input_absinfo res;
    test(ioctl(fd(), EVIOCGABS(code), &res), "EVIOCGABS");
    return res;
}

void InputDeviceEvent::flush()
{
    memset(m_status.rel, 0, sizeof(m_status.rel));
}

int InputDeviceEvent::ff_upload(const ff_effect &eff)
{
    ff_effect ff = eff;
    ff.id = -1;
    int res = ioctl(fd(), EVIOCSFF, &ff);
    if (res < 0)
        return -errno;
    return ff.id;
}

int InputDeviceEvent::ff_erase(int id)
{
    int res = ioctl(fd(), EVIOCRMFF, id);
    if (res < 0)
        return -errno;
    return 0;
}

void InputDeviceEvent::ff_run(int eff, bool on)
{
    input_event ev{};
    ev.type = EV_FF;
    ev.code = eff;
    ev.value = on? 1 : 0;
    test(write(fd(), &ev, sizeof(ev)), "write ff");
}

std::shared_ptr<InputDevice> InputDeviceEventCreate(const IniSection &ini, const std::string &id)
{
    std::string dev = "/dev/input/by-id/" + id;
    FD fd { FD_open(dev.c_str(), O_RDWR|O_CLOEXEC) };
    return std::make_shared<InputDeviceEvent>(ini, std::move(fd));
}

