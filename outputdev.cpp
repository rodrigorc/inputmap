#include "outputdev.h"
#include "inputdev.h"
#include <linux/uinput.h>
#include <algorithm>

OutputDevice::OutputDevice(const IniSection &ini, IInputByName &inputFinder)
{
    std::string name = ini.find_single_value("name");
    if (name.empty())
        name = "InputMap";
    std::string phys = ini.find_single_value("phys");
    if (phys.empty())
        phys = "InputMap";

    m_fd = FD_open("/dev/uinput", O_RDWR|O_CLOEXEC);
    uinput_setup us = {};
    us.id.bustype = BUS_VIRTUAL;
    us.id.version = 1;
    strcpy(us.name, name.c_str());
    test(ioctl(m_fd.get(), UI_DEV_SETUP, &us), "UI_DEV_SETUP");
    test(ioctl(m_fd.get(), UI_SET_PHYS, phys.c_str()), "UI_SET_PHYS");

    //test(ioctl(m_fd.get(), UI_SET_EVBIT, EV_ABS), "EV_ABS");
    //test(ioctl(m_fd.get(), UI_SET_EVBIT, EV_KEY), "EV_KEY");

    /*
    uinput_abs_setup abs = {};
    abs.absinfo.minimum = 0;
    abs.absinfo.maximum = 0xFF;
    abs.absinfo.value = 0x7F;
    abs.code = ABS_X;
    test(ioctl(m_fd.get(), UI_ABS_SETUP, &abs), "ABS_X");
    abs.code = ABS_Y;
    test(ioctl(m_fd.get(), UI_ABS_SETUP, &abs), "ABS_Y");

    test(ioctl(m_fd.get(), UI_SET_KEYBIT, BTN_TRIGGER), "BTN_TRIGGER");
    */

    m_rel[REL_X] = parse_ref(ini.find_single_value("REL_X"), inputFinder);
    m_rel[REL_Y] = parse_ref(ini.find_single_value("REL_Y"), inputFinder);

    bool has_rel = false;
    for (int i = 0; i < REL_CNT; ++i)
    {
        if (m_rel[i])
        {
            if (!has_rel)
            {
                test(ioctl(m_fd.get(), UI_SET_EVBIT, EV_REL), "EV_REL");
                has_rel = true;
            }
            test(ioctl(m_fd.get(), UI_SET_RELBIT, i), "UI_SET_RELBIT");
        }
    }

    m_key[BTN_LEFT] = parse_ref(ini.find_single_value("BTN_LEFT"), inputFinder);
    m_key[BTN_RIGHT] = parse_ref(ini.find_single_value("BTN_RIGHT"), inputFinder);

    bool has_key = false;
    for (int i = 0; i < KEY_CNT; ++i)
    {
        if (m_key[i])
        {
            if (!has_key)
            {
                test(ioctl(m_fd.get(), UI_SET_EVBIT, EV_KEY), "EV_KEY");
                has_key = true;
            }
            test(ioctl(m_fd.get(), UI_SET_KEYBIT, i), "UI_SET_KEYBIT");
        }
    }

    test(ioctl(m_fd.get(), UI_DEV_CREATE, 0), "UI_DEV_CREATE");
}

std::shared_ptr<InputDevice> OutputDevice::find_input(const std::string &name, IInputByName *inputFinder)
{
    auto it = std::find_if(m_inputs.begin(), m_inputs.end(), [&name](std::weak_ptr<InputDevice> &x)
            {
                auto p = x.lock();
                return p && p->name() == name;
            });
    if (it != m_inputs.end())
        return it->lock();

    std::shared_ptr<InputDevice> res;
    if (inputFinder)
    {
        res = inputFinder->find_input(name);
        if (res)
            m_inputs.push_back(res);
    }
    return res;
}

ValueRef OutputDevice::parse_ref(const std::string &desc, IInputByName &inputFinder)
{
    size_t dot = desc.find(".");
    if (dot == std::string::npos)
        throw std::runtime_error("invalid ref: " + desc);
    std::string sdev = desc.substr(0, dot);
    std::string saxis = desc.substr(dot + 1);
    auto dev = find_input(sdev, &inputFinder);
    auto value_id = parse_value_id(saxis);
    if (!dev)
        throw std::runtime_error("unknown device in ref: " + desc);
    return ValueRef{dev, value_id};
}

inline input_event create_event(int type, int code, int value)
{
    input_event ev;
    ev.type = type;
    ev.code = code;
    ev.value = value;
    return ev;
}

inline void do_event(std::vector<input_event> &evs, int type, int code, ValueRef &ref)
{
    if (ref)
        evs.push_back(create_event(type, code, ref.get_value()));
}

void OutputDevice::sync()
{
    std::vector<input_event> evs;

    for (int i = 0; i < REL_CNT; ++i)
        do_event(evs, EV_REL, i, m_rel[i]);
    for (int i = 0; i < KEY_CNT; ++i)
        do_event(evs, EV_KEY, i, m_key[i]);

    if (!evs.empty())
    {
        evs.push_back(create_event(EV_SYN, SYN_REPORT, 0));
        test(write(m_fd.get(), evs.data(), evs.size() * sizeof(input_event)), "write");
    }
}
