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
    Steam        = 0x002000,
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
    int fd()
    { return m_fd.get(); }
    bool on_poll(int event);

    int get_axis(SteamAxis axis);
    bool get_button(SteamButton btn);

    void set_accelerometer(bool enable);
    void set_emulation_mode(SteamEmulation mode);

    void haptic_feedback(bool left, int amplitude, int period, int duration);

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
