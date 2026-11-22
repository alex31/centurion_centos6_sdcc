# Source revisions and publication

The distribution pins each submodule at an exact
commit. An ordinary `git submodule update --init --recursive` checks out the
build and test components. The optional historical EE200 runtime has
`update = none`. The `branch` entries in `.gitmodules` identify the branches used by
an explicit `git submodule update --remote`.

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

The source imports record their upstream revisions and have local Git
commit IDs. The optional EE200 import preserves the
historical runtime assembly and attribution. The SDK links the
[native compiler helpers](../lib/compiler/README.md). The emulator import keeps the TypeScript source
used by the headless build. The C emulator is built for testing. The BASIC import keeps only the POSIX interpreter
sources used by the port. The Centurion forks document their changes in
`CENTURION.md`. The games submodule points directly to the public upstream
repository, with adaptations applied by the image builder.
The local `centos-vt100` branch in `swext/101-basic-games` records
the resulting listings under `centos/`. The Git submodule pins the public
source revision so clean clones can reproduce the same adaptations.

## Publication status

The GitHub URLs for the Centurion forks in `.gitmodules` are the intended destinations under
`alex31`. The pinned commits and branches currently exist in the local
submodules. The standard checkout skips the historical EE200 runtime. The
optional import retains its upstream README and licensing notes.

For the JavaScript emulator, publish `upstream-software` and
`centurion-inputs`, and set `centurion-inputs` as the default branch. Both
refs descend from the software snapshot with external firmware. The
historical `upstream` and `centurion` refs retain the original firmware
tables locally. Firmware packs and CENTOS seeds are supplied separately.

Publish each Centurion fork before the distribution repository.
For the other components, push both the original branch named above and
`centurion`. The SDCC clone is shallow
at the original commit. Run `git -C swext/sdcc fetch --unshallow upstream master`
before pushing the SDCC branches to an empty remote. Set each fork's
default branch to its `.gitmodules` branch so the shallow checkout contains the
distribution branch. Push `main` to `alex31/centurion_centos6_sdcc` and
verify a fresh clone with `make check`. Publishing `experimental/system_jcl`
also requires its pinned JavaScript emulator revision to be reachable.
