#ifndef OUTPUTDEV_H_INCLUDED
#define OUTPUTDEV_H_INCLUDED

#include "fd.h"
#include "inifile.h"
#include "inputdev.h"

struct IInputByName
{
    virtual ~IInputByName() {}
    virtual std::shared_ptr<InputDevice> find_input(const std::string &name) =0;
};

struct ValueRef
{
    std::weak_ptr<InputDevice> device;
    ValueId value_id;
    explicit operator bool() const
    {
        return value_id.type != 0;
    }
    int get_value() const
    {
        auto dev = device.lock();
        if (!dev)
            return 0;
        return dev->get_value(value_id);
    }
};

class OutputDevice
{
public:
    OutputDevice(const IniSection &ini, IInputByName &inputFinder);
    void sync();
private:
    FD m_fd;
    std::vector<std::weak_ptr<InputDevice>> m_inputs;
    ValueRef m_rel[REL_CNT];
    ValueRef m_key[KEY_CNT];
    ValueRef m_abs[ABS_CNT];

    std::shared_ptr<InputDevice> find_input(const std::string &name, IInputByName *inputFinder);
    ValueRef parse_ref(const std::string &desc, IInputByName &inputFinder);

    void write_value(int type, int code, int value);
};

#endif /* OUTPUTDEV_H_INCLUDED */
