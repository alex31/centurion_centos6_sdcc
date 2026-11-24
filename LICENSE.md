# Licenses

The original distribution scripts, examples and Centurion runtime code in this
repository are distributed under the GNU General Public License, version 2
or any later version. See [COPYING](COPYING). Upstream components retain their
own license terms and copyright notices.

| Component | Upstream notices |
| --- | --- |
| SDCC and its preprocessor | [SDCC COPYING](swext/sdcc/sdcc/COPYING), [SDCC documentation](swext/sdcc/sdcc/doc/README.txt), and file-specific notices |
| Fuzix-Bintools | [LICENCE](swext/fuzix-bintools/LICENCE), [COPYRIGHT.MWC](swext/fuzix-bintools/COPYRIGHT.MWC), and file-specific notices |
| C instruction emulator | [GPLv3 license](swext/centurion-c/LICENCE) and file-specific notices |
| JavaScript emulator | [CenRE License](swext/centurion-js/LICENSE.txt) and [NOTICE](swext/centurion-js/NOTICE.md) |
| Stefan BASIC | [Upstream LICENSE](swext/stefan-basic/LICENSE), source notices and the [GPLv3 text](swext/stefan-basic/COPYING.GPL-3) |
| 101 BASIC Computer Games listings | [Upstream catalog and attribution](swext/101-basic-games/README.md) and listing-specific credits |
| mcu-max | [MIT license](swext/mcu-max/LICENSE) |
| E-Kermit | [Revised three-clause BSD license](swext/ekermit/COPYING) |
| Historical EE200 assembly runtime (optional checkout) | [Upstream notice](https://github.com/EtchedPixels/Fuzix-Compiler-Kit#licence) and [source provenance](#source-provenance) |

The build installs TypeScript and ws from npm, with their own notices.
Firmware packs and CENTOS disk images are local inputs supplied separately.
See the [Centurion archive notice](https://github.com/Nakazoto/CenturionComputer/wiki/Software#operating-system)
for access information.

## Source provenance

The distribution pins its dependencies in `.gitmodules` and Git. Use
`git submodule update --init --recursive` to check out their pinned
revisions. The historical EE200 runtime is an optional checkout with
`update = none`.

| Submodule | Upstream base | Original branch in the local repository |
| --- | --- | --- |
| SDCC | [SVN r17105](https://repo.or.cz/sdcc.git/commit/f966b0004c76eb960ce61c06fabecacf3916aa94) | `master`, original Git commit |
| Fuzix-Bintools | [ca2ab54](https://codeberg.org/EtchedPixels/Fuzix-Bintools/commit/ca2ab54a4599f60dba7ca9882956178f070e7e13) | `main`, original Git history |
| EE200 runtime | [7b2a72e](https://github.com/EtchedPixels/Fuzix-Compiler-Kit/tree/7b2a72edea6967e7df0b32e1e898fdea1754cc67/supportee200) | `upstream`, selected assembly source files |
| C instruction emulator | [4e1a61c](https://github.com/EtchedPixels/Centurion/tree/4e1a61c56a48f36f231e93837f4896d621290685) | `upstream`, source tree |
| JavaScript emulator | [2bf3550](https://github.com/billsargent/centurion/tree/2bf355026e3f5d4ebdf51138088b9499ccb31b09) | `upstream-software`, selected software files and notices with external firmware |
| Stefan BASIC | [86ec458](https://github.com/slviajero/tinybasic/tree/86ec458ca3dac9d156ce23bfa28a99f1f39ca656/Basic2/Posix) | `upstream`, selected POSIX interpreter sources and license |
| 101 BASIC Computer Games | [45f7505](https://github.com/maurymarkowitz/101-BASIC-Computer-Games/tree/45f75052be3f436f2e575f58d4aa611484268227) | `main`, original listings used by the image builder |
| mcu-max | [1.0.6](https://github.com/Gissio/mcu-max/tree/aa03caffce50729566b5db6965bf735c31f33eea) | `upstream`, engine sources and license |
| E-Kermit | [1.8 protocol release](https://www.kermitproject.org/ek.html) | `upstream`, protocol sources and license |

The TypeScript source in the CenRE snapshot keeps the firmware external.
Table lengths are defined in the
[firmware specification](swext/centurion-js/src/firmware-spec.json).
The build uses a local pack for the bootstrap, CPU microcode,
diagnostic ROMs and other tables. The emulator retains the CenRE
license and attribution, and the BASIC port retains its
[provenance manifest](swext/stefan-basic/PROVENANCE.json).

The Fuzix Compiler Kit's original README states that the compiler's GPLv3
excludes the EE200 runtime. The selected runtime assembly carries no
separate license notice. Confirm the redistribution terms with the copyright
holder before redistributing that optional snapshot.
[Original EE200 source](https://github.com/EtchedPixels/Fuzix-Compiler-Kit/tree/7b2a72edea6967e7df0b32e1e898fdea1754cc67/supportee200)
