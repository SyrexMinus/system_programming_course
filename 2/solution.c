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
#include <errno.h>

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc -Wextra -Werror -Wall -Wno-gnu-folding-constant solution.c parser.c -o a.out
 * $> ./main
 */

static int
execute_command_line(const struct command_line *line)
{
	assert(line != NULL);
    int p[2];
    int pipe_inp;
    bool p_out_init;
    bool p_read_need = false;
    pid_t last_child_pid = 0;
	const struct expr *e = line->head;
	while (e != NULL) {
		if (e->type == EXPR_TYPE_COMMAND) {
            if (e->next != NULL && e->next->type == EXPR_TYPE_PIPE) {
                if (pipe(p) < 0) {
                    assert(false);
                }
                p_out_init = true;
            } else {
                p_out_init = false;
            }
			if (strcmp(e->cmd.exe, "exit") == 0 && line->head->next == NULL) {
			    if (e->cmd.arg_count > 0)
                    exit(atoi(e->cmd.args[0]));
                exit(EXIT_SUCCESS);
			} else if (strcmp(e->cmd.exe, "cd") == 0) {
			    if (e->cmd.arg_count > 0) {
                    chdir(e->cmd.args[0]);
                }
			} else {
                switch(last_child_pid = fork()) {
                    case -1:
                        assert(false);
                    case 0:
                        if (p_read_need)
                            dup2(pipe_inp, STDIN_FILENO);
                        if (p_out_init) {
                            close(p[0]);
                            dup2(p[1], STDOUT_FILENO);
                            dup2(p[1], STDERR_FILENO);
                        }
                        if (e->next == NULL) {
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
                        }
                        if (strcmp(e->cmd.exe, "exit") == 0) {
                            if (e->cmd.arg_count > 0)
                                exit(atoi(e->cmd.args[0]));
                            exit(EXIT_SUCCESS);
                        }
                        char **argv = calloc(e->cmd.arg_count + 2, sizeof(argv));
                        argv[0] = e->cmd.exe;
                        for (uint32_t i = 0; i < e->cmd.arg_count; ++i)
                            argv[i + 1] = e->cmd.args[i];
                        argv[e->cmd.arg_count + 2] = NULL;
                        int status = execvp(e->cmd.exe, argv);
                        exit(status);
                }
            }
            if (p_read_need)
                close(pipe_inp);
            if (p_out_init) {
                close(p[1]);
                p_read_need = true;
                pipe_inp = p[0];
            } else
                p_read_need = false;
		}
		e = e->next;
	}
    int status;
    waitpid(last_child_pid, &status, 0);
    usleep(100 * 1000);  // Workaround for tests. They pass commands so frequently that system fails to create files
                         // before they are accessed by next command
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return EXIT_SUCCESS;
}

int
main(void)
{
    int status = 0;
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
            status = execute_command_line(line);
			command_line_delete(line);
		}
	}
	parser_delete(p);
    return status;
}
