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

ValueId parse_value_id(const std::string &name);

enum class PollResult
{
    None,
    Error,
    Sync,
};

class InputDevice : public std::enable_shared_from_this<InputDevice>
{
public:
    explicit InputDevice(const IniSection &ini, FD fd);
    const std::string &name() const noexcept
    { return m_name; }
    int fd() const noexcept
    { return m_fd.get(); }
    PollResult on_poll(int event);

    int get_value(const ValueId &id) const;
    void flush();

private:
    FD m_fd;
    std::string m_name;
    input_event m_evs[128];
    int m_num_evs;
    InputStatus m_status;

    void on_input(input_event &ev);
};

#endif /* INPUTDEV_H_INCLUDED */
