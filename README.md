Example:
![screen](https://github.com/intellar/ImpulsePlay/blob/78df569e8fd265b34dfc83c6fa7ff6fe569a02fc/impulseplay.png)


# Intellar ImpulsePlay

Portable ESP32-S3 companion with tilt-driven **impulse physics**, a match-3 ball game, and IMU-animated eyes.

https://youtube.com/shorts/vLsGOB6HffI


**Repository:** https://github.com/intellar/ImpulsePlay

```bash
git clone https://github.com/intellar/ImpulsePlay.git
```

Blog: [intellar.ca](https://www.intellar.ca) — related projects: [Animated eye OLED](https://www.intellar.ca/blog/animated-eye-oled), [Intellar Engine](https://www.intellar.ca/blog/intellar-engine-satellite-boards).

## Features

- **Ball mode** — up to 12 balls, gravity and inertia from a BMI160 IMU, touch grab/throw, color matching, explosive bombs, high score on LittleFS
- **impulse_physics** — fixed-timestep 2D impulse collisions, wall bounces, squash & stretch on impact
- **Eyes mode** — cartoon eyes with tilt, squash, blink, and idle animations (experimental; on-screen mode toggle is disabled in firmware)
- **UI** — battery voltage, FPS overlay, in-game menu

## Hardware

| Component | Notes |
|-----------|--------|
| MCU | ESP32-S3 Super Mini (4 MB flash, PSRAM) |
| Display | ILI9341 240×320, SPI + XPT2046 touch |
| IMU | BMI160 on I2C |
| Power | LiPo with voltage divider on ADC |

### Pinout (default build)

| GPIO | Function |
|------|----------|
| 11 | TFT MOSI |
| 12 | TFT SCLK |
| 13 | TFT MISO |
| 9 | TFT DC |
| 8 | TFT RST |
| 5 | TFT CS |
| 4 | Backlight |
| 1 | Touch CS |
| 6 | IMU SDA |
| 7 | IMU SCL |
| 2 | Battery ADC (divider ×2) |

Pins are defined in `Firmware/platformio.ini` via TFT_eSPI build flags.

### Enclosure CAD

Source enclosure: `CAD/Case_v0.FCStd` (FreeCAD). Slicer outputs (`*.gcode`, `*.3mf`, Fusion backups `*.FCBak`) are gitignored.

## Build and upload

Requires [PlatformIO](https://platformio.org/).

```bash
pio run -d Firmware
pio run -d Firmware -t upload
```

Serial monitor (115200 baud):

```bash
pio device monitor -b 115200 -d Firmware
```

Optional upload port:

```bash
pio run -d Firmware -t upload --upload-port COMx
```

Environment: `esp32-s3-supermini-n4r2`

## Gameplay (ball mode)

- Touch empty space to spawn a ball; touch a ball to drag it
- Quick release to throw; drag to the top-left 50×50 px zone to delete
- Match **3+ same-color** touching balls to score
- ~10% of new balls are explosive (white); tap to arm, 2 s fuse
- **15 s** patience timer — make a match before game over
- Menu (gear, top-right): Game vs Simulation mode, reset high score

## Project layout

```
ImpulsePlay/
├── Firmware/
│   ├── platformio.ini
│   ├── include/          # Headers (physics_engine, ball/eyes animation, …)
│   ├── src/              # Application source
│   └── test/             # Host unit tests for physics_engine
├── CAD/                  # Enclosure source (FreeCAD)
├── LICENSE               # MIT
└── README.md
```

## Physics tests (host)

```bash
pio test -d Firmware -e native
```

If no `native` environment is configured, compile the test file on a desktop toolchain:

```bash
g++ -std=c++17 -I Firmware/include Firmware/test/physics_engine_collision_test.cpp -lm -o physics_test && ./physics_test
```

## License

MIT — see [LICENSE](LICENSE).

## Support

[Intellar store](https://intellar.square.site/)



