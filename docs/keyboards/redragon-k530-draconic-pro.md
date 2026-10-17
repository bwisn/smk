# Redragon K530 Draconic Pro

## Specs

- MCU: SH68F90 family
- USB VID: `0x258A`
- USB PID: `0x0049`
- Key matrix: 5 rows x 14 columns
- Backlight: RGB matrix with 19 PWM channels
- Wireless: BK3632, 2.4 GHz and two Bluetooth profiles
- Sleep wake: supported

## SMK Supported Features

- [x] Key Scan
- [x] RGB Matrix
- [x] USB
- [x] 2.4 GHz wireless
- [x] Bluetooth 1
- [x] Bluetooth 2
- [x] Sleep and wake
- [x] Pairing with `Fn2`+`Esc`

## Connection Modes

USB mode starts when a USB cable is present. The wireless slider selects one of
these profiles when the cable is not present:

| Slider position | Profile |
| --- | --- |
| `2.4G` | 2.4 GHz receiver |
| `B1` | Bluetooth 1 |
| `B2` | Bluetooth 2 |

The slider is debounced: a profile switch takes effect about a second after the
slider settles. The firmware keeps the current profile while the slider is
between positions, and picks the profile of the last switch when the board
boots with the slider in that dead combination.

Switching the profile does not touch the LED mode. The profile of the last
switch is stored in the settings, so it is still there after a reboot when the
slider sits in its dead combination.

## Default Keymap

The four keys in the lower right corner use tap and hold actions:

| Key | Tap | Hold |
| --- | --- | --- |
| Right Shift | Up | Right Shift |
| Fn | Left | Function layer |
| Fn2 | Down | Fn2 layer |
| Right Ctrl | Right | Right Ctrl |

`Caps Lock` uses Mod1 behavior:

- tap `Caps Lock` for Caps Lock
- hold `Caps Lock` and press `W`, `A`, `S`, or `D` for arrows

The function layer provides F1-F12, navigation keys, media keys, and the
other keys marked on the default keymap.

## Key events

The tick that reads the matrix turns changes into events, and the main loop
drains them. The queue keeps a press and release that happen between two
main-loop runs as two events, so fast input is not lost when the main loop is
busy.

## Backlight

The backlight uses the MCU PWM, one phase per PWM cycle. The matrix scan runs
in the LED frame-wrap phase. The scan must be short because it takes the
backlight column pins for its duration: a 110 us scan in a 4 ms frame keeps the
backlight dark about 2% of the time.

The `Caps Lock` key shows the lock state in solid green, over the effect. On
USB the state is the host's LED report. The radio module has no status
readback: its data line is wired, but it only echoes bytes clocked out (a read
after a status query returns the query's trailing bytes, not a status frame).
On radio, the key follows the Caps Lock key the firmware sends; the host
toggles Caps on every press.

## Fn2 Layer

Hold `Fn2` (the `Down` key in the lower right corner):

| Key | Action |
| --- | --- |
| `Esc` | pairing on the current profile |
| `-` | brightness down |
| `=` | brightness up |
| `0` | LEDs on/off |
| `8` | next effect speed |
| `9` | next LED effect |

`Fn` and `Fn2` are active as soon as they are down. The layer does not wait
for a timeout, so the second key of a combo is never typed instead. A tap of
`Fn2` still sends the arrow, and a long hold sends `Menu`.

Brightness has six levels (0 = off, 5 = full) and scales the colour directly:
`duty = colour * level`. Effect speed has five levels: `0` is fastest, higher
values are slower. The LED effect, speed and brightness are stored in settings
and survive a reboot.

## Build

```sh
meson compile -C build redragon-k530-draconic-pro_default_smk.hex
```
