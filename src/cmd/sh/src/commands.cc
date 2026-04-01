#include <sh/command.hh>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <iostream>

// Global shell state access
extern int last_status;
extern bool handle_builtins(SimpleCommand* cmd);
extern void report_error(const std::string& cmd, const std::string& err);

int SimpleCommand::execute() {
    if (handle_builtins(this)) return last_status;

    pid_t pid = fork();
    if (pid == 0) {
        if (!output_file.empty()) {
            int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
            int fd = open(output_file.c_str(), flags, 0644);
            dup2(fd, STDOUT_FILENO); close(fd);
        }
        std::vector<char*> c_args;
        for (auto& a : args) c_args.push_back(const_cast<char*>(a.c_str()));
        c_args.push_back(nullptr);
        execvp(c_args[0], c_args.data());
        report_error(c_args[0], "not found");
        exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

int Pipeline::execute() {
    int n = stages.size();
    int pipefds[2 * (n - 1)];
    for (int i = 0; i < n - 1; i++) pipe(pipefds + i * 2);

    for (int i = 0; i < n; i++) {
        // This is complex: stages[i] might be a SimpleCommand or another If/While!
        // For pure POSIX, only SimpleCommands usually sit in pipes.
        pid_t pid = fork();
        if (pid == 0) {
            if (i > 0) dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            if (i < n - 1) dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            for (int j = 0; j < 2 * (n - 1); j++) close(pipefds[j]);
            exit(stages[i]->execute());
        }
    }
    for (int i = 0; i < 2 * (n - 1); i++) close(pipefds[i]);
    int status;
    for (int i = 0; i < n; i++) wait(&status);
    return WEXITSTATUS(status);
}
