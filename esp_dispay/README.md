# Gaggia Bare Metal: UI (WT32-SC01 Plus)

ESP32-S3 + ST7796 480x320 (8-bit parallel) + FT6336 touch.
PlatformIO, Arduino, LovyanGFX 0.4.18, LVGL 8.4. UI code is plain C.

## Build (PlatformIO)

    pio run                          # env "demo": hardware + machine simulator
    pio run -e demo -t upload        # flash; press RST after upload if it doesn't restart
    pio device monitor               # '#' event log + telemetry CSV
    pio run -e release -t upload     # GAGGIA_SIM=0, real STM32 link (link layer is TODO)
    pio run -e native_sim -t exec    # PC build, frames -> sim_out/
    python3 sim/raw2png.py sim_out

Platform: pioarduino 55.03.312 (Arduino core 3.3.x, ESP-IDF 5.5), pinned in
platformio.ini; all its packages download from GitHub. Libraries: LVGL 8.4.0,
LovyanGFX >= 1.2.33 (older 1.x does not compile on ESP-IDF 5.5).
`demo_arduino2` keeps the official espressif32@6.9.0 / Arduino 2.0.x as a
fallback (not the tested setup).

Board: boards/wt32-sc01-plus-8mb.json (ESP32-S3, 8 MB flash, QSPI PSRAM,
native USB CDC). If esptool reports 16 MB on upload, switch flash_size /
maximum_size / partitions to the 16 MB variants.

Tested build (demo): RAM 11.4 % (37 KB static), flash 26 % (878 KB of 3.3 MB app).

## Layout

    src/main.cpp          board: LovyanGFX, LVGL port (2x DMA buffers), NVS, ui_hal_*
    src/lv_conf.h         fonts 12-48, LV_MEM_CUSTOM=1 (malloc)
    src/app/gaggia_model  telemetry, settings, profiles, commands + simulator (GAGGIA_SIM=1)
    src/ui/ui.c           screen manager, status bar, nav, fault banner, standby, dimming
    src/ui/ui_widgets.c   card, gauge, duty bar, number editor, confirm, toast, settings rows
    src/ui/scr_*.c        home, brew, manual, profiles, steam, water, settings
    sim/                  headless PC build: runs a scripted scenario, dumps screenshots

UI never touches UART: it reads `gaggia_tm()` and calls `gaggia_cmd_*()`.
STM32 link goes into gaggia_model.c (`TODO(link)` marks), then `-DGAGGIA_SIM=0`.

## Actuator telemetry (from STM32)

    pump_pct          BLDC pump PWM duty, %      -> Brew header, Manual, Water
    steam_valve_pct   steam valve PWM duty, %    -> Steam (actual vs requested arc)
    manual / manual_bar                          -> Manual mode

## PC simulator

    pio run -e native_sim -t exec && python3 sim/raw2png.py sim_out
    # or: cd sim && make LVGL=/path/to/lvgl-8.4 && ./gaggia_sim ../sim_out

## Smart knob

    FOC task --knob_input_report(pct)--> knob_input --gaggia_tick()--> steam_power -> STM32 -> valve PWM
    touch drag --gaggia_cmd_steam_power()--> knob_port_set_target(pct) --> FOC moves the rotor

`knob_input_report()` is thread-safe (call it from the FOC task on any core).
`knob_port_set_target()` is a weak no-op; implement it in the knob driver.
With `knob_auto_steam` (Settings > Boiler) opening the knob past 3 % starts
steam and closing it stops, like the original handle.

## Debug log / telemetry

Screen: Settings > Service > Telemetry log, or long-press the status text in the top bar.
Left: all telemetry fields live. Right: event log (status/phase changes, every TX
command, faults set/cleared, knob open/close, shot summary, link up/lost).

USB serial (115200), same stream:

    # I 46.140 tx brew start                 <- events, prefixed with '#'
    # I 74.755 shot done 28.6 s, 36 ml, peak 9.4 bar
    t_ms,status,phase,brew_c,...             <- CSV header once per enable
    189185,2,0,93.02,135.0,80.0,0.00,...     <- rows at 1 / 5 / 20 Hz (CSV button)

SerialPlot: ignore lines starting with '#', comma separated, header on.
Link stats: rx frames + fps, CRC errors (call gaggia_link_crc_error() from the
parser), timeouts (> GAGGIA_LINK_TIMEOUT_MS without a frame -> FAULT_LINK_LOST).

## STM32 link

See docs/STM32_LINK.md. Shared protocol in src/proto (copy to the STM32
project as is), ESP side in src/app/gaggia_link.c, active in `-e release`.
UART1: IO10 RX / IO11 TX (check your header), 921600 8N1.
Host tests: sim/test_proto.c, sim/test_link.c (commands in file headers).

## Smart knob modes

Manual screen: knob = pressure setpoint. Everywhere else: knob = steam valve
(opening past 3 % starts steam, closing stops). Settings > Machine >
"Touch moves arcs": on in demo, off in release (knob only).
