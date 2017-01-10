#include <sys/epoll.h>
#include "inputdev.h"
#include "event-codes.h"

InputDevice::InputDevice(const IniSection &ini, FD fd)
    :m_fd(std::move(fd))
{
    m_name = ini.find_single_value("name");
    if (m_name.empty())
        throw std::runtime_error("input without name");
}

InputDeviceEvent::InputDeviceEvent(const IniSection &ini, FD the_fd)
    :InputDevice(ini, std::move(the_fd)), m_num_evs(0)
{
    bool grab = parse_bool(ini.find_single_value("grab"), false);

    if (grab)
        test(ioctl(fd(), EVIOCGRAB, 1), "EVIOCGRAB");

    char buf[1024] = "";
    input_id iid;
    test(ioctl(fd(), EVIOCGID, &iid), "EVIOCGID");
    printf("    iid=%d %04x:%04x %d\n", iid.bustype, iid.vendor, iid.product, iid.version);
    test(ioctl(fd(), EVIOCGNAME(sizeof(buf)), buf), "EVIOCGNAME");
    printf("    name='%s'\n", buf);
    test(ioctl(fd(), EVIOCGPHYS(sizeof(buf)), buf), "EVIOCGPHYS");
    printf("    phys='%s'\n", buf);
    test(ioctl(fd(), EVIOCGUNIQ(sizeof(buf)), buf), "EVIOCGUNIQ");
    printf("    uniq='%s'\n", buf);
    test(ioctl(fd(), EVIOCGPROP(sizeof(buf)), buf), "EVIOCGPROP");
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

std::shared_ptr<InputDevice> InputDeviceEventCreate(const IniSection &ini, const std::string &id)
{
    std::string dev = "/dev/input/by-id/" + id;
    FD fd { FD_open(dev.c_str(), O_RDWR|O_CLOEXEC) };
    return std::make_shared<InputDeviceEvent>(ini, std::move(fd));
}

