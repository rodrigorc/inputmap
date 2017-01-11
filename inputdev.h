#ifndef INPUTDEV_H_INCLUDED
#define INPUTDEV_H_INCLUDED

#include <memory>
#include <linux/input.h>
#include "fd.h"
#include "inifile.h"

struct InputStatus
{
    int abs[ABS_CNT];
    int rel[REL_CNT];
    bool key[KEY_CNT];

    InputStatus()
    {
        reset();
    }
    void reset()
    {
        memset(abs, 0, sizeof(abs));
        memset(rel, 0, sizeof(rel));
        memset(key, 0, sizeof(key));
    }
};

struct ValueId
{
    int type;
    int code;

    ValueId()
        :type(0), code(0)
    {}
    ValueId(int t, int c)
        :type(t), code(c)
    {}
};

enum class PollResult
{
    None,
    Error,
    Sync,
};

class InputDevice : public std::enable_shared_from_this<InputDevice>
{
public:
    virtual ~InputDevice() {}
    const std::string &name() const noexcept
    { return m_name; }

    virtual int fd() =0;
    virtual ValueId parse_value(const std::string &name) =0;
    virtual PollResult on_poll(int event) =0;
    virtual int get_value(const ValueId &id) =0;
    virtual input_absinfo get_absinfo(int code) =0;
    virtual void flush() =0;

protected:
    InputDevice(const IniSection &ini);
private:
    std::string m_name;
};

std::shared_ptr<InputDevice> InputDeviceEventCreate(const IniSection &ini, const std::string &id);

class InputDeviceEvent : public InputDevice
{
public:
    explicit InputDeviceEvent(const IniSection &ini, FD fd);

    virtual int fd()
    { return m_fd.get(); }
    virtual ValueId parse_value(const std::string &name);
    virtual PollResult on_poll(int event);
    virtual int get_value(const ValueId &id);
    virtual input_absinfo get_absinfo(int code);
    virtual void flush();
private:
    FD m_fd;
    input_event m_evs[128];
    int m_num_evs;
    InputStatus m_status;

    void on_input(input_event &ev);
};

#endif /* INPUTDEV_H_INCLUDED */
