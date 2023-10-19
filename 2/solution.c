#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc -Wextra -Werror -Wall -Wno-gnu-folding-constant solution.c parser.c -o main
 * $> ./main
 */

static void
execute_command_line(const struct command_line *line)
{
	/* REPLACE THIS CODE WITH ACTUAL COMMAND EXECUTION */

	assert(line != NULL);
	printf("================================\n");
	printf("Command line:\n");
	printf("Is background: %d\n", (int)line->is_background);
	printf("Output: ");
	if (line->out_type == OUTPUT_TYPE_STDOUT) {
		printf("stdout\n");
	} else if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
		printf("new file - \"%s\"\n", line->out_file);
	} else if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
		printf("append file - \"%s\"\n", line->out_file);
	} else {
		assert(false);
	}
	printf("Expressions:\n");
	const struct expr *e = line->head;
	while (e != NULL) {
		if (e->type == EXPR_TYPE_COMMAND) {
			printf("\tCommand: %s", e->cmd.exe);
			for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
				printf(" %s", e->cmd.args[i]);
			printf("\n");
			if (strcmp(e->cmd.exe, "exit") == 0 && e->next == NULL && e->cmd.arg_count == 0) {
                exit(0);
			} else if (strcmp(e->cmd.exe, "cd") == 0) {
			    if (e->cmd.arg_count == 1) {
                    chdir(e->cmd.args[0]);
			    } else if (e->cmd.arg_count > 1) {
                    printf("cd: too many arguments\n");
			    }
			}
		} else if (e->type == EXPR_TYPE_PIPE) {
			printf("\tPIPE\n");
            assert(false);
		} else if (e->type == EXPR_TYPE_AND) {
			printf("\tAND\n");
            assert(false);
		} else if (e->type == EXPR_TYPE_OR) {
			printf("\tOR\n");
            assert(false);
		} else {
			assert(false);
		}
		e = e->next;
	}

	// reference: https://www.opennet.ru/docs/RUS/linux_parallel/node7.html
    pid_t pid;
    switch(pid=fork()) {
        case -1:
            perror("fork");
            exit(1);
        case 0:
            printf(" CHILD: Это процесс-потомок!\n");
            printf(" CHILD: Мой PID -- %d\n", getpid());
            printf(" CHILD: PID моего родителя -- %d\n", getppid());

            if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
                int fd = open(line->out_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            } else if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
                int fd = open(line->out_file, O_WRONLY | (O_APPEND | O_CREAT), S_IRUSR | S_IWUSR);
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            execlp(line->head->cmd.exe, line->head->cmd.exe, NULL);
            printf(" CHILD: command \"%s\" not found\n", line->head->cmd.exe);
            exit(1);
        default:
            printf("PARENT: Это процесс-родитель!\n");
            printf("PARENT: Мой PID -- %d\n", getpid());
            printf("PARENT: PID моего потомка %d\n", pid);
            printf("PARENT: Я жду, пока потомок не вызовет exit()...\n");
            wait(&pid);
            int rv;
            printf("PARENT: Код возврата потомка:%d\n", WEXITSTATUS(rv));
    }

}

int
main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
	struct parser *p = parser_new();
	while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
		parser_feed(p, buf, rc);
		struct command_line *line = NULL;
		while (true) {
			enum parser_error err = parser_pop_next(p, &line);
			if (err == PARSER_ERR_NONE && line == NULL)
				break;
			if (err != PARSER_ERR_NONE) {
				printf("Error: %d\n", (int)err);
				continue;
			}
			execute_command_line(line);
			command_line_delete(line);
		}
	}
	parser_delete(p);
	return 0;
}
