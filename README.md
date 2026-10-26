# Gaggia Bare Metal

A Gaggia Classic espresso machine rebuilt from the frame up, with a custom controller on an STM32F401
written bare-metal in C (STM32 HAL, no RTOS).

This is my graduation project at **Beetroot Academy**.

![Gaggia Bare Metal](stm_hardware/stm_gaggia_bareMetal/docs/image/32_box_for_display.jpg)

## What was done

- **Hydraulics.** Replaced the vibration pump with a PWM-driven BLDC gear pump and a flowmeter. Added a preheat
  boiler and a separate steam boiler. The original boiler stays on the group head as the brew boiler. Water is
  routed by Danfoss solenoid valves, and steam flow is set by a PWM-driven solenoid valve.
- **Mechanics.** Cut a side module from a donor machine's stainless body, TIG-welded stiffening ribs and boiler
  brackets inside it, and 3D-printed the pump brackets, display bezel and knob housing.
- **Electronics.** All three heaters are switched by SSRs. Three NTC sensors are read through ADC + DMA, and the
  steam boiler has a conductive water-level probe excited by a timer PWM signal.
- **Firmware (STM32).** Custom drivers for the NTC sensors (Steinhart–Hart), the water-level probe, the valves
  (fail-safe: all valves close in the error handler), the BLDC pump (PWM + tacho) and a UART log.
- **UI (ESP32).** A display and a haptic knob built from a gimbal BLDC motor and a magnetic encoder. The ESP32
  firmware is developed with the help of an AI coding assistant.

Full build log with photos: [English](stm_hardware/stm_gaggia_bareMetal/docs/build-log.md) |
[Українська](stm_hardware/stm_gaggia_bareMetal/docs/build-log.uk.md)

## Repository layout

```
stm_hardware/stm_gaggia_bareMetal/   STM32F401 firmware (CubeMX + CMake)
  Core/Src, Core/Inc                 application code and drivers
  docs/                              build log and photos
```

## Building the STM32 firmware

Requires `arm-none-eabi-gcc`, CMake and Ninja (for example from the STM32CubeCLT bundle).

```
cd stm_hardware/stm_gaggia_bareMetal
cmake --preset Debug
cmake --build --preset Debug
```

## Status

Work in progress: heater PID control and the ESP32 UI are still to do. See the status table at the end of the
build log.
