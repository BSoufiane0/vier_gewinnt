/*
 * Copyright (c) 2004-2006, Michael Tuexen. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Michael Tuexen nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "cblib.h"


static struct timer *T1, *T2, *T3, *T4;
static int c = 1;

void
f1 (void *arg)
{
	int x;

	x = *(int *)arg;
	printf("This is function 1 with arg %d.\n", x);
	fflush(stdout);
	start_timer(T1, 1000);
}

void
f2(void *arg)
{
	int x;

	x = *(int *)arg;
	printf("This is function 2 with arg %d.\n", x);
	fflush(stdout);
	start_timer(T2, 2000);
}

void
f3(void *arg)
{
	int x;

	x = *(int *)arg;
	printf("This is function 3 with arg %d.\n", x);
	fflush(stdout);
	start_timer(T3, 250);
}

void
f4(void *arg)
{
	int x;

	x = *(int *)arg;
	printf("This is function 4 with arg %d.\n", x);
	fflush(stdout);
	start_timer(T4, 2000);
}

void
f(void *arg)
{
	char buffer[1<<16];
	int x;

	x = *(int *)arg;
	read(0, buffer, sizeof(buffer));
	printf("Callback has been called %u times.\n", x);
	fflush(stdout);
	c++;
}

int
main(void) {
	const int i1 = 1;
	const int i2 = 2;
	const int i3 = 3;
	const int i4 = 4;

	init_cblib();
	T1 = create_timer(f1, (void *)&i1, "T1");
	T2 = create_timer(f2, (void *)&i2, "T2");
	T3 = create_timer(f3, (void *)&i3, "T3");
	T4 = create_timer(f4, (void *)&i4, "T4");
#ifdef DEBUG
	print_timer(T1);
	print_timer(T2);
	print_timer(T3);
	print_timer(T4);
#endif
	start_timer(T1, 1000);
	start_timer(T2,  500);
	start_timer(T3,  250);
	start_timer(T4, 2000);
	register_stdin_callback(f, (void *)&c);
	handle_events();
	return (0);
}
