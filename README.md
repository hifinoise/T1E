# T1E - E-Paper Watch

> Time. For time's sake.

**The T1E is an open-source ePaper smartwatch that lasts for days, not hours. Minimal. Hackable. Yours.**

Because checking your phone for the time is so last decade. The T1E prioritises battery life over flashy animations. Built around an ESP32 microcontroller and a DS3231 RTC, it's designed for people who want their timepiece to actually keep time without requiring daily charging rituals.

---

## Features

- **ePaper Display:** Crystal clear in direct sunlight and uses minimal power.
- **4+ Days Battery Life:** Target 16+ days using intelligent partial refreshes. Charge less, live more.
- **Real-Time Clock:** DS3231 maintains accurate timekeeping even when the main MCU sleeps.
- **Deep Sleep Mode:** Intelligent power management. [Not Implemented on IDF Yet!]
- **Fully Hackable:** 100% open-source hardware, firmware, and mechanical designs.
- **Custom App Modes:** Various watch faces, a Pomodoro timer, a dice roller, and Conway's Game of Life built in.

## What's This Then?

This repository contains the full hardware designs, 3D-printable case models, and the **official, native ESP-IDF firmware** (built via PlatformIO) for the T1E E-Paper Watch. 

Every design choice has a clear reason: the ESP32 wakes up just long enough to refresh the screen, while the external RTC handles timekeeping at micro-amp power levels.

## Repository Contents

```
├── hardware/
│   ├── schematics/        # EasyEDA schematic files (and exported PDFs)
│   ├── pcb/               # PCB layout and gerbers
│   └── bom.csv            # Bill of materials
├── mechanical/
│   ├── case/              # 3D printable case files (STL/STEP)
│   └── assembly/          # Assembly instructions
├── firmware/              
│   ├── V2a/                # ESP-IDF Version (Experimental)
│   └── V1.1/               # Old Arduino-based code
└── docs/                  # Build guides and documentation
```

## Technical Specifications

- **MCU:** ESP32-C3 / ESP32-C6 (RISC-V, Dual-core, WiFi/BLE)
- **Display:** High-contrast ePaper display
- **RTC:** DS3231 with integrated crystal 
- **Power:** LiPo battery with BMS
- **Connectivity:** WiFi 2.4GHz & Bluetooth LE

## Getting Started

### Prerequisites

- PlatformIO
- 3D printer (for the case)
- Basic soldering skills
- Patience (surprisingly important for embedded development)

### Hardware Assembly

1. **PCB Assembly:** Solder components according to the BOM and assembly guide.
2. **Case Printing:** Print case parts using provided STL files.
3. **Final Assembly:** Mount PCB in case, connect battery, install display.

*(Detailed assembly instructions are in `/docs/assembly.md`)*

### Firmware Setup

1. Install [PlatformIO](https://platformio.org/).
2. Clone this repository.
3. Open the repository in PlatformIO.
4. Select your build environment (`env:t1e-c3` or `env:t1e-c6`).
5. Build and flash the firmware to the ESP32 via USB.
6. Set the initial time via the serial console or by configuring the WiFi sync credentials.

## Design Philosophy & Contributing

- **Battery Life First:** ePaper and an RTC combo for minimal power consumption.
- **Hackable by Design:** Built to be modified—not just used. Feel free to tweak the display update logic, design a new case, or implement a fresh display driver.
- **Contributing:** Pull requests are always welcome! Help is particularly appreciated with extreme power optimisation, new UI app modes, and alternative display drivers.

---

## License

- **Hardware:** CERN-OHL-W v2
- **Firmware:** MIT License
- **Mechanical:** CC BY-SA 4.0

## Support & About
For questions, bug reports, or general discussion, open an issue on GitHub.

> *"There is never time in the future in which we will work out our salvation. The challenge is in the moment; the time is always now."* — James Baldwin

*The T1E is a project by [Ampere Works](https://ampere.works/t1e) - part of a mission to make hardware design approachable without dumbing it down.*
