#include <stdint.h>
#include <stdio.h>

#include "chomp_table.h"

typedef struct {
    uint8_t length[CHOMP_SIDE];
} Board;

typedef struct {
    uint8_t row;
    uint8_t column;
} Move;

static char input[76];

static int is_digit(char character)
{
    return character >= '0' && character <= '9';
}

static int wants_quit(const char *text)
{
    while (*text == ' ' || *text == '\t')
        ++text;
    if (*text != 'Q' && *text != 'q')
        return 0;
    ++text;
    while (*text == ' ' || *text == '\t')
        ++text;
    return *text == '\0';
}

/* Read two unsigned decimal fields, or split a single two-digit field.
   With boards at most 9 by 9, a compact pair is unambiguous. */
static int parse_move(const char *text, Move *move)
{
    unsigned fields[2];
    unsigned count = 0;
    const char *compact = 0;
    const char *next = text;

    while (*next) {
        if (!is_digit(*next)) {
            ++next;
            continue;
        }
        const char *start = next;
        if (count == 2)
            return 0;
        if (start != text && start[-1] == '-' &&
            (start == text + 1 || !is_digit(start[-2])))
            return 0;
        unsigned value = 0;
        do {
            /* Values above 9 cannot be coordinates. Stop accumulating
               them early so even a full input line cannot overflow. */
            if (value <= CHOMP_SIDE)
                value = value * 10 + (*next - '0');
            ++next;
        } while (is_digit(*next));
        if (!count && next - start == 2)
            compact = start;
        fields[count++] = value;
    }

    if (count == 1 && compact) {
        fields[0] = compact[0] - '0';
        fields[1] = compact[1] - '0';
        count = 2;
    }
    if (count != 2 || fields[0] < 1 || fields[0] > CHOMP_SIDE ||
        fields[1] < 1 || fields[1] > CHOMP_SIDE)
        return 0;

    move->row = (uint8_t)(fields[0] - 1);
    move->column = (uint8_t)(fields[1] - 1);
    return 1;
}

/* Only trailing whitespace may follow a single-digit setup answer. */
static int ask_number(const char *prompt, unsigned maximum)
{
    for (;;) {
        printf("%s", prompt);
        if (os6_getline(input, sizeof(input)) < 0 || wants_quit(input))
            return 0;
        const char *first = input;
        while (*first == ' ' || *first == '\t')
            ++first;
        if (*first >= '1' && (unsigned)(*first - '0') <= maximum) {
            const char *rest = first + 1;
            while (*rest == ' ' || *rest == '\t')
                ++rest;
            if (!*rest)
                return *first - '0';
        }
        printf("Enter a number from 1 to %u, or Q to quit.\n", maximum);
    }
}

static int is_poison(const Move *move)
{
    return move->row == 0 && move->column == 0;
}

static void apply_move(Board *board, const Move *move)
{
    for (unsigned row = move->row; row < CHOMP_SIDE; ++row) {
        if (board->length[row] <= move->column)
            break;
        board->length[row] = move->column;
    }
}

static int is_winning(const Board *board)
{
    uint16_t index = 0;
    for (unsigned i = 0; i < CHOMP_SIDE; ++i)
        index += rank_term[i][board->length[CHOMP_SIDE - 1 - i] + i];
    return (winning_bits[index >> 3] >> (index & 7)) & 1;
}

static void computer_move(const Board *board, Move *chosen)
{
    chosen->row = chosen->column = 0;
    /* Start near the lower right. If all moves lose against perfect
       play, the first safe candidate still gives the player a turn. */
    for (int row = CHOMP_SIDE - 1; row >= 0; --row) {
        for (int column = board->length[row] - 1; column >= 0; --column) {
            Move move = {(uint8_t)row, (uint8_t)column};
            if (is_poison(&move))
                break;
            if (is_poison(chosen))
                *chosen = move;
            Board child = *board;
            apply_move(&child, &move);
            if (!is_winning(&child)) {
                *chosen = move;
                return;
            }
        }
    }
}

static void draw_board(const Board *board, unsigned rows, unsigned columns)
{
    char line[4 + 2 * CHOMP_SIDE];
    line[0] = line[1] = line[2] = ' ';
    for (unsigned column = 0; column < columns; ++column) {
        line[3 + 2 * column] = ' ';
        line[4 + 2 * column] = '1' + column;
    }
    line[3 + 2 * columns] = '\0';
    printf("\n%s\n", line);
    for (unsigned row = 0; row < rows; ++row) {
        line[0] = '1' + row;
        for (unsigned column = 0; column < columns; ++column) {
            char cell = '.';
            if (column < board->length[row])
                cell = row == 0 && column == 0 ? 'P' : '*';
            line[4 + 2 * column] = cell;
        }
        printf("%s\n", line);
    }
    putchar('\n');
}

static int player_move(const Board *board, Move *move)
{
    for (;;) {
        printf("Your move (ROW COLUMN, or Q to quit): ");
        if (os6_getline(input, sizeof(input)) < 0 || wants_quit(input))
            return 0;
        if (!parse_move(input, move)) {
            puts("Enter two coordinates, or two digits such as 22.");
        } else if (move->column >= board->length[move->row]) {
            puts("That square is outside the remaining board.");
        } else {
            return 1;
        }
    }
}

int main(void)
{
    puts("CHOMP - You versus the computer");
    puts("Take one square and everything below and to its right.");
    puts("Taking P loses the game.");

    unsigned rows = ask_number("Number of rows (1-9): ", CHOMP_SIDE);
    if (!rows)
        return 0;
    unsigned columns = ask_number("Number of columns (1-9): ", CHOMP_SIDE);
    if (!columns)
        return 0;
    int first = ask_number("Who starts (1 = you, 2 = computer)? ", 2);
    if (!first)
        return 0;

    Board board = {{0}};
    for (unsigned row = 0; row < rows; ++row)
        board.length[row] = (uint8_t)columns;

    int human = first == 1;
    for (;;) {
        Move move;
        draw_board(&board, rows, columns);
        if (human) {
            if (!player_move(&board, &move))
                return 0;
        } else {
            computer_move(&board, &move);
            printf("Computer takes row %u, column %u.\n",
                   (unsigned)move.row + 1, (unsigned)move.column + 1);
        }
        if (is_poison(&move)) {
            puts(human ? "You took the poisoned square. Computer wins."
                       : "Computer took the poisoned square. You win.");
            return 0;
        }
        apply_move(&board, &move);
        human = !human;
    }
}
