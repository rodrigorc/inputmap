#include <stdint.h>
#include <sys/epoll.h>
#include <linux/hidraw.h>
#include "udev-wrapper.h"
#include "inputdev.h"
#include "inputsteam.h"
#include "event-codes.h"

/************************
 * SteamController interface:

* CONNECT:
idx:   0                       8                      16                      24                      32                      40                      48                      56                      64
ex:   01 00 03 01 02 05 00 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-
    v-/--/  |  |  |
    type(3)-/  |  |
    size ------/  |
    conn ---------/

conn:
 * 1 Disconnected
 * 2 Connected

* STATUS:
idx:   0                       8                      16                      24                      32                      40                      48                      56                      64
ex:   01 00 04 0B BB FD 02 00-00 00 00 00 32 14 64 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-
    v-/--/  |  |  |  |  |  |              |  |  |
    type(4)-/  |  |  |  |  |              |  |  |
    size ------/  |  |  |  |              |  |  |
    seq  ---------/--/--/--/              |  |  |
    volt ---------------------------------/--/  |
    batt%---------------------------------------/

* INPUT:
idx:   0                       8                      16                      24                      32                      40                      48                      56                      64
ex:   01 00 01 3C B9 0F 0F 00-00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00-00 00 00 00 55 00 20 FD-4D 40 FF FF 01 00 02 00-EE 7C 46 FD 46 00 4B E4-00 00 78 01 7A 01 FE 01-F7 01 00 00 00 00 31 14-
    v-/--/  |  |  |  |  |  |  |  |  |  |  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    type(1)-/  |  |  |  |  |  |  |  |  |  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    size ------/  |  |  |  |  |  |  |  |  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    seq  ---------/--/--/--/  |  |  |  |  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    btn  ---------------------/--/--/  |  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    tl   ------------------------------/  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    tr   ---------------------------------/           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    lx   ---------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    ly   ---------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    rx   ---------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    ry   ---------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
   *tl16 ---------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
   *tr16 ---------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
   *acc1 ---------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
   *acc2 ---------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
   *acc3 ---------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    gyr1 ---------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    gyr2 ---------------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    gyr3 ---------------------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    quat1---------------------------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    quat2---------------------------------------------------------------------------------------------------------------------------/--/  |  |  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    quat3---------------------------------------------------------------------------------------------------------------------------------/--/  |  |        |  |  |  |  |  |  |  |  |  |  |  |  |  |
    quat4---------------------------------------------------------------------------------------------------------------------------------------/--/        |  |  |  |  |  |  |  |  |  |  |  |  |  |
   *tlraw---------------------------------------------------------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |
   *trraw---------------------------------------------------------------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |
   *jxraw---------------------------------------------------------------------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |
   *jyraw---------------------------------------------------------------------------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |
   *ux   ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------/--/  |  |  |  |
   *uy   ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------/--/  |  |
   *volt ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------/--/

*: not sent over wireless

Known commands:

 * 0x81: Init?
 * 0x83: return version info
 * 0xAE: return serial info: (15 00: board?; 15 01: serial)
 * 0xBA: return unknown info
 * 0x87: write register (LEN REG BL BH). LEN in bytes (= 3 * #writes)
 * 0xC5: ??? (03 FF FF FF)
 * 0xC1: ??? (10 FFFFFFFF 030905FF FFFFFFFF FFFFFFFF)
 * 0x8F: haptic feedback (LEN SIDE AMPL AMPH PERL PERH CNTL CNTH SHAPE)
         (LEN=7 or 8) Ex: rmouse=8f 08 00 6400 0000 0100 00
 * 0x85: enable key emulation
 * 0x8E: enable mouse & cursor emulation

Known registers:

 * 0x30: Accelerometer. 0x00=disable, 0x14=enable.
 * 0x08: 0x07=disable mouse emulation
 * 0x07: 0x07=disable cursor emulation
 * 0x18: 0x00=remove margin in rpad
 * 0x2D: 0x64=???
 * 0x2E: 0x00=???
 * 0x31: 0x02=???
 * 0x32: 0x012C=???
 * 0x34-0x3B: 0x0A=???
*************/

InputDeviceSteam::InputDeviceSteam(const IniSection &ini, FD the_fd)
    :InputDevice(ini, std::move(the_fd))
{
    memset(m_data, 0, sizeof(m_data));

    //init?
    //send_cmd({0x81});

    //enable accelerometer
    send_cmd({0x87, 0x03, 0x30, 0x14, 0x00});
    //disable accelerometer
    //send_cmd({0x87, 0x03, 0x30, 0x00, 0x00});
    //enable key emulation
    //send_cmd({0x85});
    //enable mouse & cursor emulation
    //send_cmd({0x8e});
    //disable mouse emulation
    send_cmd({0x87, 0x03, 0x08, 0x07, 0x00});
    //disable cursor emulation
    send_cmd({0x87, 0x03, 0x07, 0x07, 0x00});
    //disable key emulation
    send_cmd({0x81});
    //remove margin in rpad
    send_cmd({0x87, 0x03, 0x18, 0x00, 0x00});

    //unknown commands
    //send_cmd({0xc5, 0x03, 0xff, 0xff, 0xff});
    //
    //send_cmd({0x87, 0x03, 0x2d, 0x64, 0x00});
    //send_cmd({0x87, 0x03, 0x32, 0x2c, 0x01});
    //send_cmd({0x87, 0x03, 0x31, 0x02, 0x00});
    //send_cmd({0x87, 0x03, 0x2e, 0x00, 0x00});
    //send_cmd({0x87, 0x03, 0x3b, 0x0a, 0x00});
    //send_cmd({0x87, 0x03, 0x3a, 0x0a, 0x00});
    //send_cmd({0x87, 0x03, 0x39, 0x0a, 0x00});
    //send_cmd({0x87, 0x03, 0x38, 0x0a, 0x00});
    //send_cmd({0x87, 0x03, 0x37, 0x0a, 0x00});
    //send_cmd({0x87, 0x03, 0x36, 0x0a, 0x00});
    //send_cmd({0x87, 0x03, 0x35, 0x0a, 0x00});
    //send_cmd({0x87, 0x03, 0x34, 0x0a, 0x00});
}

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

EventName g_steam_abs_names[] =
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
};

EventName g_steam_button_names[] =
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
    throw std::runtime_error("unknown value name " + name);
}

static uint16_t U2(const uint8_t *m, size_t x)
{
    return m[x] | (m[x+1] << 8);
}
static int16_t S2(const uint8_t *m, size_t x)
{
    int16_t res = U2(m, x);
    if (res == -32768) //or else you will get weird values when negating this value
        res = -32767;
    return res;
}
/*static uint32_t U4(const uint8_t *m, size_t x)
{
    uint32_t b1 = U2(m, x);
    uint32_t b2 = U2(m, x+2);
    return b1 | (b2 << 16);
}
static int32_t S4(const uint8_t *m, size_t x)
{
    return U4(m, x);
}*/
static uint32_t U3(const uint8_t *m, size_t x)
{
    uint32_t b1 = U2(m, x);
    uint32_t b2 = m[x+2];
    return b1 | (b2 << 16);
}

PollResult InputDeviceSteam::on_poll(int event)
{
    if ((event & EPOLLIN) == 0)
        return PollResult::None;

    uint8_t data[64];
    int res = read(fd(), data, sizeof(data));
    if (res == -1)
    {
        if (errno == EINTR)
            return PollResult::None;
        perror("input read");
        return PollResult::Error;
    }
    if (res != sizeof(data))
        return PollResult::Error;

    uint8_t type = data[2];
    if (type != 1) //input
        return PollResult::None;

    memcpy(m_data, data, sizeof(m_data));
    return PollResult::Sync;
}

int int_sign(int x)
{
    if (x < 0)
        return -1;
    if (x > 0)
        return 1;
    return 0;
}
int InputDeviceSteam::get_value(const ValueId &id)
{
    uint32_t btn = U3(m_data, 8);

    /*
    if (btn & SteamButton::RightPadTouch)
        send_cmd({0x8f, 0x08, 0x00, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00});
    if (btn & SteamButton::LeftPadTouch)
        send_cmd({0x8f, 0x08, 0x01, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00});
    */

    switch (id.type)
    {
    case EV_ABS:
        switch (id.code)
        {
        case SteamAxis::X:
            return S2(m_data, 16);
        case SteamAxis::Y:
            return -S2(m_data, 18);
        case SteamAxis::StickX:
            switch (btn & (SteamButton::LPadTouch | SteamButton::LPadAndJoy))
            {
            case SteamButton::LPadTouch:
                return 0;
            case 0:
            case SteamButton::LPadAndJoy:
                return m_stickX = S2(m_data, 16);
            case SteamButton::LPadTouch | SteamButton::LPadAndJoy:
                return m_stickX;
            }
        case SteamAxis::StickY:
            switch (btn & (SteamButton::LPadTouch | SteamButton::LPadAndJoy))
            {
            case SteamButton::LPadTouch:
                return 0;
            case 0:
            case SteamButton::LPadAndJoy:
                return m_stickY = -S2(m_data, 18);
            case SteamButton::LPadTouch | SteamButton::LPadAndJoy:
                return m_stickY;
            }
        case SteamAxis::LPadX:
            switch (btn & (SteamButton::LPadTouch | SteamButton::LPadAndJoy))
            {
            case 0:
                return 0;
            case SteamButton::LPadTouch:
            case SteamButton::LPadTouch | SteamButton::LPadAndJoy:
                return m_lpadX = S2(m_data, 16);
            case SteamButton::LPadAndJoy:
                return m_lpadX;
            }
        case SteamAxis::LPadY:
            switch (btn & (SteamButton::LPadTouch | SteamButton::LPadAndJoy))
            {
            case 0:
                return 0;
            case SteamButton::LPadTouch:
            case SteamButton::LPadTouch | SteamButton::LPadAndJoy:
                return m_lpadY = -S2(m_data, 18);
            case SteamButton::LPadAndJoy:
                return m_lpadY;
            }
        case SteamAxis::RPadX:
            return S2(m_data, 20);
        case SteamAxis::RPadY:
            return -S2(m_data, 22);
        case SteamAxis::LTrigger:
            return m_data[11];
        case SteamAxis::RTrigger:
            return m_data[12];
        case SteamAxis::GyroX:
            return S2(m_data, 34);
        case SteamAxis::GyroY:
            return S2(m_data, 36);
        case SteamAxis::GyroZ:
            return S2(m_data, 38);
/*
        case ABS_HAT0X:
            if (btn & SteamButton::LPadClick)
                return int_sign(S2(m_data, 16) * 3 / 32768);
            else
                return 0;
        case ABS_HAT0Y:
            if (btn & SteamButton::LPadClick)
                return int_sign(-S2(m_data, 18) * 3 / 32768);
            else
                return 0;
        case ABS_HAT1X:
            if (btn & SteamButton::RPadClick)
                return int_sign(S2(m_data, 20) * 3 / 32768);
            else
                return 0;
        case ABS_HAT1Y:
            if (btn & SteamButton::RPadClick)
                return int_sign(-S2(m_data, 22) * 3 / 32768);
            else
                return 0;
*/
        }
        break;
    case EV_KEY:
        return (btn & id.code) != 0;
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
            res.minimum = -32768;
            res.maximum = 32767;
            break;

        case SteamAxis::LTrigger:
        case SteamAxis::RTrigger:
            res.minimum = -255;
            res.maximum = 255;
            break;
            
        case SteamAxis::GyroX:
        case SteamAxis::GyroY:
        case SteamAxis::GyroZ:
            res.minimum = -32768;
            res.maximum = 32767;
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

void InputDeviceSteam::flush()
{
}

static std::vector<std::string> find_udev_devices(udev *ud, udev_device *parent, const char *subsystem, 
        const char *attr, const char *value,
        const char *attr2 = nullptr, const char *value2 = nullptr
        )
{
    std::vector<std::string> res;
    udev_enumerate_ptr ude { udev_enumerate_new(ud) };
    if (subsystem)
        udev_enumerate_add_match_subsystem(ude.get(), subsystem);
    if (attr)
        udev_enumerate_add_match_sysattr(ude.get(), attr, value);
    if (attr2)
        udev_enumerate_add_match_sysattr(ude.get(), attr2, value2);
    if (parent)
        udev_enumerate_add_match_parent(ude.get(), parent);
    udev_enumerate_scan_devices(ude.get());
    for (udev_list_entry *le = udev_enumerate_get_list_entry(ude.get()); le; le = udev_list_entry_get_next(le))
    {
        const char *name = udev_list_entry_get_name(le);
        res.push_back(name);
    }
    return res;
}

static std::string find_steam_devpath()
{
    udev_ptr ud { udev_new() };

    std::vector<std::string> sys_usbs = find_udev_devices(ud.get(), nullptr, "usb", "idVendor", "28de", "idProduct", "1102"); //wired
    std::vector<std::string> sys_usbs2 = find_udev_devices(ud.get(), nullptr, "usb", "idVendor", "28de", "idProduct", "1142"); //wireless
    sys_usbs.insert(sys_usbs.end(),
             std::make_move_iterator(sys_usbs2.begin()),
             std::make_move_iterator(sys_usbs2.end()));
    for (const std::string &sys_usb : sys_usbs)
    {
        udev_device_ptr usb { udev_device_new_from_syspath(ud.get(), sys_usb.c_str()) };

        for (const std::string &sys_itf : find_udev_devices(ud.get(), usb.get(), "usb", "bInterfaceProtocol", "00"))
        {
            udev_device_ptr itf { udev_device_new_from_syspath(ud.get(), sys_itf.c_str()) };
            
            for (const std::string &sys_hid : find_udev_devices(ud.get(), itf.get(), "hidraw", nullptr, nullptr))
            {
                udev_device_ptr hid { udev_device_new_from_syspath(ud.get(), sys_hid.c_str()) };
                return udev_device_get_devnode(hid.get());
            }
        }
    }
    throw std::runtime_error("steam device not found");
}

std::shared_ptr<InputDevice> InputDeviceSteamCreate(const IniSection &ini)
{
    std::string devpath = find_steam_devpath();
    printf("Steam %s\n", devpath.c_str());
    FD fd { FD_open(devpath.c_str(), O_RDWR|O_CLOEXEC) };
    return std::make_shared<InputDeviceSteam>(ini, std::move(fd));
}

void InputDeviceSteam::send_cmd(const std::initializer_list<uint8_t> &data)
{
    unsigned char feat[65] = {0};
    memcpy(feat+1, data.begin(), data.size());
    test(ioctl(fd(), HIDIOCSFEATURE(sizeof(feat)), feat), "HIDIOCSFEATURE");
}

