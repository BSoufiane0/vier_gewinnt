/*-
 * Copyright (c) 2004-2011, Michael Tuexen.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "4clib.h"
#include <stdio.h>

static int field[NUMBER_OF_ROWS][NUMBER_OF_COLUMNS];
static int next_row[NUMBER_OF_COLUMNS];

void
init_4clib(void)
{
	int i, j;

	for (i = 0; i < NUMBER_OF_ROWS; i++)
		for (j = 0; j < NUMBER_OF_COLUMNS; j++)
			field[i][j] = 0;
	for (j = 0; j < NUMBER_OF_COLUMNS; j++)
		next_row[j] = 0;
}

int 
valid_move(int c)
{
	if ((0 <= c) && (c < NUMBER_OF_COLUMNS) && (next_row[c] < NUMBER_OF_ROWS))
		return 1;
	else
		return 0;
}

void
make_move(int c, int player)
{
	field[next_row[c]][c] = player;
	next_row[c]++;
}

int
winner(void)
{
	int i, j;

	for (i = 0; i < NUMBER_OF_ROWS; i++)
		for (j = 0; j < NUMBER_OF_COLUMNS - 3; j++)
			if ((field[i][j] != 0) &&
			    (field[i][j] == field[i][j+1]) && 
			    (field[i][j] == field[i][j+2]) &&
			    (field[i][j] == field[i][j+3]))
				return field[i][j];
	for (i = 0; i < NUMBER_OF_ROWS - 3; i++)
		for (j = 0; j < NUMBER_OF_COLUMNS - 3; j++)
			if ((field[i][j] != 0) &&
			    (field[i][j] == field[i+1][j+1]) && 
			    (field[i][j] == field[i+2][j+2]) &&
			    (field[i][j] == field[i+3][j+3]))
				return field[i][j];
	for (i = 0; i < NUMBER_OF_ROWS - 3; i++)
		for (j = 0; j < NUMBER_OF_COLUMNS; j++)
			if ((field[i][j] != 0) &&
			    (field[i][j] == field[i+1][j]) && 
			    (field[i][j] == field[i+2][j]) &&
			    (field[i][j] == field[i+3][j]))
				return field[i][j];
	for (i = 0; i < NUMBER_OF_ROWS - 3; i++)
		for (j = 3; j < NUMBER_OF_COLUMNS; j++)
			if ((field[i][j] != 0) &&
			    (field[i][j] == field[i+1][j-1]) && 
			    (field[i][j] == field[i+2][j-2]) &&
			    (field[i][j] == field[i+3][j-3]))
				return field[i][j];
	return (0);
}

void 
print_board(void)
{
	int i, j;

	printf("---------------\n");
	for (i = NUMBER_OF_ROWS - 1; i >= 0; i--) {
		for(j = 0; j < NUMBER_OF_COLUMNS; j++) {
			printf("|");
			switch (field[i][j]) {
			case PLAYER_1: 
				printf("X");
				break;
			case PLAYER_2:
				printf("O");
				break;
			default:
				printf(" ");
			}
		}
		printf("|\n");
	}
	printf("---------------\n");
}


