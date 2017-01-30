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

#include <stdint.h>
#include <vector>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <linux/hidraw.h>
#include "steamcontroller.h"
#include "fd.h"
#include "udev-wrapper.h"

/*
#include <sys/epoll.h>
#include <linux/hidraw.h>
#include "udev-wrapper.h"
#include "inputdev.h"
#include "inputsteam.h"
#include "event-codes.h"
*/
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
 * 0x83: return version info. The reply looks like a list of 5 byte struct { uint8_t id; uint32_t data }. The IDs are:
    - 0x00: firmware version?
    - 0x01: product id (0x1102=Steam Controller)
    - 0x02: capabilities? (0x03)
    - 0x04: firmware build time
    - 0x05: some unknown time
    - 0x0A: bootloader firmware build time
 * 0xAE: return serial info: (15 00: board?; 15 01: serial)
 * 0xBA: return unknown info
 * 0x87: write register (LEN REG BL BH). LEN in bytes (= 3 * #writes)
 * 0xC5: ??? (03 FF FF FF)
 * 0xC1: ??? (10 FFFFFFFF 030905FF FFFFFFFF FFFFFFFF)
 * 0x8F: haptic feedback (LEN SIDE ONL ONH OFFL OFFH CNTL CNTH UNK)
         (LEN=7 or 8) Ex: rmouse=8f 08 00 6400 0000 0100 00
         SIDE: 0=right; 1=left.
         ON: time the motor is on each pulse, in us.
         OFF: time the motor is off each pulse, in us.
         CNT: number of pulses.
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

SteamController::SteamController(FD fd)
    :m_fd(std::move(fd))
{
    memset(m_data, 0, sizeof(m_data));

    //remove margin in rpad, we do this unconditionally
    write_register(0x18, 0);

#if 0
    uint8_t reply[64] = {};
    send_cmd({0x83});
    recv_cmd(reply);
    for (int i = 0; i < 64; ++i)
        printf(" %02X", reply[i]);
    printf("\n");
#endif
}

SteamController::~SteamController() noexcept
{
    try
    {
    if (m_fd)
        set_emulation_mode(static_cast<SteamEmulation>(SteamEmulation::Keys | SteamEmulation::Cursor | SteamEmulation::Mouse));
    }
    catch (...)
    { //ignore errors
    }
}

static inline uint16_t U2(const uint8_t *m, size_t x)
{
    return m[x] | (m[x+1] << 8);
}
static inline int16_t S2(const uint8_t *m, size_t x)
{
    int16_t res = U2(m, x);
    if (res == -32768) //or else you will get weird values when negating this value
        res = -32767;
    return res;
}
/*static inline uint32_t U4(const uint8_t *m, size_t x)
{
    uint32_t b1 = U2(m, x);
    uint32_t b2 = U2(m, x+2);
    return b1 | (b2 << 16);
}
static inline int32_t S4(const uint8_t *m, size_t x)
{
    return U4(m, x);
}*/
static inline uint32_t U3(const uint8_t *m, size_t x)
{
    uint32_t b1 = U2(m, x);
    uint32_t b2 = m[x+2];
    return b1 | (b2 << 16);
}

static inline uint8_t BL(uint16_t x)
{
    return x & 0xFF;
}
static inline uint8_t BH(uint16_t x)
{
    return x >> 8;
}

bool SteamController::on_poll(int event)
{
    if ((event & EPOLLIN) == 0)
        return false;

    uint8_t data[64];
    int res = read(m_fd.get(), data, sizeof(data));
    if (res == -1)
    {
        if (errno == EINTR)
            return false;;
        throw std::runtime_error("read error");
    }
    if (res != sizeof(data))
        return false;

    uint8_t type = data[2];
    if (type != 1) //input
        return false;

    memcpy(m_data, data, sizeof(m_data));
    return true;
}

/*
static int int_sign(int x)
{
    if (x < 0)
        return -1;
    if (x > 0)
        return 1;
    return 0;
}
*/

int SteamController::get_axis(SteamAxis axis)
{
    uint32_t btn = U3(m_data, 8);
    switch (axis)
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
    default:
        return 0;
    }
}

bool SteamController::get_button(SteamButton btn)
{
    if (btn == SteamButton::LPadTouch)
        btn = static_cast<SteamButton>(SteamButton::LPadTouch | SteamButton::LPadAndJoy);
    uint32_t btns = U3(m_data, 8);
    return (btns & btn) != 0;
}

void SteamController::send_cmd(const std::initializer_list<uint8_t> &data)
{
    unsigned char feat[65] = {0};
    memcpy(feat + 1, data.begin(), data.size());

    timespec x;
    x.tv_sec = 0;
    x.tv_nsec = 50000000; // 50 ms

    //This command sometimes fails with EPIPE, particularly with the wireless device
    //so retry a few times before giving up.
    for (int i = 0; i < 10; ++i) // up to 500 ms
    {
        if (ioctl(m_fd.get(), HIDIOCSFEATURE(sizeof(feat)), feat) >= 0)
            return;
        nanosleep(&x, &x);
    }
    throw std::runtime_error("HIDIOCSFEATURE");
}

void SteamController::recv_cmd(uint8_t reply[64])
{
    unsigned char feat[65] = {0};
    test(ioctl(m_fd.get(), HIDIOCGFEATURE(sizeof(feat)), feat), "HIDIOCGFEATURE");
    memcpy(reply, feat + 1, 64);
}


void SteamController::write_register(uint8_t reg, uint16_t value)
{
    send_cmd({0x87, 0x03, reg, BL(value), BH(value)});
}


void SteamController::set_accelerometer(bool enable)
{
    write_register(0x30, enable? 0x14 : 0x00);
}

void SteamController::set_emulation_mode(SteamEmulation mode)
{
    if (mode & SteamEmulation::Keys)
        send_cmd({0x85});
    else
        send_cmd({0x81});

    //I don't know how to enable these two separately, but I know how to disable them, so...
    switch (mode & (SteamEmulation::Cursor | SteamEmulation::Mouse))
    {
    case 0:
        write_register(0x08, 0x07);
        write_register(0x07, 0x07);
        break;
    case SteamEmulation::Cursor:
        send_cmd({0x8e});
        write_register(0x08, 0x07);
        break;
    case SteamEmulation::Mouse:
        send_cmd({0x8e});
        write_register(0x07, 0x07);
        break;
    case SteamEmulation::Cursor | SteamEmulation::Mouse:
        send_cmd({0x8e});
        break;
    }
}

void SteamController::haptic(bool left, int time_on, int time_off, int cycles)
{
    send_cmd({0x8f, 0x08,
        BL(left? 1 : 0),
        BL(time_on), BH(time_on),
        BL(time_off), BH(time_off),
        BL(cycles), BH(cycles),
        0,
    });
}

void SteamController::haptic_freq(bool left, int freq, int duty, int duration)
{
    int period = 1000000 / freq; //in us
    int time_on = period * duty / 100;
    int time_off = period - time_on;
    if (time_on <= 0 || time_off < 0)
        return;
    int cycles = duration / period;
    if (cycles <= 0)
        cycles = 1;
    haptic(left, time_on, time_off, cycles);
}

std::string SteamController::get_serial()
{
    uint8_t reply[64] = {};
    send_cmd({0xAE, 0x15, 0x01});
    recv_cmd(reply);
    reply[13] = 0;
    std::string serial = reinterpret_cast<const char *>(reply + 3);
    return serial;
}

std::string SteamController::get_board()
{
    uint8_t reply[64] = {};
    send_cmd({0xAE, 0x15, 0x00});
    recv_cmd(reply);
    reply[13] = 0;
    std::string board = reinterpret_cast<const char *>(reply + 3);
    return board;
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

static std::vector<std::string> find_steam_devpaths()
{
    std::vector<std::string> res;

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
                res.push_back(udev_device_get_devnode(hid.get()));
            }
        }
    }
    return res;
}

SteamController SteamController::Create(const std::string &serial)
{
    std::vector<std::string> devpaths = find_steam_devpaths();
    for (const std::string &devpath : devpaths)
    {
        printf("Device %s\n", devpath.c_str());
        FD fd { FD_open(devpath.c_str(), O_RDWR|O_CLOEXEC) };
        SteamController sc(std::move(fd));
        std::string detected = sc.get_serial();
        printf("Serial '%s'\n", detected.c_str());
        if (detected.empty())
            continue; //no actual device
        //printf("Board '%s'\n", sc.get_board().c_str());
        if (serial.empty() || serial == detected)
            return sc;
    }
    throw std::runtime_error("steam device not found");
}

