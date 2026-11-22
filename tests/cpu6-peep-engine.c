/* Drive the actual SDCC peephole engine and compiled-in CPU6 rules. */
#include "common.h"
#include "SDCCpeeph.h"
#include "cpu6/peep.h"
#include <assert.h>

extern PORT cpu6_port;

int main(int argc, char **argv)
{
    assert(argc == 3 || (argc == 4 && (!strcmp(argv[3], "--allocate-ab") ||
           !strcmp(argv[3], "--loops") || !strcmp(argv[3], "--size"))));
    optimize.codeSize = argc == 4 && !strcmp(argv[3], "--size");
    FILE *input = fopen(argv[1], "r"), *output = fopen(argv[2], "w");
    assert(input && output);
    port = &cpu6_port;
    initPeepHole();
    lineNode *head = NULL, *tail = NULL;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), input)) {
        size_t n = strlen(buffer);
        while (n && (buffer[n-1] == '\n' || buffer[n-1] == '\r')) buffer[--n] = 0;
        if (!n) continue;
        lineNode *line = Safe_alloc(sizeof(*line));
        memset(line, 0, sizeof(*line));
        const char *text = buffer;
        if (*text == '!') { line->mightBeVolatileAccess = true; text++; }
        if (*text == '@') { line->isInline = true; text++; }
        line->line = Safe_strdup(text);
        line->isLabel = text[strlen(text)-1] == ':';
        line->isComment = *text == ';';
        line->prev = tail;
        if (tail) tail->next = line; else head = line;
        tail = line;
    }
    fclose(input);
    if (argc == 4 && !strcmp(argv[3], "--loops")) {
        cpu6OptimizeLoops(head);
    } else {
        peepHole(&head);
        if (argc == 4 && !strcmp(argv[3], "--allocate-ab") && cpu6AllocateAB(head)) peepHole(&head);
    }
    cpu6FinalizeBranches(head);
    for (lineNode *line = head; line; line = line->next) fprintf(output, "%s\n", line->line);
    fclose(output);
    return 0;
}
