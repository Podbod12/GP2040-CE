---
title: LED Configuration
description: Documentation on configuring LEDs and lights in a GP2040-CE board configuration
---

# LED Configuration

:::note
This page covers only the LED and lighting options of a board configuration. For the folder structure, pin mapping and the other required options, see [Board Configuration](https://gp2040-ce.info/development/board-configuration).
:::

A board configuration can describe several kinds of lighting hardware. Each one is configured with `#define` entries in the board's `BoardConfig.h` file:

| Hardware | Used for | Configured in |
| -------- | -------- | ------------- |
| Addressable RGB LED chain (WS2812/NeoPixel style) | Button lighting, case lighting, and optionally the player and turbo indicators | [RGB LED Chain](#rgb-led-chain) |
| Simple GPIO LEDs | Player indicators, turbo indicator, reverse indicator, reactive LEDs | [GPIO LEDs](#gpio-leds) |
| Single on-board LED | Status indication (for example USB state) | [On-Board LED](#on-board-led) |

The examples on this page come from the [Haute42 COSMOX board configuration](https://github.com/OpenStickCommunity/GP2040-CE/tree/main/configs/Haute42COSMOX), which uses an RGB LED chain with one LED under each button.

## How BoardConfig.h LED Values Are Used

The values in `BoardConfig.h` are **defaults**. The firmware copies them into its stored configuration only for settings that have never been saved. After that, the saved settings are used, and users change them from the web configurator.

This has two consequences for board authors:

- Changing an LED value in `BoardConfig.h` has no effect on a device that has already been flashed and configured. Test LED changes on a device with a reset configuration.
- The light layout (see [Describing the Lights](#describing-the-lights)) is loaded once, then stored. Users can restore it later by applying a light preset from the web configurator.

## Quick Reference

Every LED option below is optional unless marked otherwise. If you leave an option out, the firmware uses the default shown.

### RGB LED Chain Options

| Name | Required? | Default | Description |
| ---- | --------- | ------- | ----------- |
| **BOARD_LEDS_PIN** | Yes, to use RGB LEDs | `-1` | GPIO pin connected to the data line of the LED chain. With `-1` or any invalid pin, the RGB LED system is disabled. |
| **LED_FORMAT** | No | `LED_FORMAT_GRB` | Color order of the LEDs. One of `LED_FORMAT_GRB`, `LED_FORMAT_RGB`, `LED_FORMAT_GRBW` or `LED_FORMAT_RGBW`. |
| **LED_BRIGHTNESS_MAXIMUM** | No | `128` | Brightness cap on a 0-255 scale. This is the value that the highest brightness step maps to. |
| **LEDS_BRIGHTNESS** | No | `-1` | Starting brightness step, from `0` to `10`. A value of `-1` (or any value outside that range) starts at step `10`. |
| **LEDS_AUTO_DISABLE_TIME** | No | `0` | Time in milliseconds before the lights turn off when idle. `0` disables the feature. |
| **LEDS_TURN_OFF_WHEN_SUSPENDED** | No | `0` | Set to `1` to turn the lights off while the USB host is suspended. |
| **LIGHT_DATA_NAME_DEFAULT** | Recommended | `""` | Name of the default light layout. Setting a non-empty name enables the light data described below. |
| **LIGHT_DATA_SIZE_DEFAULT** | With light data | `0` | Number of lights (rows) in `LIGHT_DATA_DEFAULT`. |
| **LIGHT_DATA_DEFAULT** | With light data | one empty entry | The light layout. See [Light Data Format](#light-data-format). |
| **LIGHT_DATA_NAME_*N***, **LIGHT_DATA_SIZE_*N***, **LIGHT_DATA_*N*** | No | empty | Additional light presets, where *N* is `1` to `7`. See [Additional Light Presets](#additional-light-presets). |

### Default Animation and Color Options

| Name | Default | Description |
| ---- | ------- | ----------- |
| **LEDS_BASE_ANIMATION_INDEX** | `AnimationNonPressedEffects::AnimationNonPressedEffects_EFFECT_RAINBOW_SYNCED` | Idle effect for button lights. |
| **LEDS_PRESSED_ANIMATION_INDEX** | `AnimationPressedEffects::AnimationPressedEffects_PRESSEDEFFECT_STATIC_COLOR` | Effect shown when a button is pressed. |
| **LEDS_CASE_ANIMATION_INDEX** | `AnimationNonPressedEffects::AnimationNonPressedEffects_EFFECT_RAINBOW_SYNCED` | Idle effect for case lights. |
| **LEDS_STATIC_COLOR_UNPRESSED** | `ColorIndexRed` | Color used by the static idle effect. |
| **LEDS_STATIC_COLOR_PRESSED** | `ColorIndexWhite` | Color used by the static pressed effect. |
| **LEDS_STATIC_COLOR_CASE** | `ColorIndexGreen` | Color used by case lights in static mode. |
| **LEDS_IDLE_SPECIAL_COLOR** | `ColorYellow` | Special color used by idle effects that take a second color. |
| **LEDS_PRESSED_SPECIAL_COLOR** | `ColorGreen` | Special color used by pressed effects that take a second color. |
| **LEDS_CASE_SPECIAL_COLOR** | `ColorBlue` | Special color used by case effects that take a second color. |
| **LEDS_IDLE_SPECIAL_COLOR_IS_RAINDOW**, **LEDS_PRESSED_SPECIAL_COLOR_IS_RAINDOW**, **LEDS_CASE_SPECIAL_COLOR_IS_RAINDOW** | `false` | Set to `true` to make the matching special color cycle through the rainbow. |

:::caution
The three `..._IS_RAINDOW` option names are spelled this way in the firmware source. Use the spelling shown, or the define is ignored.
:::

### GPIO LED Options

| Name | Default | Description |
| ---- | ------- | ----------- |
| **PLED_TYPE** | `PLED_TYPE_NONE` | Player indicator type: `PLED_TYPE_NONE`, `PLED_TYPE_PWM` (four GPIO LEDs) or `PLED_TYPE_RGB` (lights in the RGB chain). |
| **PLED1_PIN** to **PLED4_PIN** | `-1` | GPIO pin of each PWM player LED. |
| **PLED_COLOR** | `1` (white) | Default color index for RGB player lights. |
| **TURBO_LED_TYPE** | `PLED_TYPE_NONE` | Turbo indicator type: `PLED_TYPE_NONE`, `PLED_TYPE_PWM` or `PLED_TYPE_RGB`. |
| **TURBO_LED_PIN** | `-1` | GPIO pin of the turbo indicator LED. |
| **TURBO_LED_INDEX** | `-1` | Position of the turbo LED in the RGB chain. Used only by the [legacy setup](#legacy-led-index-options). |
| **TURBO_LED_COLOR** | `2` (red) | Color index of the turbo indicator. |
| **REVERSE_LED_PIN** | `-1` | GPIO pin of the LED used by the Reverse add-on. |
| **REACTIVE_LED_ENABLED** | `0` | Set to `1` to enable the reactive LED add-on. |
| **REACTIVE_LED_COUNT** | `10` | Number of reactive LEDs supported. |
| **REACTIVE_LED_DELAY** | `1` | Time between fade updates. |
| **REACTIVE_LED_MAX_BRIGHTNESS** | `255` | Maximum brightness of a reactive LED. |
| **REACTIVE_LED_FADE_INC** | `1` | Brightness change for each fade update. |

### On-Board LED Options

| Name | Default | Description |
| ---- | ------- | ----------- |
| **BOARD_LED_ENABLED** | `0` | Set to `1` to enable the on-board LED add-on. |
| **BOARD_LED_TYPE** | `ON_BOARD_LED_MODE_OFF` | One of `ON_BOARD_LED_MODE_OFF`, `ON_BOARD_LED_MODE_MODE_INDICATOR`, `ON_BOARD_LED_MODE_INPUT_TEST` or `ON_BOARD_LED_MODE_PS_AUTH`. |
| **BOARD_LED_PIN** | Board's default LED pin, or `25` | GPIO pin of the on-board LED. |

:::caution
`BOARD_LEDS_PIN` (with an S) is the data pin of the RGB LED chain. `BOARD_LED_PIN` (no S) is the single on-board LED. They are unrelated settings.
:::

## RGB LED Chain

Follow these steps to add RGB lighting to a new board configuration.

### Step 1: Set the Data Pin, Format and Brightness

Define the GPIO pin that drives the LED chain, the color order of your LEDs, and the brightness cap:

```cpp
#define BOARD_LEDS_PIN 28
#define LED_BRIGHTNESS_MAXIMUM 100
#define LED_FORMAT LED_FORMAT_GRB
```

Check your LED datasheet for the color order. Most WS2812-style LEDs use GRB. Use the `W` formats only for LEDs with a dedicated white channel.

The firmware always offers 10 brightness steps, so there is no step count to define. `LED_BRIGHTNESS_MAXIMUM` sets the absolute brightness of the top step, and the lower steps are even fractions of it. Keep the value at or above `10`, because lower values are raised to `10`.

### Step 2: Reserve the Data Pin

The data pin must not be used as a button. The Haute42 COSMOX configuration marks it as owned by an add-on, next to the other add-on pins:

```cpp
// Setting GPIO pins to assigned by add-on
#define GPIO_PIN_28 GpioAction::ASSIGNED_TO_ADDON
```

Use the same pin number you gave to `BOARD_LEDS_PIN`. Do the same for any GPIO pin used by PWM player LEDs, a turbo LED or a reverse LED.

### Step 3: Describe the Lights

The firmware needs to know how many LEDs the chain has, where each light sits on the chain, where it sits physically on the device, and what each light represents. You provide this as a table of lights.

#### Light Data Format

Each light is one row of six values, written as a flat comma-separated list:

```cpp
first led index, number of leds, x coordinate, y coordinate, GPIO pin or color index, light type
```

| Position | Name | Description |
| -------- | ---- | ----------- |
| 1 | First LED index | Position of the light's first LED in the chain. The first LED after the controller is `0`. |
| 2 | Number of LEDs | Number of consecutive LEDs that make up this light. Use `1` for one LED per button. |
| 3 | X coordinate | Horizontal position of the light on a grid that you define. |
| 4 | Y coordinate | Vertical position of the light on the same grid. |
| 5 | GPIO pin or color index | Meaning depends on the light type. For `LightType_ActionButton` and `LightType_Turbo`, the RP2040 GPIO pin number of the switch. For `LightType_Case` and the player light types, an index into the non-button color table (`0` to `31`). |
| 6 | Light type | One of the values in the following table. |

| Light Type | Description |
| ---------- | ----------- |
| `LightType::LightType_ActionButton` | A light under a button. It reacts when the GPIO pin in position 5 is pressed. |
| `LightType::LightType_Case` | A case or ambient light that is not tied to a button. |
| `LightType::LightType_Turbo` | The turbo indicator, when `TURBO_LED_TYPE` is `PLED_TYPE_RGB`. |
| `LightType::LightType_Player1Light` to `LightType::LightType_Player4Light` | Player indicators for players 1 to 4, when `PLED_TYPE` is `PLED_TYPE_RGB`. |

Then wrap the rows in three defines:

```cpp
#define LIGHT_DATA_SIZE_DEFAULT 16 // number of rows in the data below
#define LIGHT_DATA_DEFAULT \
0, 1, 0, 2, 5, LightType::LightType_ActionButton, \
1, 1, 2, 2, 3, LightType::LightType_ActionButton, \
/* ...one row per light... */ \
15, 1, 3, 6, 26, LightType::LightType_ActionButton
#define LIGHT_DATA_NAME_DEFAULT "Haute/Cosmox T16"
```

Note that the last row has no trailing backslash or comma.

To read a row, take the first row of the example, `0, 1, 0, 2, 5, LightType::LightType_ActionButton`. It describes a light that starts at LED `0` and uses `1` LED. It sits at grid position `(0, 2)`. It lights up when GPIO pin `5` is pressed, which in this board is the left direction (`GPIO_PIN_05` is `BUTTON_PRESS_LEFT`).

Follow these rules when you write the table:

- **List the LEDs in the physical order of the chain.** If your PCB wires the left direction LED first, its first LED index is `0`. The Haute42 COSMOX file keeps a comment that lists the order of the LEDs, which makes the table easy to review.
- **Set the size to the number of rows,** not the number of values. The web configurator uses `LIGHT_DATA_SIZE_DEFAULT` to read the table back.
- **Do not define the chain length.** The firmware works it out from the table: it is the highest `first LED index + number of LEDs`.
- **Use the physical GPIO number in position 5.** The firmware checks that GPIO pin directly to decide whether a light is pressed, so a switch wired to a duplicate pin, such as the extra buttons on the Haute42 COSMOX, can have its own light.
- **Stay within the limits.** Each value is stored in one byte (`0` to `255`), and a layout can contain at most 100 lights and 100 LEDs.
- **Choose any grid scale you like.** Coordinates are relative. The firmware subtracts the smallest X and Y value, so empty space on the left and top is removed. The coordinates matter for effects that move across the device, such as the left-to-right, top-to-bottom and circular chase effects, so place the lights where they sit on the device.

#### Additional Light Presets

A single board design sometimes ships in variants with different button counts or layouts. Haute42, for example, offers a 16-light and a 12-light variant. Define extra presets with `LIGHT_DATA_NAME_1`, `LIGHT_DATA_SIZE_1` and `LIGHT_DATA_1`, up to `LIGHT_DATA_NAME_7`:

```cpp
#define LIGHT_DATA_SIZE_1 12
#define LIGHT_DATA_1 \
0, 1, 0, 2, 5, LightType::LightType_ActionButton, \
/* ...one row per light... */ \
11, 1, 12, 3, 9, LightType::LightType_ActionButton
#define LIGHT_DATA_NAME_1 "Haute/Cosmox T12"
```

The default preset (`LIGHT_DATA_DEFAULT`) is applied the first time the board starts. Users can switch to any preset by name from the web configurator.

### Step 4: Choose the Default Animations and Colors

Pick the idle and pressed effects that new devices start with. The Haute42 COSMOX configuration starts with the synchronized rainbow:

```cpp
#define LEDS_BASE_ANIMATION_INDEX AnimationNonPressedEffects::AnimationNonPressedEffects_EFFECT_RAINBOW_SYNCED
```

`LEDS_BASE_ANIMATION_INDEX` and `LEDS_CASE_ANIMATION_INDEX` take one of the idle effects below. If both are set to the same effect, case lights follow the button effect.

Written as `AnimationNonPressedEffects::AnimationNonPressedEffects_EFFECT_<name>`, the available names are:

- `EFFECT_STATIC_COLOR`
- `EFFECT_RAINBOW_SYNCED`
- `EFFECT_RAINBOW_ROTATE`
- `EFFECT_CHASE_INDEX`
- `EFFECT_CHASE_SEQUENTIAL`
- `EFFECT_CHASE_CIRCLE_CLOCKWISE`
- `EFFECT_CHASE_CIRCLE_ANTICLOCKWISE`
- `EFFECT_CHASE_LEFT_TO_RIGHT`
- `EFFECT_CHASE_RIGHT_TO_LEFT`
- `EFFECT_CHASE_TOP_TO_BOTTOM`
- `EFFECT_CHASE_BOTTOM_TO_TOP`
- `EFFECT_CHASE_INDEX_PINGPONG`
- `EFFECT_CHASE_SEQUENTIAL_PINGPONG`
- `EFFECT_CHASE_CIRCLE_PINGPONG`
- `EFFECT_CHASE_HORIZONTAL_PINGPONG`
- `EFFECT_CHASE_VERTICAL_PINGPONG`
- `EFFECT_CHASE_RANDOM`
- `EFFECT_JIGGLESTATIC`
- `EFFECT_JIGGLETWOSTATICS`
- `EFFECT_RAIN`

`LEDS_PRESSED_ANIMATION_INDEX` takes one of these pressed effects, written as `AnimationPressedEffects::AnimationPressedEffects_<name>`:

- `PRESSEDEFFECT_STATIC_COLOR`
- `PRESSEDEFFECT_RANDOM`
- `PRESSEDEFFECT_JIGGLESTATIC`
- `PRESSEDEFFECT_JIGGLETWOSTATICS`
- `PRESSEDEFFECT_BURST`
- `PRESSEDEFFECT_BURST_SMALL`

The `LEDS_STATIC_COLOR_*` options take a color index. The built-in colors are `ColorIndexBlack` (`0`), `ColorIndexWhite`, `ColorIndexRed`, `ColorIndexOrange`, `ColorIndexYellow`, `ColorIndexLimeGreen`, `ColorIndexGreen`, `ColorIndexSeafoam`, `ColorIndexAqua`, `ColorIndexSkyBlue`, `ColorIndexBlue`, `ColorIndexPurple`, `ColorIndexPink` and `ColorIndexMagenta` (`13`). The `LEDS_*_SPECIAL_COLOR` options take the matching color name without `Index`, for example `ColorYellow`.

### Step 5: Add Case, Player and Turbo Lights (Optional)

Add these lights as extra rows in the same table, after the button lights.

**Case lights** use `LightType::LightType_Case`. Position 5 is an index from `0` to `30` into the non-button color table. New devices start with `LEDS_STATIC_COLOR_CASE` in all of these entries.

```cpp
// Two case lights on LEDs 16 and 17, using color index 0
16, 1, 0, 8, 0, LightType::LightType_Case, \
17, 1, 14, 8, 0, LightType::LightType_Case
```

**Player lights** need `PLED_TYPE` set to `PLED_TYPE_RGB`, plus one row for each player light, using `LightType_Player1Light` to `LightType_Player4Light`. Use color index `31` in position 5 to get the `PLED_COLOR` default:

```cpp
#define PLED_TYPE PLED_TYPE_RGB
// ...rows in LIGHT_DATA_DEFAULT:
18, 1, 0, 9, 31, LightType::LightType_Player1Light, \
19, 1, 1, 9, 31, LightType::LightType_Player2Light, \
20, 1, 2, 9, 31, LightType::LightType_Player3Light, \
21, 1, 3, 9, 31, LightType::LightType_Player4Light
```

**A turbo light** needs `TURBO_ENABLED`, `TURBO_LED_TYPE` set to `PLED_TYPE_RGB`, and one row of type `LightType_Turbo`. In position 5, use the pin number you set in `TURBO_LED_PIN`.

:::note
The Haute42 COSMOX configuration only uses `LightType_ActionButton` lights. The case, player and turbo examples above come from reading how the firmware handles those types, so test them on hardware before you ship them.
:::

## GPIO LEDs

Use these options when your board has LEDs wired straight to GPIO pins instead of (or as well as) an RGB chain.

### Player LEDs

Set `PLED_TYPE` to `PLED_TYPE_PWM` and give the GPIO pin of each LED. The pins are fed with PWM, so the LEDs can fade and blink in the patterns each console expects:

```cpp
#define PLED_TYPE PLED_TYPE_PWM
#define PLED1_PIN 12
#define PLED2_PIN 13
#define PLED3_PIN 14
#define PLED4_PIN 15
```

Also mark these pins with `GpioAction::ASSIGNED_TO_ADDON`, as described in [Step 2](#step-2-reserve-the-data-pin).

### Turbo LED

Set `TURBO_LED_PIN` to the GPIO pin of the LED. Setting a valid pin without a type defaults the type to `PLED_TYPE_PWM`. `TURBO_LED_COLOR` sets the color when the turbo light is part of the RGB chain.

### Reverse LED

Set `REVERSE_LED_PIN` to the GPIO pin of the LED that the Reverse add-on drives.

### Reactive LEDs

Set `REACTIVE_LED_ENABLED` to `1` to turn the add-on on by default. Which pins and actions the LEDs follow is stored configuration and is set from the web configurator, not from `BoardConfig.h`.

## On-Board LED

Use these options for the single status LED on the microcontroller board:

```cpp
#define BOARD_LED_ENABLED 1
#define BOARD_LED_TYPE ON_BOARD_LED_MODE_MODE_INDICATOR
```

| Mode | Behavior |
| ---- | -------- |
| `ON_BOARD_LED_MODE_OFF` | The LED is not used. |
| `ON_BOARD_LED_MODE_MODE_INDICATOR` | The LED reports the USB state and whether the device is in configuration mode. |
| `ON_BOARD_LED_MODE_INPUT_TEST` | The LED blinks when an input is pressed. |
| `ON_BOARD_LED_MODE_PS_AUTH` | The LED shows PlayStation authentication activity. |

If your board's LED is not on the Raspberry Pi Pico's default pin, also define `BOARD_LED_PIN`.

## Legacy LED Index Options

Older board configurations describe the RGB chain without a light table. Each button is given a single LED index, and the firmware builds the light positions from `BUTTON_LAYOUT`:

```cpp
#define LEDS_PER_PIXEL 1
#define LEDS_DPAD_LEFT   0
#define LEDS_DPAD_DOWN   1
#define LEDS_DPAD_RIGHT  2
#define LEDS_DPAD_UP     3
#define LEDS_BUTTON_B3   4
// ...and so on for B1-B4, L1, R1, L2, R2, S1, S2, L3, R3, A1 and A2
```

The firmware uses this setup only when `LIGHT_DATA_NAME_DEFAULT` is empty. For new board configurations, use [light data](#describing-the-lights) instead. It supports lights that are not tied to a standard button, such as extra buttons, case lights and per-device layouts, and it positions lights from real coordinates instead of an approximation.

In the legacy setup, in RGB mode `PLED1_PIN` to `PLED4_PIN` are read as LED positions in the chain rather than GPIO pins, and all four must be set. The `pledIndex` and `caseRGB` settings from older firmware, and the brightness step count, are no longer used.

## Checklist

Before you submit a board configuration with LEDs, check the following:

1. `BOARD_LEDS_PIN` is set to a valid GPIO pin, and that pin is marked `GpioAction::ASSIGNED_TO_ADDON`.
2. `LED_FORMAT` matches the color order of your LEDs. If red and green are swapped on the first test, change the format.
3. `LIGHT_DATA_NAME_DEFAULT` is not empty, and `LIGHT_DATA_SIZE_DEFAULT` equals the number of rows in `LIGHT_DATA_DEFAULT`.
4. Each button light uses the physical GPIO pin of its switch in position 5.
5. The first LED index values follow the physical order of the chain, and no two lights share an LED.
6. The last row of each light table has no trailing comma or backslash.
7. You tested on a device with a reset configuration, because saved settings override `BoardConfig.h` values.
8. The board's `README.md` includes the main pin mapping, as required. Consider adding the LED order to the README too, so that reviewers can check it.

For help with compiling and flashing a test build, see [Compile Firmware](https://gp2040-ce.info/development/compile-firmware).
