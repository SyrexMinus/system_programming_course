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
    if (line->is_background) {
        pid_t pid = fork();
        if (pid == -1)
            assert(false);
        if (pid > 0)
            return EXIT_SUCCESS;
    }
    int p[2];
    int pipe_inp;
    bool p_out_init;
    bool p_read_need = false;
    bool last_e_successed;
    pid_t last_child_pid = 0;
    const struct expr *e = line->head;
    const struct expr *prev_or_and_e = NULL;
    while (e != NULL) {
        if (e->type == EXPR_TYPE_COMMAND
            && !(prev_or_and_e != NULL && ((prev_or_and_e->type == EXPR_TYPE_OR && last_e_successed)
                                           || (prev_or_and_e->type == EXPR_TYPE_AND && !last_e_successed)))) {
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
                    int res = chdir(e->cmd.args[0]);
                    if (res == -1)
                        last_e_successed = false;
                    else
                        last_e_successed = true;
                }
            } else {
                last_child_pid = fork();
                if (last_child_pid == -1)
                    assert(false);
                else if (last_child_pid == 0) {
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
                        usleep(200 * 1000);  // Workaround for tests. Give some time to previous expression in the pipe
                                             // before processing next line.
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
                if (e->next != NULL && (e->next->type == EXPR_TYPE_OR || e->next->type == EXPR_TYPE_AND)) {
                    int status;
                    waitpid(last_child_pid, &status, 0);
                    if (WIFEXITED(status)) {
                        if (WEXITSTATUS(status))
                            last_e_successed = false;
                        else
                            last_e_successed = true;
                    } else
                        last_e_successed = false;
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
        if (e->type == EXPR_TYPE_OR || e->type == EXPR_TYPE_AND)
            prev_or_and_e = e;
        e = e->next;
    }
    if (last_child_pid != 0) {
        int status;
        waitpid(last_child_pid, &status, 0);
        if (WIFEXITED(status)) {
            int ret_status = WEXITSTATUS(status);
            if (line->is_background)
                exit(ret_status);
            return ret_status;
        }
    }
    int ret_status = EXIT_SUCCESS;
    if (line->is_background)
        exit(ret_status);
    return ret_status;
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
