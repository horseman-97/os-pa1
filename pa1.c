/**********************************************************************
 * Copyright (c) 2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

#include <string.h>
#include <sys/wait.h>

#include "types.h"
#include "list_head.h"
#include "parser.h"

struct entry{
	struct list_head list;
	char *pcommand;
};
struct list_head history;
static int __process_command(char * command);
/***********************************************************************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */
static int run_command(int nr_tokens, char *tokens[])
{
	// Pipe
	int len = nr_tokens;
	char *firstcommand[len*2];
	char *secondcommand[len*2];
	int status = 0;
	int j = 0;
	int i = 0;
	int pipesymbol = 0;
	for(i = 0; tokens[i]!=NULL; ++i)
	{
		if(strcmp(tokens[i],"|") == 0)
		{
			pipesymbol = true;
			firstcommand[i] = NULL;
			break;
		}
		firstcommand[i] = tokens[i];
	}
	i+=1;
	for(j = 0; tokens[i]!=NULL; ++j)
	{
		secondcommand[j] = tokens[i];
		i++;
	}
	pid_t cpid;
	pid_t cpid2;
	int fd[2];
	if (pipesymbol == 1)
	{
		pipe(fd);
		cpid = fork();
		/*  */ if (cpid < 0)
		{
			fprintf(stderr, "Cannot initialize Pipe\n");
			exit(EXIT_FAILURE);
		} else if (cpid == 0) /* Child1 write command result*/
		{
			close(fd[0]); /* Close unused read end */
			dup2(fd[1], 1); /* Write at the pipe's write_end */
			close(fd[1]); /* Reader see EOF */

			if (execvp(firstcommand[0], firstcommand) < 0)
			{
				fprintf(stderr, "Unable to execute %s\n", firstcommand[0]);
				exit(EXIT_FAILURE);
			}
		} else
		{
			wait(&status);
			cpid2 = fork();
			/*  */ if (cpid2 < 0)
			{
				fprintf(stderr, "fork fail\n");
				exit(EXIT_FAILURE);
			} else if (cpid2 == 0) {/* Child2 read command result*/

				close(fd[1]);				/* Close unused write end*/
				dup2(fd[0], 0); /* Read at the pipe's read_end */
				close(fd[0]);				/* Writer see EOF */
				execvp(secondcommand[0], secondcommand);
				exit(0);
			}
			else
			{
				close(fd[1]);
				wait(&status);
			}
		}
		return 1;
	}
	// Internal
	if (strcmp(tokens[0], "exit") == 0)
	{
		return 0;
	}
	if (pipesymbol==0)
	{
		// cd
		if (strcmp(tokens[0], "cd") == 0)
		{
			// home dir
			/*  */ if (tokens[1] == NULL)
			{
				chdir(getenv("HOME"));
			} else if (strcmp(tokens[1], "~") == 0)
			{
				chdir(getenv("HOME"));
			} else
			{
				chdir(tokens[1]);
			}
			return 1;
		}
		//history
		if (strcmp(tokens[0], "history") == 0)
		{
			struct entry *chistory;
			int num = 0;
			list_for_each_entry_reverse(chistory, &history, list)
			{
				fprintf(stderr, "%2d: %s", num++, chistory->pcommand);
			}
			return 1;
		}
		// !history
		if (strcmp(tokens[0], "!") == 0)
		{
			struct entry *chistory;
			int num = 0;
			list_for_each_entry_reverse(chistory, &history, list)
			{
				if (num == atoi(tokens[1]))
				{
					break;
				} else
				{
					num++;
				}
			}
			char *temp;
			temp = (char *)malloc(strlen(chistory->pcommand));
			strcpy(temp, chistory->pcommand);
			__process_command(temp);
			return 1;
		}
		// External
		pid_t pid;
		pid = fork();
		/*  */ if (pid > 0)
		{
			waitpid(-1, 0, 0);
			return 1;
		} else if (pid == 0)
		{
			if (execvp(tokens[0], tokens) < 0)
			{
				fprintf(stderr, "Unable to execute %s\n", tokens[0]);
				exit(EXIT_FAILURE);
			}
		}
	}
	fprintf(stderr, "Unable to execute %s\n", tokens[0]);
	return -EINVAL;
}


/***********************************************************************
 * struct list_head history
 *
 * DESCRIPTION
 *   Use this list_head to store unlimited command history.
 */
LIST_HEAD(history);


/***********************************************************************
 * append_history()
 *
 * DESCRIPTION
 *   Append @command into the history. The appended command can be later
 *   recalled with "!" built-in command
 */
static void append_history(char * const command)
{
	struct entry *ehistory;

	ehistory = (struct entry *)malloc(sizeof(struct entry));
	ehistory->pcommand = (char *)malloc(strlen(command)+1);

	strcpy(ehistory->pcommand, command);

	INIT_LIST_HEAD(&ehistory->list);
	list_add(&ehistory->list, &history);

}


/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
static int initialize(int argc, char * const argv[])
{
	return 0;
}


/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
static void finalize(int argc, char * const argv[])
{

}


/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING BELOW THIS LINE ******      */
/*          ****** BUT YOU MAY CALL SOME IF YOU WANT TO.. ******      */
static int __process_command(char * command)
{
	char *tokens[MAX_NR_TOKENS] = { NULL };
	int nr_tokens = 0;

	if (parse_command(command, &nr_tokens, tokens) == 0)
		return 1;

	return run_command(nr_tokens, tokens);
}

static bool __verbose = true;
static const char *__color_start = "[0;31;40m";
static const char *__color_end = "[0m";

static void __print_prompt(void)
{
	char *prompt = "$";
	if (!__verbose) return;

	fprintf(stderr, "%s%s%s ", __color_start, prompt, __color_end);
}

/***********************************************************************
 * main() of this program.
 */
int main(int argc, char * const argv[])
{
	char command[MAX_COMMAND_LEN] = { '\0' };
	int ret = 0;
	int opt;

	while ((opt = getopt(argc, argv, "qm")) != -1) {
		switch (opt) {
		case 'q':
			__verbose = false;
			break;
		case 'm':
			__color_start = __color_end = "\0";
			break;
		}
	}

	if ((ret = initialize(argc, argv))) return EXIT_FAILURE;

	/**
	 * Make stdin unbuffered to prevent ghost (buffered) inputs during
	 * abnormal exit after fork()
	 */
	setvbuf(stdin, NULL, _IONBF, 0);

	while (true) {
		__print_prompt();

		if (!fgets(command, sizeof(command), stdin)) break;

		append_history(command);
		ret = __process_command(command);

		if (!ret) break;
	}

	finalize(argc, argv);

	return EXIT_SUCCESS;
}
