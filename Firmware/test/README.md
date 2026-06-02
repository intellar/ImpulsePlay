# Firmware tests

Host-side unit tests for `physics_engine` (collision and squash/stretch).

```bash
g++ -std=c++17 -I ../include physics_engine_collision_test.cpp -lm -o physics_test
./physics_test
```

The test file is excluded from ESP32 builds (`#ifndef ARDUINO`).
