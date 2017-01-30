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

#ifndef INPUTSTEAM_H_INCLUDED
#define INPUTSTEAM_H_INCLUDED

#include "inifile.h"
#include "inputdev.h"
#include "steam/steamcontroller.h"

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
    unsigned m_count;
    bool m_auto_haptic_left, m_auto_haptic_right;
};

#endif /* INPUTSTEAM_H_INCLUDED */
