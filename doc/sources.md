# Source revisions and publication

The distribution pins the `centurion` branch of each submodule at an exact
commit. An ordinary `git submodule update --init --recursive` checks out these
pins. The `branch` entries in `.gitmodules` also identify the branches used by
an explicit `git submodule update --remote`.

| Submodule | Upstream base | Original branch in the local repository |
| --- | --- | --- |
| SDCC | [SVN r17105](https://repo.or.cz/sdcc.git/commit/f966b0004c76eb960ce61c06fabecacf3916aa94) | `master`, original Git commit |
| Fuzix-Bintools | [ca2ab54](https://codeberg.org/EtchedPixels/Fuzix-Bintools/commit/ca2ab54a4599f60dba7ca9882956178f070e7e13) | `main`, original Git history |
| EE200 runtime | [7b2a72e](https://github.com/EtchedPixels/Fuzix-Compiler-Kit/tree/7b2a72edea6967e7df0b32e1e898fdea1754cc67/supportee200) | `upstream`, selected assembly source files |
| JavaScript emulator | [2bf3550](https://github.com/billsargent/centurion/tree/2bf355026e3f5d4ebdf51138088b9499ccb31b09) | `upstream`, selected TypeScript source files and notices |
| Stefan BASIC | [86ec458](https://github.com/slviajero/tinybasic/tree/86ec458ca3dac9d156ce23bfa28a99f1f39ca656/Basic2/Posix) | `upstream`, selected POSIX interpreter sources and license |

The three source imports preserve the original contents of the selected
files and have local Git commit IDs. The EE200 import keeps only the runtime
assembly and attribution. The emulator import keeps the TypeScript source
used by the headless build. The BASIC import keeps only the POSIX interpreter
sources used by the port. Each submodule has a `CENTURION.md` on the
`centurion` branch with its changes.

## Publication status

The GitHub URLs in `.gitmodules` are the intended destinations under
`alex31`. The pinned commits and branches currently exist in the local
submodules. Confirm the [EE200 runtime redistribution terms](../swext/libee200/CENTURION.md)
before publishing. Review any conditions on keeping or redistributing
the original ROM tables included in the CenRE source.

Publish each submodule before the distribution repository. Push both the
original branch named above and `centurion`. The SDCC clone is shallow
at the original commit. Run `git -C swext/sdcc fetch --unshallow upstream master`
before pushing the SDCC branches to an empty remote. Set each component's
default branch to `centurion` so the default shallow checkout contains the
distribution branch. Then push `main` of this repository and verify a plain
clone with `make check`.
