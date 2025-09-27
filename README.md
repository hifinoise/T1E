# T1E - E-Paper Watch

*Because checking your phone for the time is so last decade.*

![T1E Watch](https://ampere.works/images/t1e-hero.jpg)

## What's This Then?

The T1E is an open-source e-paper watch that prioritizes battery life over flashy animations. Built around the ESP32-C3 and a DS3231 RTC, it's designed for people who want their timepiece to actually keep time without requiring daily charging rituals.

This is an Ampere Works project - part of our mission to make hardware design approachable without dumbing it down. Because the best way to understand how things work is to build them yourself, preferably with clear documentation and minimal existential dread.

**Key Features:**
- E-paper display (because pixels that stay put are the best pixels)
- ESP32-C3 with WiFi/Bluetooth capability
- DS3231 RTC for precision timekeeping
- Passive buzzer for alarms that won't wake the neighbors
- Open-source hardware and firmware
- 3D printable case design
- Designed to be understood, not just assembled

## Repository Contents

```
├── hardware/
│   ├── schematics/          # EasyEDA schematic files (and exported PDFs for the brave)
│   ├── pcb/                 # PCB layout and gerbers 
│   └── bom.csv              # Bill of materials
├── mechanical/
│   ├── case/                # 3D printable case files (STL)
│   └── assembly/            # Assembly instructions
├── firmware/
│   ├── src/                 # Arduino/ESP-IDF source code
│   └── libraries/           # Required libraries
└── docs/                    # Documentation and build guide
```

## Technical Specifications

- **MCU**: ESP32-C3 (RISC-V, WiFi/BLE)
- **Display**: E-paper (model TBD - check hardware docs)
- **RTC**: DS3231 with integrated crystal
- **Power**: Li-Po battery with BMS
- **Case**: 3D printed PLA+/PETG
- **Connectivity**: WiFi 2.4GHz, Bluetooth LE

## Circuit Overview

The heart of the T1E is refreshingly straightforward:

- **ESP32-C3**: Handles display updates, wireless connectivity, and user interface
- **DS3231**: Maintains accurate time even when the main MCU is sleeping
- **I2C Bus**: Connects RTC and display to the ESP32-C3
- **Power Management**: Designed for extended battery life with deep sleep modes

The DS3231's backup battery ensures your watch doesn't become a fashionable wrist-mounted paperweight if the main battery dies.

## Getting Started

### Prerequisites

- Arduino IDE or ESP-IDF
- 3D printer (for the case)
- Basic soldering skills
- Patience (surprisingly important for embedded development)

### Hardware Assembly

1. **PCB Assembly**: Solder components according to the BOM and assembly guide
2. **Case Printing**: Print case parts using provided STL files  
3. **Final Assembly**: Mount PCB in case, connect battery, install display

*Note: Schematics designed in EasyEDA because sometimes convenience trumps tool snobbery. EasyEDA files included along with exported PDFs for those who prefer their schematics without the cloud dependency.*

Detailed assembly instructions are in `/docs/assembly.md`.

### Firmware Setup

1. Clone this repository
2. Install required libraries (listed in `/firmware/libraries/`)
3. Configure WiFi credentials in `config.h`
4. Flash firmware to ESP32-C3
5. Set initial time via serial console or WiFi sync

## Design Philosophy

The T1E embodies the Ampere Works approach to making hardware approachable:

- **Battery Life First**: E-paper display and RTC combo for minimal power consumption (because charging your watch daily is just smartphone anxiety in a different form factor)
- **Understandable Complexity**: Every design choice has a clear reason that doesn't require a graduate degree to grasp
- **Open Source**: Full hardware and software available under permissive licenses
- **Hackable by Design**: Built to be modified, improved, and learned from - not just used
- **Real-World Ready**: Designed for actual humans who want their projects to work, not just look impressive on social media

## Contributing

Found a bug? Have an improvement? Pull requests welcome!

Areas where help is particularly appreciated:
- Power optimization
- Alternative display drivers
- Case design improvements
- Documentation

## License

- Hardware: CERN-OHL-W v2
- Firmware: MIT License
- Mechanical: CC BY-SA 4.0

## Links

- **Project Page**: https://ampere.works/t1e
- **3D Model**: [Tinkercad Link](https://www.tinkercad.com/things/hXJ27IvFWx4-copy-of-t1e-v12-13-nov-2024-m2?sharecode=TuJhkoSxLSFucC7o3fV2j75Cr38F5Rp8J1hG9knSP6U)
- **Ampere Works**: https://ampere.works

## Support

For questions, bug reports, or general discussion, open an issue on GitHub or reach out via the Ampere Works website.

---

*"Time is an illusion. Lunchtime doubly so." - Douglas Adams*

*But accurate timekeeping? That's just good engineering.*