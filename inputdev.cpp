#include <stdint.h>
#include <sys/epoll.h>
#include <libudev.h>
#include <linux/hidraw.h>
#include "inputdev.h"
#include "event-codes.h"

ValueId parse_value_id(const std::string &name)
{
    for (const auto &kv : g_key_names)
    {
        if (kv.name && kv.name == name)
            return ValueId(EV_KEY, kv.id);
    }
    for (const auto &kv : g_rel_names)
    {
        if (kv.name && kv.name == name)
            return ValueId(EV_REL, kv.id);
    }
    for (const auto &kv : g_abs_names)
    {
        if (kv.name && kv.name == name)
            return ValueId(EV_ABS, kv.id);
    }
    throw std::runtime_error("unknown value name " + name);
}

InputDevice::InputDevice(const IniSection &ini, FD fd)
    :m_fd(std::move(fd))
{
    m_name = ini.find_single_value("name");
    if (m_name.empty())
        throw std::runtime_error("input without name");
}

InputDeviceEvent::InputDeviceEvent(const IniSection &ini, FD the_fd)
    :InputDevice(ini, std::move(the_fd)), m_num_evs(0)
{
    bool grab = parse_bool(ini.find_single_value("grab"), false);

    if (grab)
        test(ioctl(fd(), EVIOCGRAB, 1), "EVIOCGRAB");

    char buf[1024] = "";
    input_id iid;
    test(ioctl(fd(), EVIOCGID, &iid), "EVIOCGID");
    printf("    iid=%d %04x:%04x %d\n", iid.bustype, iid.vendor, iid.product, iid.version);
    test(ioctl(fd(), EVIOCGNAME(sizeof(buf)), buf), "EVIOCGNAME");
    printf("    name='%s'\n", buf);
    test(ioctl(fd(), EVIOCGPHYS(sizeof(buf)), buf), "EVIOCGPHYS");
    printf("    phys='%s'\n", buf);
    test(ioctl(fd(), EVIOCGUNIQ(sizeof(buf)), buf), "EVIOCGUNIQ");
    printf("    uniq='%s'\n", buf);
    test(ioctl(fd(), EVIOCGPROP(sizeof(buf)), buf), "EVIOCGPROP");
    printf("    prop='%s'\n", buf);

    test(ioctl(fd(), EVIOCGBIT(EV_REL, sizeof(buf)), buf), "EV_REL");
    printf("    rel: ");
    for (const auto &kv : g_rel_names)
    {
        if (!kv.name)
            continue;
        if (test_bit(kv.id, (unsigned char*)buf))
            printf(" %s", kv.name);
    }
    printf("\n");

    test(ioctl(fd(), EVIOCGBIT(EV_ABS, sizeof(buf)), buf), "EV_ABS");
    printf("    abs: ");
    for (const auto &kv : g_abs_names)
    {
        if (!kv.name)
            continue;
        if (test_bit(kv.id, (unsigned char*)buf))
            printf(" %s", kv.name);
    }
    printf("\n");

    test(ioctl(fd(), EVIOCGBIT(EV_KEY, sizeof(buf)), buf), "EV_KEY");
    printf("    key: ");
    for (const auto &kv : g_key_names)
    {
        if (!kv.name)
            continue;
        if (test_bit(kv.id, (unsigned char*)buf))
            printf(" %s", kv.name);
    }
    printf("\n");
}

PollResult InputDeviceEvent::on_poll(int event)
{
    if ((event & EPOLLIN) == 0)
        return PollResult::None;

    int free_evs = countof(m_evs) - m_num_evs;
    if (free_evs == 0)
        throw std::runtime_error("input buffer overflow");
    int res = read(fd(), &m_evs[m_num_evs], free_evs * sizeof(input_event));
    if (res == -1)
    {
        if (errno == EINTR)
            return PollResult::None;
        perror("input read");
        return PollResult::Error;
    }

    m_num_evs += res / sizeof(input_event);
    if (m_evs[m_num_evs - 1].type != EV_SYN)
    {
        printf("no EV_SYN, buffering\n");
        return PollResult::None;
    }

    for (int i = 0; i < m_num_evs; ++i)
        on_input(m_evs[i]);
    m_num_evs = 0;
    return PollResult::Sync;
}

void InputDeviceEvent::on_input(input_event &ev)
{
    switch (ev.type)
    {
    case EV_SYN:
        //printf("SYN %d %d\n", ev.code, ev.value);
        break;
    case EV_ABS:
        //printf("ABS %d %d\n", ev.code, ev.value);
        m_status.abs[ev.code] = ev.value;
        break;
    case EV_REL:
        //printf("REL %d %d\n", ev.code, ev.value);
        m_status.rel[ev.code] += ev.value;
        break;
    case EV_KEY:
        //printf("KEY %d %d\n", ev.code, ev.value);
        m_status.key[ev.code] = ev.value;
        break;
    case EV_MSC:
        //printf("MSC %d %d\n", ev.code, ev.value);
        break;
    default:
        //printf("??? %d %d %d\n", ev.type, ev.code, ev.value);
        break;
    }
}

int InputDeviceEvent::get_value(const ValueId &id) const
{
    switch (id.type)
    {
    case EV_REL:
        return m_status.rel[id.code];
    case EV_KEY:
        return m_status.key[id.code];
    default:
        return 0;
    }
}

input_absinfo InputDeviceEvent::get_absinfo(int code) const
{
    input_absinfo res;
    test(ioctl(fd(), EVIOCGABS(code), &res), "EVIOCGABS");
    return res;
}

void InputDeviceEvent::flush()
{
    memset(m_status.rel, 0, sizeof(m_status.rel));
}

std::shared_ptr<InputDevice> InputDeviceEventCreate(const IniSection &ini, const std::string &id)
{
    std::string dev = "/dev/input/by-id/" + id;
    FD fd { FD_open(dev.c_str(), O_RDWR|O_CLOEXEC) };
    return std::make_shared<InputDeviceEvent>(ini, std::move(fd));
}

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
    v-/--/  |  |  |  |  |  |  |  |  |  |  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    type(1)-/  |  |  |  |  |  |  |  |  |  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    size ------/  |  |  |  |  |  |  |  |  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    seq  ---------/--/--/--/  |  |  |  |  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    btn  ---------------------/--/--/  |  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    tl8  ------------------------------/  |           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    tr8  ---------------------------------/           |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    lx   ---------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    ly   ---------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    rx   ---------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    ry   ---------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    tl   ---------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    tr   ---------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    acc1 ---------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    acc2 ---------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    acc3 ---------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    gyr1 ---------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    gyr2 ---------------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    gyr3 ---------------------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |  |  |                                |  |  |  |  |  |
    quat1---------------------------------------------------------------------------------------------------------------------/--/  |  |  |  |  |  |                                |  |  |  |  |  |
    quat2---------------------------------------------------------------------------------------------------------------------------/--/  |  |  |  |                                |  |  |  |  |  |
    quat3---------------------------------------------------------------------------------------------------------------------------------/--/  |  |                                |  |  |  |  |  |
    quat4---------------------------------------------------------------------------------------------------------------------------------------/--/                                |  |  |  |  |  |
    ux   ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------/--/  |  |  |  |
    uy   ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------/--/  |  |
    volt ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------/--/


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

enum SteamButton
{
    RightTriggerFull= 0x000001,
    LeftTriggerFull = 0x000002,
    RightBumper     = 0x000004,
    LeftBumper      = 0x000008,
    BtnY            = 0x000010,
    BtnB            = 0x000020,
    BtnX            = 0x000040,
    BtnA            = 0x000080,
    North           = 0x000100,
    East            = 0x000200,
    West            = 0x000400,
    South           = 0x000800,
    Menu            = 0x001000,
    Steam           = 0x002000,
    Escape          = 0x004000,
    LeftBack        = 0x008000,
    RightBack       = 0x010000,
    LeftPadClick    = 0x020000,
    RightPadClick   = 0x040000,
    LeftPadTouch    = 0x080000,
    RightPadTouch   = 0x100000,
    Unknown200000   = 0x200000,
    Stick           = 0x400000,
    LeftPadAndJoy   = 0x800000,
};

int int_sign(int x)
{
    if (x < 0)
        return -1;
    if (x > 0)
        return 1;
    return 0;
}
int InputDeviceSteam::get_value(const ValueId &id) const
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
        case ABS_X:
            if (!(btn & SteamButton::LeftPadTouch))
                return m_x0 = S2(m_data, 16);
            else if (btn & SteamButton::LeftPadAndJoy)
                return m_x0;
            else
                return 0;
        case ABS_Y:
            if (!(btn & SteamButton::LeftPadTouch))
                return m_y0 = -S2(m_data, 18);
            else if (btn & SteamButton::LeftPadAndJoy)
                return m_y0;
            else
                return 0;
        case ABS_BRAKE:
            return U2(m_data, 24);
        case ABS_GAS:
            return U2(m_data, 26);
        case ABS_RX:
            return S2(m_data, 20);
        case ABS_RY:
            return -S2(m_data, 22);
        case ABS_Z:
            if (btn & SteamButton::LeftPadTouch)
                return m_z0 = S2(m_data, 58);
            else if (btn & SteamButton::LeftPadAndJoy)
                return m_z0;
            else
                return 0;
        case ABS_RZ:
            if (btn & SteamButton::LeftPadTouch)
                return m_rz0 = -S2(m_data, 60);
            else if (btn & SteamButton::LeftPadAndJoy)
                return m_rz0;
            else
                return 0;
        case ABS_HAT0X:
            if (btn & SteamButton::LeftPadClick)
                return int_sign(S2(m_data, 16) * 3 / 32768);
            else
                return 0;
        case ABS_HAT0Y:
            if (btn & SteamButton::LeftPadClick)
                return int_sign(-S2(m_data, 18) * 3 / 32768);
            else
                return 0;
        case ABS_HAT1X:
            if (btn & SteamButton::RightPadClick)
                return int_sign(S2(m_data, 20) * 3 / 32768);
            else
                return 0;
        case ABS_HAT1Y:
            if (btn & SteamButton::RightPadClick)
                return int_sign(-S2(m_data, 22) * 3 / 32768);
            else
                return 0;
        case ABS_TILT_X:
            return S2(m_data, 28);
        case ABS_TILT_Y:
            return S2(m_data, 30);
        case ABS_DISTANCE:
            return S2(m_data, 32);
        }
        break;
    case EV_KEY:
        switch (id.code)
        {
        case BTN_SELECT:
            return !!(btn & SteamButton::Menu);
        case BTN_START:
            return !!(btn & SteamButton::Escape);
        case BTN_MODE:
            return !!(btn & SteamButton::Steam);
        case BTN_BASE:
            return !!(btn & SteamButton::BtnX);
        case BTN_BASE2:
            return !!(btn & SteamButton::BtnY);
        case BTN_BASE3:
            return !!(btn & SteamButton::BtnA);
        case BTN_BASE4:
            return !!(btn & SteamButton::BtnB);
        case BTN_NORTH:
            return !!(btn & SteamButton::North);
        case BTN_SOUTH:
            return !!(btn & SteamButton::South);
        case BTN_EAST:
            return !!(btn & SteamButton::East);
        case BTN_WEST:
            return !!(btn & SteamButton::West);
        case BTN_TL:
            return !!(btn & SteamButton::LeftBumper);
        case BTN_TR:
            return !!(btn & SteamButton::RightBumper);
        case BTN_TL2:
            return !!(btn & SteamButton::LeftTriggerFull);
        case BTN_TR2:
            return !!(btn & SteamButton::RightTriggerFull);
        case BTN_GEAR_DOWN:
            return !!(btn & SteamButton::LeftBack);
        case BTN_GEAR_UP:
            return !!(btn & SteamButton::RightBack);
        case BTN_THUMBL:
            return !!(btn & SteamButton::Stick);
        /*
        case LeftPadClick
        case RightPadClick
        case LeftPadTouch
        case RightPadTouch
        case Unknown200000
        case Unknown800000
        */
        }
        break;
    }
    //int16_t ux = B2(buf, 58);
    //int16_t uy = B2(buf, 60);
    return 0;
}

input_absinfo InputDeviceSteam::get_absinfo(int code) const
{
    input_absinfo res {};
    res.value = get_value(ValueId(EV_ABS, code));
    switch (code)
    {
    case ABS_HAT0X:
    case ABS_HAT0Y:
    case ABS_HAT1X:
    case ABS_HAT1Y:
        res.minimum = -1;
        res.maximum = 1;
        break;
    default:
        res.minimum = -32768;
        res.maximum = 32767;
        break;
    }
    return res;
}

void InputDeviceSteam::flush()
{
}

std::shared_ptr<InputDevice> InputDeviceSteamCreate(const IniSection &ini)
{
    std::string devpath;

    udev *ud = udev_new();
    udev_enumerate *ude = udev_enumerate_new(ud);
    udev_enumerate_add_match_subsystem(ude, "usb");
    udev_enumerate_add_match_sysattr(ude, "interface", "Valve");
    udev_enumerate_scan_devices(ude);
    for (udev_list_entry *le = udev_enumerate_get_list_entry(ude); le && devpath.empty(); le = udev_list_entry_get_next(le))
    {
        const char *name = udev_list_entry_get_name(le);
        udev_device *parent = udev_device_new_from_syspath(ud, name);

        udev_enumerate *ude2 = udev_enumerate_new(ud);
        udev_enumerate_add_match_subsystem(ude2, "hidraw");
        udev_enumerate_add_match_parent(ude2, parent);
        udev_enumerate_scan_devices(ude2);
        for (udev_list_entry *le2 = udev_enumerate_get_list_entry(ude2); le2 && devpath.empty(); le2 = udev_list_entry_get_next(le2))
        {
            const char *name2 = udev_list_entry_get_name(le2);
            udev_device *dev = udev_device_new_from_syspath(ud, name2);
            devpath = udev_device_get_devnode(dev);
            udev_device_unref(dev);
        }
        udev_enumerate_unref(ude2);
        udev_device_unref(parent);
    }

    udev_enumerate_unref(ude);
    udev_unref(ud);

    FD fd { FD_open(devpath.c_str(), O_RDWR|O_CLOEXEC) };
    return std::make_shared<InputDeviceSteam>(ini, std::move(fd));
}

void InputDeviceSteam::send_cmd(const std::initializer_list<uint8_t> &data) const
{
    unsigned char feat[65] = {0};
    memcpy(feat+1, data.begin(), data.size());
    test(ioctl(fd(), HIDIOCSFEATURE(sizeof(feat)), feat), "HIDIOCSFEATURE");
}

