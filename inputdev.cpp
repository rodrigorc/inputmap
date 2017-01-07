#include <sys/epoll.h>
#include "inputdev.h"
#include "event-codes.h"

ValueId parse_value_id(const std::string &name)
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
    throw std::runtime_error("unknown value name " + name);
}

InputDevice::InputDevice(const IniSection &ini, FD fd)
    :m_fd(std::move(fd)), m_num_evs(0)
{
    m_name = ini.find_single_value("name");
    if (m_name.empty())
        throw std::runtime_error("input without name");
    bool grab = parse_bool(ini.find_single_value("grab"), false);

    if (grab)
        test(ioctl(m_fd.get(), EVIOCGRAB, 1), "EVIOCGRAB");

    char buf[1024] = "";
    input_id iid;
    test(ioctl(m_fd.get(), EVIOCGID, &iid), "EVIOCGID");
    printf("    iid=%d %04x:%04x %d\n", iid.bustype, iid.vendor, iid.product, iid.version);
    test(ioctl(m_fd.get(), EVIOCGNAME(sizeof(buf)), buf), "EVIOCGNAME"); 
    printf("    name='%s'\n", buf);
    test(ioctl(m_fd.get(), EVIOCGPHYS(sizeof(buf)), buf), "EVIOCGPHYS"); 
    printf("    phys='%s'\n", buf);
    test(ioctl(m_fd.get(), EVIOCGUNIQ(sizeof(buf)), buf), "EVIOCGUNIQ"); 
    printf("    uniq='%s'\n", buf);
    test(ioctl(m_fd.get(), EVIOCGPROP(sizeof(buf)), buf), "EVIOCGPROP"); 
    printf("    prop='%s'\n", buf);

    test(ioctl(m_fd.get(), EVIOCGBIT(EV_REL, sizeof(buf)), buf), "EV_REL");
    printf("    rel: ");
    for (const auto &kv : g_rel_names)
    {
        if (!kv.name)
            continue;
        if (test_bit(kv.id, (unsigned char*)buf))
            printf(" %s", kv.name);
    }
    printf("\n");

    test(ioctl(m_fd.get(), EVIOCGBIT(EV_ABS, sizeof(buf)), buf), "EV_ABS");
    printf("    abs: ");
    for (const auto &kv : g_abs_names)
    {
        if (!kv.name)
            continue;
        if (test_bit(kv.id, (unsigned char*)buf))
            printf(" %s", kv.name);
    }
    printf("\n");

    test(ioctl(m_fd.get(), EVIOCGBIT(EV_KEY, sizeof(buf)), buf), "EV_KEY");
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

PollResult InputDevice::on_poll(int event)
{
    if (event & EPOLLIN)
    {
        int free_evs = countof(m_evs) - m_num_evs;
        if (free_evs == 0)
            throw std::runtime_error("input buffer overflow");
        int res = read(m_fd.get(), &m_evs[m_num_evs], free_evs * sizeof(input_event));
        if (res == -1)
        {
            if (errno == EINTR)
                return PollResult::None;
            perror("input read");
            throw std::runtime_error("input read");
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
    else if (event & EPOLLERR)
    {
        printf("closing fd\n");
        return PollResult::Error;
    }
    else
        return PollResult::None;
}

void InputDevice::on_input(input_event &ev)
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

int InputDevice::get_value(const ValueId &id) const
{
    switch (id.type)
    {
    case EV_REL:
        return m_status.rel[id.code];
    case EV_KEY:
        return m_status.key[id.code];
    default:
        return 0;
    }
}

void InputDevice::flush()
{
    memset(m_status.rel, 0, sizeof(m_status.rel));
}
