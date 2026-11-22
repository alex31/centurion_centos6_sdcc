/* Host E-Kermit test peer. Stdin and stdout carry the raw Centurion link. */
#include <stdio.h>

#include "cdefs.h"
#include "kermit.h"

static FILE *payload;

static int open_input(struct k_data *state, UCHAR *name, int mode)
{
    (void)state;
    (void)name;
    return mode == 1 ? X_OK : X_ERROR;
}

static int close_input(struct k_data *state, UCHAR disposition, int mode)
{
    (void)state;
    (void)disposition;
    return mode == 1 ? X_OK : X_ERROR;
}

static int read_input(struct k_data *state)
{
    state->zincnt = 0;
    return fgetc(payload);
}

static int escaped(int value)
{
    switch (value) {
    case SOH: return 'A';
    case CR: return 'M';
    case '+': return 'P';
    case '_': return 'U';
    case '`': return '`';
    default: return 0;
    }
}

static int send_packet(struct k_data *state, UCHAR *packet, int size)
{
    int start = 0;
    (void)state;
    while (start < size) {
        char line[71];
        int end = start, used = 0;
        while (end < size) {
            const int value = packet[end] & 127;
            const int escape = escaped(value);
            if (used + (escape ? 2 : 1) > (int)sizeof line)
                break;
            if (escape) {
                line[used++] = '`';
                line[used++] = (char)escape;
            } else if (value >= 32 && value <= 126) {
                line[used++] = (char)value;
            } else {
                return X_ERROR;
            }
            ++end;
        }
        if (fprintf(stdout, "%c%02X", end == size ? 'L' : 'C', (unsigned)(end - start)) < 0 ||
            fwrite(line, 1, used, stdout) != (size_t)used || fputc(CR, stdout) == EOF ||
            fflush(stdout) == EOF)
            return X_ERROR;
        if (end != size) {
            int reply;
            do {
                reply = fgetc(stdin);
                if (reply == EOF)
                    return X_ERROR;
            } while ((reply & 127) != '+');
        }
        start = end;
    }
    return X_OK;
}

static int receive_packet(UCHAR *packet, int capacity)
{
    int value, used = 0;
    do {
        value = fgetc(stdin);
        if (value == EOF)
            return -1;
    } while ((value & 127) != SOH);
    for (;;) {
        value = fgetc(stdin);
        if (value == EOF || used == capacity)
            return -1;
        value &= 127;
        if (value == CR)
            return used;
        packet[used++] = (UCHAR)value;
    }
}

int main(int argc, char **argv)
{
    static struct k_data state;
    static struct k_response response;
    static UCHAR name[] = "PAYLOAD.EXF";
    UCHAR *files[] = {name, 0};
    int status;

    if (argc != 2 || !(payload = fopen(argv[1], "rb")))
        return 1;
    state.txd = send_packet;
    state.openf = open_input;
    state.readf = read_input;
    state.closef = close_input;
    state.binary = BINARY;
    state.xfermode = 1;
    state.parity = PAR_EVEN;
    state.bct = 3;
    state.filelist = files;
    status = kermit(K_INIT, &state, 0, 0, "", &response);
    if (status == X_OK)
        status = kermit(K_SEND, &state, 0, 0, "", &response);
    while (status == X_OK) {
        short slot;
        UCHAR *packet = getrslot(&state, &slot);
        if (!packet || slot < 0) {
            status = X_ERROR;
            break;
        }
        const int length = receive_packet(packet, P_PKTLEN + 7);
        if (length <= 0) {
            status = X_ERROR;
            break;
        }
        status = kermit(K_RUN, &state, slot, length, "", &response);
    }
    fclose(payload);
    if (status != X_DONE || !state.ebqflg || state.bct != 3) {
        fprintf(stderr, "E-Kermit transfer failed (status %d, state %d, sequence %d)\n",
                status, state.state, state.r_seq);
        return 1;
    }
    return 0;
}
