#ifndef INPUTSTEAM_H_INCLUDED
#define INPUTSTEAM_H_INCLUDED

#include "inifile.h"
#include "inputdev.h"
#include "steamcontroller.h"

class InputDeviceSteam : public InputDevice
{
public:
    explicit InputDeviceSteam(const IniSection &ini);

    virtual int fd()
    { return m_steam.fd(); }
    virtual ValueId parse_value(const std::string &name);
    virtual PollResult on_poll(int event);
    virtual int get_value(const ValueId &id);
    virtual input_absinfo get_absinfo(int code);
    virtual void flush();
private:
    SteamController m_steam;
};

#endif /* INPUTSTEAM_H_INCLUDED */
