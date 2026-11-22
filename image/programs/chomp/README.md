# Chomp against the CPU6

The application image includes the C game. At the CENTOS prompt, run
`.RUN SDCC.XCHOMP`. The image builder generates the packed table with
`chomp_table.py` before compiling the program with SDCC.

Choose the number of rows and columns, each from 1 to 9, and who takes
the first turn. Each move gives
the row first and the column second. For row 1 and column 2, enter
`1 2`, `1,2`, `1;2`, `1:2` or `12`. A move must select a remaining
square. Select `Q` at a prompt to return to CENTOS.

The build generates one packed table for all of these boards. The opponent
looks for a move that leaves a losing position to the next player and makes
a legal move when the current position is losing. Taking the poison in the
upper left loses the game. [David Gale's rules and analysis](https://doi.org/10.1080/00029890.1974.11993683)

For a standalone copy, `make -C image/programs/chomp image` installs
the root command `.RUN XCHOMP` on a new disk. Use `SEED=/path/to/input.img`
to select the source disk or `CHOMP_IMAGE=/path/to/new.img` to select the output.
