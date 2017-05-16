/*
 * setsid.c -- execute a command in a new session
 * Rick Sladkey <jrs@world.std.com>
 * In the public domain.
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2001-01-18 John Fremlin <vii@penguinpowered.com>
 * - fork in case we are process group leader
 *
 * 2003-07-09 Alistair Phipps <v1aphipp@inf.ed.ac.uk>
 * - removed Native Language Support for easy building on Solaris
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int
main(int argc, char *argv[]) {
	
	if (argc < 2) {
		fprintf(stderr, "usage: %s program [arg ...]\n", argv[0]);
		exit(1);
	}
	if (getpgrp() == getpid()) {
		switch(fork()){
		case -1:
			perror("fork");
			exit(1);
		case 0:		/* child */
			break;
		default:	/* parent */
			exit(0);
		}
	}
	if (setsid() < 0) {
		perror("setsid"); /* cannot happen */
		exit(1);
	}
	execvp(argv[1], argv + 1);
	perror("execvp");
	exit(1);
}
