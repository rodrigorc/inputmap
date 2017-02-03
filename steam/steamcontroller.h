/*

Copyright 2017, Rodrigo Rivas Costa <rodrigorivascosta@gmail.com>

This file is part of inputmap.

inputmap is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

inputmap is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with inputmap.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef STEAMCONTROLLER_H_INCLUDED
#define STEAMCONTROLLER_H_INCLUDED

#include "fd.h"

enum SteamAxis
{
    X,
    Y,
    StickX,
    StickY,
    LPadX,
    LPadY,
    RPadX,
    RPadY,
    LTrigger,
    RTrigger,
    GyroX,
    GyroY,
    GyroZ,
};

enum SteamButton
{
    RTriggerFull = 0x000001,
    LTriggerFull = 0x000002,
    RBumper      = 0x000004,
    LBumper      = 0x000008,
    BtnY         = 0x000010,
    BtnB         = 0x000020,
    BtnX         = 0x000040,
    BtnA         = 0x000080,
    North        = 0x000100,
    East         = 0x000200,
    West         = 0x000400,
    South        = 0x000800,
    Menu         = 0x001000,
    Logo         = 0x002000,
    Escape       = 0x004000,
    LBack        = 0x008000,
    RBack        = 0x010000,
    LPadClick    = 0x020000,
    RPadClick    = 0x040000,
    LPadTouch    = 0x080000,
    RPadTouch    = 0x100000,
    Unknown2     = 0x200000,
    Stick        = 0x400000,
    LPadAndJoy   = 0x800000,
};

enum SteamEmulation
{
    None    = 0,
    Keys    = 1, //enter, space, escape...
    Cursor  = 2, //up, down, left, right
    Mouse   = 4, //mouse cursor, left/right buttons
};

class SteamController
{
public:
    static SteamController Create(const std::string &serial);
    ~SteamController() noexcept;
    SteamController(SteamController &&o) = default;

    int fd()
    { return m_fd.get(); }
    bool on_poll(int event);

    int get_axis(SteamAxis axis);
    bool get_button(SteamButton btn);

    void set_accelerometer(bool enable);
    void set_emulation_mode(SteamEmulation mode);

    //left: true=left pad, false=right pad
    //time_on, time_off: in us
    void haptic(bool left, int time_on, int time_off, int cycles);
    //freq: in Hz
    //duty: 0-100 %
    //duration: in us
    void haptic_freq(bool left, int freq, int duty, int duration);
    std::string get_serial();
    std::string get_board();

private:
    FD m_fd;
    int16_t m_lpadX, m_lpadY, m_stickX, m_stickY;
    uint8_t m_data[64];

    SteamController(FD fd);
    void send_cmd(const std::initializer_list<uint8_t> &data);
    void recv_cmd(uint8_t reply[64]);
    void write_register(uint8_t reg, uint16_t value);
};

#endif /* STEAMCONTROLLER_H_INCLUDED */
