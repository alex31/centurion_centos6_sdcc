/*
 *	wumpus
 *	stolen from PCC Vol 2 No 1
 *
 * Adapted for CENTOS from usr/src/games/wump.c in Research UNIX V7.
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 * See LICENSE in this directory.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define	NBAT	3
#define	NROOM	20
#define	NTUNN	3
#define	NPIT	3

#define SHOT_LIMIT 5
#define LINE_SIZE 80

static struct room
{
	int	tunn[NTUNN];
	int	flag;
} room[NROOM];

static const char *const intro[] =
{
	"\n",
	"Welcome to 'Hunt the Wumpus.'\n",
	"\n",
	"The Wumpus lives in a cave of %d rooms.\n",
	"Each room has %d tunnels leading to other rooms.\n",
	"\n",
	"Hazards:\n",
	"\n",
	"Bottomless Pits - Some rooms have Bottomless Pits in them.\n",
	"	If you go there, you fall into the pit and lose!\n",
	"Super Bats - Some other rooms have super bats.\n",
	"	If you go there, a bat will grab you and take you to\n",
	"	somewhere else in the cave where you could\n",
	"	fall into a pit or run into the . . .\n",
	"\n",
	"Wumpus:\n",
	"\n",
	"The Wumpus is not bothered by the hazards since\n",
	"he has sucker feet and is too big for a bat to lift.\n",
	"\n",
	"Usually he is asleep.\n",
	"Two things wake him up:\n",
	"	your entering his room\n",
	"	your shooting an arrow anywhere in the cave.\n",
	"If the wumpus wakes, he either decides to move one room or\n",
	"stay where he was.  But if he ends up where you are,\n",
	"he eats you up and you lose!\n",
	"\n",
	"You:\n",
	"\n",
	"Each turn you may either move or shoot a crooked arrow.\n",
	"\n",
	"Moving - You can move to one of the adjoining rooms;\n",
	"	that is, to one that has a tunnel connecting it with\n",
	"	the room you are in.\n",
	"\n",
	"Shooting - You have 5 arrows.  You lose when you run out.\n",
	"	Each arrow can go from 1 to 5 rooms.\n",
	"	You aim by telling the computer\n",
	"	The arrow's path is a list of room numbers\n",
	"	telling the arrow which room to go to next.\n",
	"	The list is terminated with a 0.\n",
	"	The first room in the path must be connected to the\n",
	"	room you are in.  Each succeeding room must be\n",
	"	connected to the previous room.\n",
	"	If there is no tunnel between two of the rooms\n",
	"	in the arrow's path, the arrow chooses one of the\n",
	"	three tunnels from the room it's in and goes its\n",
	"	own way.\n",
	"\n",
	"	If the arrow hits the wumpus, you win!\n",
	"	If the arrow hits you, you lose!\n",
	"\n",
	"Warnings:\n",
	"\n",
	"When you are one or two rooms away from the wumpus,\n",
	"the computer says:\n",
	"		'I smell a Wumpus'\n",
	"When you are one room away from some other hazard, it says:\n",
	"		Bat    - 'Bats nearby'\n",
	"		Pit    - 'I feel a draft'\n",
	"\n",
	0,
};

#define	BAT	01
#define	PIT	02
#define	WUMP	04

static int arrow;
static int loc;
static int wloc;

enum input_result { INPUT_QUIT = -1, INPUT_INVALID, INPUT_OK };

static int tunnel(int from);
static int rnum(int limit);
static int near(const struct room *p, int hazard);
static int icomp(const void *left, const void *right);

static int space(int character)
{
	return character == ' ' || character == '\t';
}

static char *skip_spaces(char *text)
{
	while (space(*text))
		++text;
	return text;
}

static int command_word(char **cursor)
{
	char *word = skip_spaces(*cursor);
	int command = *word;

	if (!command || (word[1] && !space(word[1])))
		return 0;
	*cursor = skip_spaces(word + 1);
	if (command >= 'A' && command <= 'Z')
		command += 'a' - 'A';
	return command;
}

static int quit_word(char *text)
{
	return command_word(&text) == 'q' && !*text;
}

static int read_reply(const char *prompt, char *line)
{
	int length;

	printf("%s", prompt);
	length = os6_getline_checked(line, LINE_SIZE);
	if (length == EOF)
		return INPUT_QUIT;
	return length < 0 ? INPUT_INVALID : INPUT_OK;
}

static int confirm(const char *prompt, char *line)
{
	for (;;) {
		int status = read_reply(prompt, line);
		char *cursor = line;
		int command;

		if (status == INPUT_QUIT)
			return INPUT_QUIT;
		if (status == INPUT_OK) {
			command = command_word(&cursor);
			if (!*cursor) {
				if (command == 'y' || command == 'n')
					return command;
				if (command == 'q')
					return INPUT_QUIT;
			}
		}
		puts("Enter Y, N or Q.");
	}
}

static int read_room(char **cursor, int *number)
{
	char *text = *cursor, *end;
	long value;

	errno = 0;
	value = strtol(text, &end, 10);
	if (end == text || errno || value < 0 || value > NROOM ||
	    (*end && !space(*end)))
		return INPUT_INVALID;
	*cursor = skip_spaces(end);
	*number = (int)value;
	return INPUT_OK;
}

static int read_move(char *cursor, char *line, int *destination)
{
	int number, status;

	if (!*cursor) {
		status = read_reply("Which room? ", line);
		if (status != INPUT_OK)
			return status;
		cursor = skip_spaces(line);
		if (quit_word(cursor))
			return INPUT_QUIT;
	}
	if (read_room(&cursor, &number) != INPUT_OK || !number || *cursor)
		return INPUT_INVALID;
	*destination = number - 1;
	return INPUT_OK;
}

static int read_shot(char *cursor, char *line, int *path, int *count)
{
	int number, status;
	*count = 0;

	for (;;) {
		if (!*cursor) {
			status = read_reply("Rooms (0 to finish)? ", line);
			if (status != INPUT_OK)
				return status;
			cursor = skip_spaces(line);
			if (quit_word(cursor))
				return INPUT_QUIT;
			if (!*cursor)
				return INPUT_INVALID;
		}
		while (*cursor) {
			if (read_room(&cursor, &number) != INPUT_OK)
				return INPUT_INVALID;
			if (!number)
				return *count && !*cursor ? INPUT_OK : INPUT_INVALID;
			if (*count == SHOT_LIMIT)
				return INPUT_INVALID;
			path[(*count)++] = number - 1;
		}
		if (*count == SHOT_LIMIT)
			return INPUT_OK;
	}
}

int main(void)
{
	int i, j, k, command, status;
	int path[SHOT_LIMIT], count;
	struct room *p;
	char line[LINE_SIZE], *cursor;
	time_t now = time(NULL);

	srand(now == (time_t)-1 ? 1 : (unsigned int)(now ^ (now >> 16)));
	status = confirm("Instructions? (y-n) ", line);
	if (status == INPUT_QUIT)
		return EXIT_SUCCESS;
	if (status == 'y')
		for(i=0; intro[i]; i++)
			printf(intro[i], i&1? NROOM: NTUNN);


/*
 * initialize the room connections
 */

init:
	p = &room[0];
	for(i=0; i<NROOM; i++) {
		for(j=0; j<NTUNN; j++)
			p->tunn[j] = -1;
		p++;
	}
	k = 0;
	for(i=1; i<NROOM; ) {
		j = rnum(NROOM);
		p = &room[j];
		if(j == k || p->tunn[0] >= 0 || p->tunn[1] >= 0)
			continue;
		p->tunn[1] = k;
		room[k].tunn[0] = j;
		k = j;
		i++;
	}
	p = &room[0];
	for(i=0; i<NROOM; i++) {
		for(j=0; j<NTUNN; j++) {
			if(p->tunn[j] < 0)
				p->tunn[j] = tunnel(i);
			if(p->tunn[j] == i)
				goto init;
			for(k=0; k<j; k++)
				if(p->tunn[j] == p->tunn[k])
					goto init;
		}
		qsort(p->tunn, NTUNN, sizeof(p->tunn[0]), icomp);
		p++;
	}

/*
 * put in player, wumpus,
 * pits and bats
 */

setup:
	arrow = 5;
	p = &room[0];
	for(i=0; i<NROOM; i++) {
		p->flag = 0;
		p++;
	}
	for(i=0; i<NPIT; ) {
		p = &room[rnum(NROOM)];
		if((p->flag&PIT) == 0) {
			p->flag |= PIT;
			i++;
		}
	}
	for(i=0; i<NBAT; ) {
		p = &room[rnum(NROOM)];
		if((p->flag&(PIT|BAT)) == 0) {
			p->flag |= BAT;
			i++;
		}
	}
	i = rnum(NROOM);
	wloc = i;
	room[i].flag |= WUMP;
	for(;;) {
		i = rnum(NROOM);
		if((room[i].flag&(PIT|BAT|WUMP)) == 0) {
			loc = i;
			break;
		}
	}

/*
 *	main loop of the game
 */

loop:
	printf("You are in room %d\n", loc+1);
	p = &room[loc];
	if(p->flag&PIT) {
		printf("You fell into a pit\n");
		goto done;
	}
	if(p->flag&WUMP) {
		printf("You were eaten by the wumpus\n");
		goto done;
	}
	if(p->flag&BAT) {
		printf("Theres a bat in your room\n");
		loc = rnum(NROOM);
		goto loop;
	}
	for(i=0; i<NTUNN; i++)
	if(near(&room[p->tunn[i]], WUMP))
		goto nearwump;
	if (near(p, WUMP)) {
	nearwump:
		printf("I smell a wumpus\n");
	}
	if (near(p, BAT))
		printf("Bats nearby\n");
	if (near(p, PIT))
		printf("I feel a draft\n");
	printf("There are tunnels to");
	for(i=0; i<NTUNN; i++)
		printf(" %d", p->tunn[i]+1);
	printf("\n");

again:
	status = read_reply("Move or shoot (m-s, q to quit) ", line);
	if (status == INPUT_QUIT)
		return EXIT_SUCCESS;
	if (status != INPUT_OK)
		goto bad_input;
	cursor = line;
	command = command_word(&cursor);
	switch (command) {
	case 'q':
		if (!*cursor)
			return EXIT_SUCCESS;
		goto bad_input;
	case 'm':
		status = read_move(cursor, line, &i);
		if (status == INPUT_QUIT)
			return EXIT_SUCCESS;
		if (status != INPUT_OK)
			goto bad_input;
		for(j=0; j<NTUNN; j++)
			if(i == p->tunn[j])
				goto groom;
		printf("You hit the wall\n");
		goto again;
	groom:
		loc = i;
		if(i == wloc)
			goto mwump;
		goto loop;

	case 's':
		status = read_shot(cursor, line, path, &count);
		if (status == INPUT_QUIT)
			return EXIT_SUCCESS;
		if (status != INPUT_OK)
			goto bad_input;
		for(i=0; i<count; i++) {
			j = path[i];
		ranarw:
			for(k=0; k<NTUNN; k++)
				if(j == p->tunn[k])
					goto garow;
			j = rnum(NROOM);
			goto ranarw;
		garow:
			p = &room[j];
			if(j == loc) {
				printf("You shot yourself\n");
				goto done;
			}
			if(p->flag&WUMP) {
				printf("You slew the wumpus\n");
				goto done;
			}
		}
		if(--arrow == 0) {
			printf("That was your last shot\n");
			goto done;
		}
		goto mwump;
	}

bad_input:
	puts("Invalid input. Try again.");
	goto again;

mwump:
	p = &room[wloc];
	p->flag &= ~WUMP;
	i = rnum(NTUNN+1);
	if(i != NTUNN)
		wloc = p->tunn[i];
	room[wloc].flag |= WUMP;
	goto loop;

done:
	if (confirm("Another game? (y-n) ", line) == 'y') {
		status = confirm("Same room setup? (y-n) ", line);
		if (status == INPUT_QUIT)
			return EXIT_SUCCESS;
		if (status == 'y')
			goto setup;
		goto init;
	}
	return EXIT_SUCCESS;
}

static int tunnel(int i)
{
	struct room *p;
	int n, j, c;

	c = 20;

loop:
	n = rnum(NROOM);
	if(n == i)
		if(--c > 0)
			goto loop;
	p = &room[n];
	for(j=0; j<NTUNN; j++)
	if(p->tunn[j] == -1) {
		p->tunn[j] = i;
		return(n);
	}
	goto loop;
}

static int rnum(int limit)
{
	return (int)(((unsigned long)rand() * (unsigned int)limit) >> 15);
}

static int near(const struct room *p, int hazard)
{
	int i;

	for(i=0; i<NTUNN; i++)
	if(room[p->tunn[i]].flag & hazard)
		return (1);
	return(0);
}

static int icomp(const void *left, const void *right)
{
	return *(const int *)left - *(const int *)right;
}
