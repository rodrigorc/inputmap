# The SteamController

The SteamController is a USB HID device, but unfortunately the Linux kernel does not recognize it as a proper input device (yet).
This is because, although it is a HID device, it does not send the usual HID codes. In this document I will document how I think this device works.

## Connection

The SteamController can be connected to the PC, either by using a USB cable or using the wireless adaptor, provided by the manufacturer.

When the device is connected with a cable it shows as device `28de:1102`, which contains 3 USB interfaces:

 1. A virtual keyboard, to send emulated key events, for keys `ENTER`, `ESC`, etc.
 2. A virtual mouse, by default the right pad sends mouse events through this interface.
 3. A vendor defined interface that implements the vendor defined protocol. From now on I will call this interface and protocol _Steam.

When the wireless adaptor is connected it shows as device `28de:1142`, which contains 5 USB interfaces:

 * A virtual keyboard, like the wired one (no virtual mouse, sorry).
 * 4 Steam interfaces, to be able to connect up to 4 controller wirelessly.

## The hidraw device

The Steam interface does implement the HID protocol, but it does not define any standard input, so the Linux kernel is not able to create an input device for it.
It creates a hidraw device, instead (`/dev/hidraw*`).

The HID descriptor for this device is as follows:

    06 00 FF 09 01 A1 01 15 00 26 FF 00 75 08 95 40 09 01 81 02 95 40 09 01 91 02 95 40 09 01 B1 02 C0

That, if I read the HID tables correctly, is decoded as:

    1: USAGE_PAGE = 00 FF
    2: USAGE = 01
    0: BEGIN_COLLECTION = 01
        1: LOGICAL_MIN = 00
        1: LOGICAL_MAX = FF 00
        1: REPORT_SIZE = 08
        1: REPORT_COUNT = 40
        2: USAGE = 01
        0: INPUT = 02
        1: REPORT_COUNT = 40
        2: USAGE = 01
        0: OUTPUT = 02
        1: REPORT_COUNT = 40
        2: USAGE = 01
        0: FEATURE = 02
    0: END_COLLECTION

That basically means that it generates input reports of 64 vendor defined bytes and that it accepts output reports of 64 vendor defined bytes.

It also defines a _feature_ of 64 bytes. That is a kind of special command that can be sent or received offside the usual events.

What this means is that if you read the Steam `/dev/hidraw*` device you will get blocks of 64 bytes each, with vendor specific meaning.

## The input report

Each block of data read from the Steam hidraw device contains 64 bytes of data. I have managed to make sense of most of them.

In the following tables I will describe what I know.

The basic structure of a data frame is as follows, all multibyte values are little-endian:

| Byte index     | Name        |
| -------------: |-------------|
|  0-1           | Version     |
|  2             | Type of Message |
|  3             | Size        \
|  4-7           | Sequence Number |

These values are:

 * Version: Probably the version of the protocol, currently it is always 0x0001.
 * Type of Message (ToM): I have only seen 3 types of message: 0x01=input data; 0x03=wireless connect/disconnect; 0x04=battery status.
 * Size: The number of useful bytes of this message, counting from the next one. It depends on the type of message, if ToM=0x01, Size=0x3C; if ToM=0x03, Size=0x01; if ToM=0x04, Size=0x0B.
 * Sequence Number: a 32-bit number that is incremented for each message. It is useful to know if you missed any message, for example. It is not available when ToM=0x03 (wireless connect/disconnect).

The rest of the bytes depend on the value of ToM.

### The _wireless connect/disconnect_ report

When ToM equals 0x03, that means that a wireless device has been connected/disconnected to this Steam interface. The wired device does not generate this report, it just appears or vanishes directly, but the 4 wireless devices exist whenever the wireless adaptor is connected.

The data bytes of this report are:

| Byte index     | Name        |
| -------------: |-------------|
|  4             | Connection  |

And the description:

 * Connection: 0x01 means device disconnected, 0x02 means device connected.

### The _battery status_ report

When ToM equals 0x04, it is a status report from the device. It works as a ping, reminding you that the device still works, and as a battery status.

The data bytes of this report are:

| Byte index     | Name        |
| -------------: |-------------|
|  8-11          | Unknown, always 0 |
|  12-13         | Volts       |
|  14            | Battery     |

And the description:

 * Volts: The voltage from the batteries, in millivolts. If it is wired, it'll measure the USB voltage (around 5000), if it is wireless, it is the voltage of the batteries.
 * Battery: The battery level as a percentage, from 0 to 100 (0x64). A wired device will always have 100% battery.

### The _input_ report

When ToM equals 0x01 it is the most interesting one, it contains the input report with the status of all the buttons and axes of the device. If the device is connected wirelessly, then some of the fields are always 0, these are marked in the table with an asterisk (\*).

The data fields in this report are:

| Byte index     | Name        |
| -------------: |-------------|
|  8-10          | Buttons     |
|  11            | Left trigger value |
|  12            | Right trigger value |
|  13-15         | Unknown, always 0, probably for future buttons so that you can read 8-15 as a 64-bit value |
|  16-17         | X coord. |
|  18-19         | Y coord. |
|  20-21         | Right pad X coord. |
|  22-23         | Right pad Y coord. |
|  24-25         | \* Left trigger value (16-bit) |
|  26-27         | \* Right trigger value (16-bit) |
|  28-29         | \* Accelerometer X value |
|  30-31         | \* Accelerometer Y value |
|  32-33         | \* Accelerometer Z value |
|  34-35         | Gyroscopic X value |
|  36-37         | Gyroscopic Y value |
|  38-39         | Gyroscopic Z value |
|  40-41         | Quaternion W value |
|  42-43         | Quaternion X value |
|  44-45         | Quaternion Y value |
|  46-47         | Quaternion Z value |
|  48-49         | Unknown, always 0 |
|  50-51         | \* Uncalibrated left trigger |
|  52-53         | \* Uncalibrated right trigger |
|  54-55         | \* Uncalibrated X joystick |
|  56-57         | \* Uncalibrated Y joystick |
|  58-59         | \* Left pad X coord. |
|  60-61         | \* Left pad Y coord. |
|  62-63         | \* Voltage         |


Each bit from the 24 bit Buttons value is a button, 0 is released, 1 is pressed:

 * Bit 0: Right trigger fully pressed.
 * Bit 1: Left trigger fully pressed.
 * Bit 2: Right bumper (the button above the trigger).
 * Bit 3: Left bumber.
 * Bit 4: Button Y.
 * Bit 5: Button B.
 * Bit 6: Button X.
 * Bit 7: Button A.
 * Bit 8: North, the left pad has been clicked in the upper side.
 * Bit 9: East, the left pad has been clicked in the right side.
 * Bit 10: West, the left pad has been clicked in the left side.
 * Bit 11: South, the left pad has been clicked in the bottom side.
 * Bit 12: Menu, the button with a left arrow next to the Steam button.
 * Bit 13: Steam button.
 * Bit 14: Escape, the button with a right arrow next to the Steam button.
 * Bit 15: Left back pedal.
 * Bit 16: Right back pedal.
 * Bit 17: Left pad clicked.
 * Bit 18: Right pad clicked.
 * Bit 19: Left pad touched.
 * Bit 20: Right pad touched.
 * Bit 21: Unknown.
 * Bit 22: Joystick pressed.
 * Bit 23: LPadAndJoy.

The values _X coord._ and _Y coord._ deserve a special explanation. Read as-is, they represent the position of the joystick or a touch in the left pad, whatever the user is doing. The idea is that these two controllers should be mostly equivalent. If you want to know which one is being used, just check for the _Left pad touched_ button: if it is on, then it is a left pad event, if not it is a joystick event.

But what if you want to manage input from the left pad and the joystick at the same time? Well, then you need to take into account the _Left pad touched_ and the _LPadAndJoy_ buttons:

| Left pad touched | LPadAndJoy  | Meaning of X/Y coord. | Is left pad touched | Is joystick moved |
| ---------------- | ----------- | --------------------- | ------------------- | ----------------- |
|  0               | 0           | Joystick position     | No                  | Maybe, if <> 0    |
|  1               | 0           | Left pad coord.       | Yes                 | No                |
|  0               | 1           | Joystick position     | Yes                 | Yes               |
|  1               | 1           | Left pad position     | Yes                 | Yes               |

That is, when both controls are used at the same time, you will get alternating events with the two last rows of this table. You can get the left pad coordinates unconditionally from the _Left pad X/Y coord_ values, but those unfortunately are not available with the wireless device.



## Feature reports

The Steam hidraw device also accepts commands in the form of a feature report. It is sent by using a `ioctl()` call with the `HIDIOCSFEATURE` value. Some commands have a reply, that can be read with the `HIDIOCGFEATURE` `ioctl()`. See the source code for details of how they are sent and received.

All the commands are 64 bytes in length, although in practice most commands use only a few of them. The general structure of the command is:

| Byte index     | Name        |
| -------------: |-------------|
|  0             | Command code  |
|  1             | Length of the command data |
|  2-            | Data of the command |

The following commands are currently known:

 * 0x85: Enables the emulation of the virtual keyboard when the buttons are pressed.
 * 0x81: Disables the emulation of the virtual keyboard.
 * 0x8E: Resets the device to the default emulation values.
 * 0xAE 0x15 0x01: Gets the serial number. The reply will contain the serial number in positions 3-13.
 * 0xAE 0x15 0x00: Gets the board information. The reply will contain the board number in positions 3-13.
 * 0x83: Version information. It will return 30 bytes, or 6 structures of 5 bytes each. The first byte of the structure identifies the value, the other 4 are the value itself. These are the known values:
  - 0x00: Unknown, maybe firmware version.
  - 0x01: Product id. The constant 0x1102 is the Steam Controller.
  - 0x02: Capabilities, always 0x03.
  - 0x04: firmware build time.
  - 0x05: some unknown time.
  - 0x0A: bootloader build time.
 * 0xBA: returns some unknown information.
 * 0x87 0x03 R Blow Bhigh: Writes 16-bit value B into register R. Several writes can be combined into a single command, such as `0x87 0x06 R1 XL XH R2 YL YH`. The known registers are:
  - 0x30: Accelerometer: 0x00=disabled, 0x14=enabled.
  - 0x08: Write 0x07 to disable mouse keys emulation.
  - 0x07: Write 0x07 to disable mouse cursor emulation.
  - 0x18: Write 0x00 to remove the margin of the right pad. By default touching the rim of the right pad does not generate events.
 * 0x8F: Send haptic feedback. See below.


## Haptic feedback

The little motors in the device are activated with the command 0x8F, as described in the previous section. This command takes 7 or 8 bytes of data, so the command is as follows:

| Byte index     | Name        |
| -------------: |-------------|
|  0             | 0x8F (Command) |
|  1             | 0x07 or 0x08 (Length)  |
|  2             | Side |
|  3-4           | Time On |
|  5-6           | Time Off |
|  7-8           | Count |
|  9             | Unknown |

The Side value is:

 * 0: right motor.
 * 1: left motor.

The rest of the values describe the shape of the signal that is sent to the motor. As far as I know it sends a square signal to the motor that makes it vibrate. For each cicle of the signal you can control the duration of the ON and the OFF sections. And also the number of cycles in the signal. When all these cycles has been sent, the motors stops by itself. If you send another signal to the same motor, the previous one is forgotten. The Time On/Off values are measured in microseconds.

So if you want to send a signal of _freq_ Hz with a duty cycle of _duty_% for _duration_ seconds, you will send, you could use the following code to compute the actual parameters of the command:

    int period = 1000000 / freq; //in us
    int time_on = period * duty / 100;
    int time_off = period - time_on;
    int count = duration / period;

