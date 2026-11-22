# Centurion CPU6 emulator

C++26 port of the microcoded CenRE emulator by Meisaka Yukara, adapted
for the Centurion SDCC distribution. See [LICENSE.txt](LICENSE.txt).

Build with GCC 15.2, Boost headers and Python 3. The server uses the firmware
pack, firmware specification and web assets supplied by the parent project.
The local control-store ROMs specialize the microcycle decoder through
`#embed` and `constexpr`. Keep the firmware, downloaded ROM-bearing sources
and firmware-derived build products in the local build directory.

```sh
make OUT=/absolute/path/to/build FIRMWARE_JSON=/absolute/path/to/firmware.json
```

Use `make headless` with the Node-API headers to build the binding used by
the parent project's image validator. It drives the same machine directly
at maximum speed. The compiler regression probes use the C++ test sources
from the parent checkout.
