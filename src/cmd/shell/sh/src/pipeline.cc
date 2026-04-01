#include <sh/pipeline.hh>
#include <sh/command.hh>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <vector>

extern int last_status;

int PipelineCommand::execute(bool fork_process) {
    // Note: fork_process is ignored here because a pipeline always forks its stages
    int num_commands = commands.size();
    int pipe_fds[2];
    int input_fd = 0; // The first command reads from the original stdin
    std::vector<pid_t> pids;

    for (int i = 0; i < num_commands; ++i) {
        // Create pipe for all but the last command
        if (i < num_commands - 1) {
            if (pipe(pipe_fds) == -1) {
                perror("sh: pipe");
                return 1;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            // --- Child Process ---

            // 1. Hook up the input from the previous stage
            if (input_fd != 0) {
                dup2(input_fd, STDIN_FILENO);
 	 close(input_fd);
            }

            // 2. Hook up the output to the next stage
            if (i < num_commands - 1) {
                close(pipe_fds[0]); // Don't need the read end
                dup2(pipe_fds[1], STDOUT_FILENO);
                close(pipe_fds[1]);
            }

            // 3. Execute the command WITHOUT a second fork.
            // This allows '1>&2' to correctly override the pipe.
            _exit(commands[i]->execute(false));

        } else if (pid < 0) {
            perror("sh: fork");
            return 1;
        }

        // --- Parent Process ---
        if (input_fd != 0) close(input_fd);

        if (i < num_commands - 1) {
            close(pipe_fds[1]);
            input_fd = pipe_fds[0]; // Pass read end to the next child
        }
        pids.push_back(pid);
    }

    // Wait for all processes; return the status of the last one
    int status = 0;
    int last_cmd_status = 0;
    for (size_t i = 0; i < pids.size(); ++i) {
        waitpid(pids[i], &status, 0);
        if (i == pids.size() - 1) {
            last_cmd_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        }
    }

    return last_cmd_status;
}
