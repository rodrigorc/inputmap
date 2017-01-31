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

#include <stdint.h>
#include <sys/epoll.h>
#include <linux/hidraw.h>
#include <math.h>
#include "udev-wrapper.h"
#include "inputdev.h"
#include "inputsteam.h"
#include "event-codes.h"

enum PseudoAxis
{
    LPadAngle = 1000,
    LPadRadius,
    RPadAngle,
    RPadRadius,
};

static EventName g_steam_abs_names[] =
{
    {SteamAxis::X,        "X"},
    {SteamAxis::Y,        "Y"},
    {SteamAxis::StickX,   "StickX"},
    {SteamAxis::StickY,   "StickY"},
    {SteamAxis::LPadX,    "LPadX"},
    {SteamAxis::LPadY,    "LPadY"},
    {SteamAxis::RPadX,    "RPadX"},
    {SteamAxis::RPadY,    "RPadY"},
    {SteamAxis::LTrigger, "LTrigger"},
    {SteamAxis::RTrigger, "RTrigger"},
    {SteamAxis::GyroX,    "GyroX"},
    {SteamAxis::GyroY,    "GyroY"},
    {SteamAxis::GyroZ,    "GyroZ"},
    {PseudoAxis::LPadAngle,  "LPadAngle"},
    {PseudoAxis::LPadRadius, "LPadRadius"},
    {PseudoAxis::RPadAngle,  "RPadAngle"},
    {PseudoAxis::RPadRadius, "RPadRadius"},
};

static EventName g_steam_button_names[] =
{
    {SteamButton::RTriggerFull, "RTriggerFull"},
    {SteamButton::LTriggerFull, "LTriggerFull"},
    {SteamButton::RBumper,      "RBumper"},
    {SteamButton::LBumper,      "LBumper"},
    {SteamButton::BtnY,         "BtnY"},
    {SteamButton::BtnB,         "BtnB"},
    {SteamButton::BtnX,         "BtnX"},
    {SteamButton::BtnA,         "BtnA"},
    {SteamButton::North,        "North"},
    {SteamButton::East,         "East"},
    {SteamButton::West,         "West"},
    {SteamButton::South,        "South"},
    {SteamButton::Menu,         "Menu"},
    {SteamButton::Steam,        "Steam"},
    {SteamButton::Escape,       "Escape"},
    {SteamButton::LBack,        "LBack"},
    {SteamButton::RBack,        "RBack"},
    {SteamButton::LPadClick,    "LPadClick"},
    {SteamButton::RPadClick,    "RPadClick"},
    {SteamButton::LPadTouch,    "LPadTouch"},
    {SteamButton::RPadTouch,    "RPadTouch"},
    {SteamButton::Unknown2,     "Unknown2"},
    {SteamButton::Stick,        "Stick"},
    {SteamButton::LPadAndJoy,   "LPadAndJoy"},
};

InputDeviceSteam::InputDeviceSteam(const IniSection &ini)
:InputDevice(ini),
    m_steam(SteamController::Create(ini.find_single_value("serial").c_str())),
    m_count(0)
{
    bool mouse = parse_bool(ini.find_single_value("mouse"), false);
    m_steam.set_emulation_mode(mouse? SteamEmulation::Mouse : SteamEmulation::None);

    std::string auto_haptic = ini.find_single_value("auto_haptic");
    m_auto_haptic_left = auto_haptic.find('l') != std::string::npos ||
                         auto_haptic.find('L') != std::string::npos;
    m_auto_haptic_right = auto_haptic.find('r') != std::string::npos ||
                         auto_haptic.find('R') != std::string::npos;
}

ValueId InputDeviceSteam::parse_value(const std::string &name)
{
    for (const auto &ev : g_steam_abs_names)
    {
        if (ev.name == name)
            return ValueId{ EV_ABS, ev.id };
    }
    for (const auto &ev : g_steam_button_names)
    {
        if (ev.name == name)
            return ValueId{ EV_KEY, ev.id };
    }
    for (const auto &kv : g_ff_names)
    {
        if (kv.name && kv.name == name)
            return ValueId(EV_FF, kv.id);
    }
    throw std::runtime_error("unknown value name " + name);
}

PollResult InputDeviceSteam::on_poll(int event)
{
    if (!m_steam.on_poll(event))
        return PollResult::None;
    ++m_count;

    if (m_auto_haptic_left)
        if (m_steam.get_button(SteamButton::LPadTouch))
            m_steam.haptic_freq(true, 200, 50, 8000);
    if (m_auto_haptic_right)
        if (m_steam.get_button(SteamButton::RPadTouch))
            m_steam.haptic_freq(false, 200, 50, 10000);
    return PollResult::Sync;
}

int InputDeviceSteam::get_value(const ValueId &id)
{
    /*
    if (btn & SteamButton::RightPadTouch)
        send_cmd({0x8f, 0x08, 0x00, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00});
    if (btn & SteamButton::LeftPadTouch)
        send_cmd({0x8f, 0x08, 0x01, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00});
    */

    switch (id.type)
    {
    case EV_ABS:
        //For some reason the pads are rotated by about 15 degrees
        switch (id.code)
        {
        case PseudoAxis::LPadAngle:
            {
                int x = m_steam.get_axis(SteamAxis::LPadX);
                int y = m_steam.get_axis(SteamAxis::LPadY);
                float a = atan2(y, x);
                return a * (180 / M_PI) + 15;
            }
            break;
        case PseudoAxis::RPadAngle:
            {
                int x = m_steam.get_axis(SteamAxis::RPadX);
                int y = m_steam.get_axis(SteamAxis::RPadY);
                float a = atan2(y, x);
                return a * (180 / M_PI) - 15;
            }
            break;
        case PseudoAxis::LPadRadius:
            {
                int x = m_steam.get_axis(SteamAxis::LPadX);
                int y = m_steam.get_axis(SteamAxis::LPadY);
                float a = hypot(y, x);
                return a;
            }
            break;
        case PseudoAxis::RPadRadius:
            {
                int x = m_steam.get_axis(SteamAxis::RPadX);
                int y = m_steam.get_axis(SteamAxis::RPadY);
                float a = hypot(y, x);
                return a;
            }
            break;
        default:
            return m_steam.get_axis(static_cast<SteamAxis>(id.code));
        }
    case EV_KEY:
        return m_steam.get_button(static_cast<SteamButton>(id.code));
        break;
    }
    return 0;
}

input_absinfo InputDeviceSteam::get_absinfo(int code)
{
    input_absinfo res {};
    res.value = get_value(ValueId(EV_ABS, code));
    switch (code)
    {
        case SteamAxis::X:
        case SteamAxis::Y:
        case SteamAxis::StickX:
        case SteamAxis::StickY:
        case SteamAxis::LPadX:
        case SteamAxis::LPadY:
        case SteamAxis::RPadX:
        case SteamAxis::RPadY:
            res.minimum = -32767;
            res.maximum = 32767;
            break;

        case SteamAxis::LTrigger:
        case SteamAxis::RTrigger:
            res.minimum = 0;
            res.maximum = 255;
            break;

        case SteamAxis::GyroX:
        case SteamAxis::GyroY:
        case SteamAxis::GyroZ:
            res.minimum = -32767;
            res.maximum = 32767;
            break;
        case PseudoAxis::LPadAngle:
        case PseudoAxis::RPadAngle:
            res.minimum = -180;
            res.minimum = 180;
            break;
        case PseudoAxis::LPadRadius:
        case PseudoAxis::RPadRadius:
            res.minimum = 0;
            res.minimum = 32767;
            break;

/*
    case ABS_HAT0X:
    case ABS_HAT0Y:
    case ABS_HAT1X:
    case ABS_HAT1Y:
        res.minimum = -1;
        res.maximum = 1;
        break;
    default:
    */
    }
    return res;
}

int InputDeviceSteam::ff_upload(const ff_effect &eff)
{
    return 0;
}
int InputDeviceSteam::ff_erase(int id)
{
    return 0;
}
void InputDeviceSteam::ff_run(int eff, bool on)
{
    if (on)
    {
        //printf("haptic on %d\n", eff);
        m_steam.haptic(true, 5000, 5000, 65535);
        m_steam.haptic(false, 5000, 5000, 65535);
    }
    else
    {
        //printf("haptic off %d\n", eff);
        m_steam.haptic(true, 1, 1, 1);
        m_steam.haptic(false, 1, 1, 1);
    }
}

void InputDeviceSteam::flush()
{
}

