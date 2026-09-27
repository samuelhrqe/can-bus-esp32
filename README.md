# CAN Bus on the ESP32

Source code for a series of blog articles exploring CAN bus communication on the ESP32, using different embedded frameworks.

Each article walks through building and validating a CAN node from scratch, hardware wiring, driver configuration, message transmission, logic analyzer validation, and bus error monitoring/recovery. This repository holds the companion code for every article in the series.

## Frameworks covered

- [ x ] **ESP-IDF** — native Espressif framework, using the TWAI (Two-Wire Automotive Interface) event-driven driver
- [  ] **Zephyr RTOS** — planned for a future article, using Zephyr's native CAN API

## Hardware used

- ESP32-D0WDQ6 (rev v1.0), 38-pin dev board
- CAN transceiver: TJA1050 / TJA1051
- Jumper wires
- 1 kΩ and 2 kΩ resistors (voltage divider on the RXD line, required for 5V transceivers)
- Generic 24 MHz, 8-channel USB logic analyzer (validated with Saleae Logic 2)

## Repository structure

```bash
can-bus-esp32/
├── esp-idf/
│   └── twai-sender/       # first article: node setup, TX tasks, error monitoring
├── zephyr/
│   └── (coming soon)
└── README.md
```

Each subfolder is a self-contained project you can open, build and flash independently — check the README inside each one for framework-specific build instructions.

## Articles

| # | Title | Framework | Link |
|---|-------|-----------|------|
| 1 | ESP32 and the CAN bus: building a node with ESP-IDF | ESP-IDF | _coming soon_ |
| 2 | ESP32 and the CAN bus: building a node with Zephyr RTOS | Zephyr | _coming soon_ |

## What you'll find in each project

- CAN node configuration using the target framework's native driver
- Periodic message transmission with multiple concurrent tasks
- Self-test mode for validation without a second physical node
- Logic analyzer setup and captured frame decoding
- Bus error monitoring (`ACTIVE` → `WARNING` → `PASSIVE` → `BUS_OFF`) and automatic recovery

## Contributing / discussion

Found a different approach, used another transceiver, or validated this on a different framework? Feel free to open an issue or a discussion — feedback and alternative implementations are welcome.
