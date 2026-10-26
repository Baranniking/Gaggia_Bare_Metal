# Gaggia Bare Metal — Build Log

[Українська](build-log.uk.md) | **English**

A Gaggia Classic rebuilt from the frame up: new hydraulics, a BLDC pump with a flowmeter, a separate steam boiler,
solenoid valve distribution, SSR-driven heaters and a custom STM32 controller written bare-metal (STM32 HAL, no RTOS).

Graduation project at **Beetroot Academy**.

> Photos are in [`docs/image`](image). Steps follow the photo numbering.

---

## 1. Starting point — stock Gaggia Classic

The donor for the whole project: a regular Gaggia Classic with a single boiler, a vibration pump and a mechanical steam knob.

![Original machine](image/1_original.jpg)

Inside it had already been modded once: an off-the-shelf PID temperature controller sat on top of the stock wiring.
All of this was removed.

![Original hardware](image/2_original_hardware.jpg)

| Stock boiler | Mechanical steam valve | Vibration pump |
|---|---|---|
| ![](image/3_original_boiler.jpg) | ![](image/4_original_steam_knob.jpg) | ![](image/5_original_vibration_pump.jpg) |

- **Boiler** — single aluminium boiler with bimetal thermostats, originally shared between brew and steam.
  It stays on the group head and now works as the brew boiler only.
- **Steam knob** — manual needle valve on a copper pipe; replaced with an electrically controlled valve.
- **Pump** — Defond Phoenix-50 vibration pump (230 V, 53 W, 1.8 MPa, 1.5 min ON / 1 min OFF duty).
  No pressure/flow control possible — replaced with a BLDC pump.

---

## 2. New hydraulics

All the new hydraulic parts laid out before integration: two cylindrical boilers (preheat and steam), Danfoss EV210A solenoid valves,
safety/check valves and PTFE tubing.

![Hydraulic hardware](image/6_intergation_hydraulic_hardware.jpg)

### BLDC pump + flowmeter

The vibration pump is replaced with a BLDC gear pump. A flowmeter sits directly on the pump outlet, and the
assembly is held together by 3D-printed brackets (blue clamp, yellow base).

| | |
|---|---|
| ![](image/7_intergation_bldc_pump.jpg) | ![](image/8_bldc_pump_with_flowmeter.jpg) |

The pump speed is set by PWM from the STM32 (TIM4, `PB6`), with a tacho input for RPM feedback — see
[`bldc_pump.c`](../Core/Src/bldc_pump.c).

### PWM steam valve

The manual steam knob is replaced with an ODE 21AP1KC4V30 solenoid valve (G 1/8, DN 3.0 mm, 0–3.5 bar, DC coil).
Driving the coil with PWM gives adjustable steam flow instead of plain on/off.

![PWM steam valve](image/9_pwm_steam_valve.jpg)

Tuning the PWM for the valve: [video](image/10_settings_pwm_valve.MP4)

---

## 3. Extra housing module

The Gaggia body has no room for a second boiler, valves and electronics, so a side module was added.

The module was cut from the stainless body of a donor espresso machine (cut line marked in red):

| Donor unit | Cut module |
|---|---|
| ![](image/11_donor_unit.jpg) | ![](image/12_module%20housing.jpg) |

Stiffening ribs and internal brackets were TIG-welded in to make the thin sheet rigid and to carry the boilers:

| | |
|---|---|
| ![](image/13_stiffening%20ribs.jpg) | ![](image/14_stiffening%20ribs.jpg) |

Test-fitting the module against the Gaggia frame:

| | |
|---|---|
| ![](image/15_module%20fitting.jpg) | ![](image/16_module%20fitting.jpg) |
| ![](image/17_module%20fitting.jpg) | ![](image/18_module%20fitting.jpg) |

---

## 4. Paint

Custom airbrushed livery inspired by the Lancia Rally 037 (W. Röhrl / K. Geistdörfer, #1):
white Gaggia body, black module, Martini-style stripes.

![Custom paint job](image/19_custom%20paint%20job.jpg)

---

## 5. Distribution valves

Danfoss EV210A (NC) solenoid valves with brass tees form the water distribution block:
brew group, steam boiler fill and hot water.

| | |
|---|---|
| ![](image/20_distribution%20valves.jpg) | ![](image/21_distribution%20valves.jpg) |

The valve block is closed with a cover; the tubing exits through a slot.

![Valve box](image/22_box_distribution%20valves.jpg)

In firmware every valve is a GPIO output (`PA3` fill, `PA4` brew, `PA5` hot water). All valves are closed on
start-up and in `Error_Handler()` — see [`valves.c`](../Core/Src/valves.c).

---

## 6. Boiler integration

The machine now has three boilers:

- **Preheat boiler** (cylindrical, in the module) — heats the incoming water before it reaches the group.
- **Steam boiler** (cylindrical, in the module) — dedicated to steam, with its own fill valve and level probe.
- **Brew boiler** — the original Gaggia boiler on the group head.

The cylindrical boilers are mounted vertically in the module on the welded brackets, plumbed with PTFE tubing in spiral wrap.

| | |
|---|---|
| ![](image/23_boiler_integration.jpg) | ![](image/24_boiler_integration.jpg) |
| ![](image/25_boiler_integration.jpg) | ![](image/26_boiler_integration.jpg) |

Each heater is switched by an SSR (`PA0` preheat, `PA1` steam, `PA2` brew). Temperature is read by three NTC
thermistors on ADC1 + DMA (Steinhart–Hart, 1.5 kΩ pull-up) — see [`ntc_sensor.c`](../Core/Src/ntc_sensor.c).

The steam boiler water level is measured with a conductive probe: the probe is excited by a PWM signal from TIM1
(`PA8`) and the returned amplitude is read by the ADC (`PA6`). Low amplitude = probe in water (WET), high = DRY —
see [`water_level_steam.c`](../Core/Src/water_level_steam.c).

### SSR

SSRs are mounted on a heatsink plate inside the module, next to the mains inlet.

| | |
|---|---|
| ![](image/27_ssr_intergration.jpg) | ![](image/28_ssr_intergration.jpg) |

### Assembled module

| Rear (IEC mains inlet) | Side |
|---|---|
| ![](image/29_rear%20view.jpg) | ![](image/30_side%20view.jpg) |

---

## 7. User interface

The original switch panel is removed. In its place: a display in a 3D-printed "Bare Metal" bezel and a haptic
knob on the side where the steam knob used to be.

| Cable gland for the knob | Display box |
|---|---|
| ![](image/31_%20haptic%20knop.jpg) | ![](image/32_box_for_display.jpg) |

### Haptic knob

The knob is a small gimbal BLDC motor with a magnetic angle encoder, in a 3D-printed knurled housing.
The motor produces detents / end stops in software, so the same knob can be used for any menu.

| | |
|---|---|
| ![](image/33_3D_printing_of_a_tactile_pen%20housing.jpg) | ![](image/34_3D_printing_of_a_tactile_pen%20housing.jpg) |

| Housing with magnet | Encoder board |
|---|---|
| ![](image/35_3D_printing_of_a_tactile_pen%20housing.jpg) | ![](image/36_encoder.jpg) |

| Gimbal BLDC | Bench test |
|---|---|
| ![](image/37_bldc.jpg) | ![](image/38_testing.jpg) |

Bench test: ESP32 dev board, BLDC driver, buck converter and the motor on a breadboard.

The display and the haptic knob run on a separate ESP32. The STM32 stays responsible for everything that touches
water and mains (heaters, valves, pump, sensors); the ESP32 only handles the user interface.

---

## Firmware status

MCU: STM32F401 (CubeMX + CMake, `arm-none-eabi-gcc`).

The STM32 firmware is written by hand. The ESP32 UI firmware (display + haptic knob) is developed with the help
of an AI coding assistant.

| Module | File | Status |
|---|---|---|
| ADC1 + DMA (NTC ×3, water probe) | `adc_dma.c` | done |
| NTC temperature (Steinhart–Hart) | `ntc_sensor.c` | done, needs open/short-circuit check by temperature |
| Steam boiler water level probe | `water_level_steam.c` | done, threshold being tuned |
| Solenoid valves | `valves.c` | done |
| UART log | `log.c` | done |
| BLDC pump (PWM + tacho) | `bldc_pump.c` | in progress |
| Heater control (PID + SSR) | — | todo |
| Display / haptic knob (ESP32) | separate project | todo |
