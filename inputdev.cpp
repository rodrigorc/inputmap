#include "inputdev.h"
#include <sys/epoll.h>

ValueId parse_value_id(const std::string &name)
{
    if (name == "REL_X")
        return ValueId(EV_REL, REL_X);
    if (name == "REL_Y")
        return ValueId(EV_REL, REL_Y);
    if (name == "BTN_LEFT")
        return ValueId(EV_KEY, BTN_LEFT);
    if (name == "BTN_RIGHT")
        return ValueId(EV_KEY, BTN_RIGHT);

    throw std::runtime_error("unknown value name " + name);
}

InputDevice::InputDevice(const IniSection &ini, FD fd)
    :m_fd(std::move(fd))
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

#define BIT(b) if (test_bit(b, (unsigned char*)buf)) printf(" %s",#b);
    test(ioctl(m_fd.get(), EVIOCGBIT(EV_REL, sizeof(buf)), buf), "EV_REL");
    printf("    rel=%02X-%02X-%02X: ", buf[0], buf[1], buf[2]);
    BIT(REL_X) BIT(REL_Y) BIT(REL_Z) BIT(REL_RX) BIT(REL_RY) BIT(REL_RZ) BIT(REL_HWHEEL) BIT(REL_DIAL) BIT(REL_WHEEL) BIT(REL_MISC)
        printf("\n");

    test(ioctl(m_fd.get(), EVIOCGBIT(EV_ABS, sizeof(buf)), buf), "EV_ABS");
    printf("    abs=%02X-%02X-%02X: ", buf[0], buf[1], buf[2]);
    BIT(ABS_X) BIT(ABS_Y) BIT(ABS_Z) BIT(ABS_RX) BIT(ABS_RY) BIT(ABS_RZ) BIT(ABS_THROTTLE) BIT(ABS_RUDDER) BIT(ABS_WHEEL) BIT(ABS_GAS)
        BIT(ABS_BRAKE) BIT(ABS_HAT0X) BIT(ABS_HAT0Y) BIT(ABS_HAT1X) BIT(ABS_HAT1Y) BIT(ABS_HAT2X) BIT(ABS_HAT2Y) BIT(ABS_HAT3X) BIT(ABS_HAT3Y)
        BIT(ABS_PRESSURE) BIT(ABS_DISTANCE) BIT(ABS_TILT_X) BIT(ABS_TILT_Y) BIT(ABS_TOOL_WIDTH) BIT(ABS_VOLUME) BIT(ABS_MISC)
        printf("\n");

    test(ioctl(m_fd.get(), EVIOCGBIT(EV_KEY, sizeof(buf)), buf), "EV_KEY");
    printf("    key=%02X-%02X-%02X: ", buf[0], buf[1], buf[2]);
    for (int k = 0; k < KEY_MAX; ++k)
        if (test_bit(k, (unsigned char*)buf)) printf(" 0x%X", k);
    printf("\n");
#undef BIT
}

PollResult InputDevice::on_poll(int event)
{
    input_event evs[32];
    if (event & EPOLLIN)
    {
        int res = read(m_fd.get(), &evs, sizeof(evs));
        if (res == -1)
        {
            if (errno == EINTR)
                return PollResult::None;
            perror("input read");
            throw std::runtime_error("input read");
        }

        int nr = res / sizeof(input_event);
        bool sync = false;
        for (int i = 0; i < nr; ++i)
        {
            sync = sync || on_input(evs[i]);
        }
        return sync ? PollResult::Sync : PollResult::None;
    }
    else if (event & EPOLLERR)
    {
        printf("closing fd\n");
        return PollResult::Error;
    }
    else
        return PollResult::None;
}

bool InputDevice::on_input(input_event &ev)
{
    InputStatus &st = m_status[!m_idx];
    switch (ev.type)
    {
    case EV_SYN:
        //printf("SYN %d %d\n", ev.code, ev.value);
        m_status[m_idx] = m_status[!m_idx];
        memset(&m_status[m_idx].rel, 0, sizeof(m_status[m_idx].rel));
        m_idx = !m_idx;
        //printf("REL %d %d\n",  m_status[m_idx].rel[REL_X], m_status[m_idx].rel[REL_Y]);
        return true;
    case EV_ABS:
        //printf("ABS %d %d\n", ev.code, ev.value);
        st.abs[ev.code] = ev.value;
        break;
    case EV_REL:
        //printf("REL %d %d\n", ev.code, ev.value);
        st.rel[ev.code] += ev.value;
        break;
    case EV_KEY:
        //printf("KEY %d %d\n", ev.code, ev.value);
        st.key[ev.code] = ev.value;
        break;
    case EV_MSC:
        //printf("MSC %d %d\n", ev.code, ev.value);
        break;
    default:
        //printf("??? %d %d %d\n", ev.type, ev.code, ev.value);
        break;
    }
    return false;
}

int InputDevice::get_value(const ValueId &id) const
{
    const InputStatus &st = m_status[m_idx];
    switch (id.type)
    {
    case EV_REL:
        return st.rel[id.code];
    case EV_KEY:
        return st.key[id.code];
    default:
        return 0;
    }
}

