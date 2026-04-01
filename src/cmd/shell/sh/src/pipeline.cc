#include <sh/pipeline.hh>
#include <sh/command.hh>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <vector>

extern int last_status;

int PipelineCommand::execute() {
    int num_commands = commands.size();
    int pipe_fds[2];
    int input_fd = 0; // Initially, the first process reads from stdin
    std::vector<pid_t> pids;

    for (int i = 0; i < num_commands; ++i) {
        // Create a pipe for all but the last command
        if (i < num_commands - 1) {
            if (pipe(pipe_fds) == -1) {
                perror("pipe");
                return 1;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            // --- Child Process ---

            // 1. Redirect input from the previous pipe's read end
            if (input_fd != 0) {
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }

            // 2. Redirect output to the current pipe's write end
            if (i < num_commands - 1) {
                close(pipe_fds[0]); // Child doesn't need to read from its own pipe
                dup2(pipe_fds[1], STDOUT_FILENO);
                close(pipe_fds[1]);
            }

            // Execute the specific command (Simple, If, While, etc.)
            // Note: If execute() normally forks, this will result in double-forking.
            // For pipelines, SimpleCommand::execute() should ideally have a 
            // mode that avoids forking if it's already in a subshell.
            _exit(commands[i]->execute());

        } else if (pid < 0) {
            perror("fork");
            return 1;
        }

        // --- Parent Process ---

        // Close the read end of the *previous* pipe in the parent
        if (input_fd != 0) {
            close(input_fd);
        }

        // Close the write end of the *current* pipe in the parent
        if (i < num_commands - 1) {
            close(pipe_fds[1]);
            // Save the read end for the next command's input
            input_fd = pipe_fds[0];
        }

        pids.push_back(pid);
    }

    // Wait for all processes in the pipeline
    int status = 0;
    for (pid_t pid : pids) {
        waitpid(pid, &status, 0);
    }

    // The shell status is typically the exit status of the LAST command
    return status;
}
