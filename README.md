# Centurion CPU6 emulator

C++ port of the microcoded CenRE emulator by Meisaka Yukara, adapted
for the Centurion SDCC distribution. See [LICENSE.txt](LICENSE.txt).

Build with GCC 13 or newer, Boost headers and Python 3. The server uses the
firmware pack, firmware specification and web assets supplied by the parent
project.
GCC 15 uses C++26 and `#embed`. With GCC 13, the Python standard library
generates a C++23 initializer for the same local control-store ROMs. Both
builds specialize the microcycle handlers with the same `constexpr` decoder.
Keep the firmware, downloaded ROM-bearing sources and firmware-derived build
products in the local build directory.

```sh
make OUT=/absolute/path/to/build FIRMWARE_JSON=/absolute/path/to/firmware.json
```

Use `make headless` with the Node-API headers to build the binding used by
the parent project's image validator. It drives the same machine directly
at maximum speed. The compiler regression probes use the C++ test sources
from the parent checkout.
