#ifndef INPUTSTEAM_H_INCLUDED
#define INPUTSTEAM_H_INCLUDED

#include <memory>
#include <linux/input.h>
#include "fd.h"
#include "inifile.h"
#include "inputdev.h"

std::shared_ptr<InputDevice> InputDeviceSteamCreate(const IniSection &ini);

class InputDeviceSteam : public InputDevice
{
public:
    explicit InputDeviceSteam(const IniSection &ini, FD fd);

    virtual ValueId parse_value(const std::string &name);
    virtual PollResult on_poll(int event);
    virtual int get_value(const ValueId &id);
    virtual input_absinfo get_absinfo(int code);
    virtual void flush();
private:
    int16_t m_lpadX, m_lpadY, m_stickX, m_stickY;
    uint8_t m_data[64];

    void send_cmd(const std::initializer_list<uint8_t> &data);
};

#endif /* INPUTSTEAM_H_INCLUDED */
