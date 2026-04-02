#include <sh/command.hh>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <cstring>

// Global shell state access
extern int last_status;
extern bool handle_builtins(SimpleCommand* cmd);
extern void report_error(const std::string& cmd, const std::string& err);

/**
 * Executes a simple command.
 * @param fork_process If true, the command forks a child. 
 * If false, it executes in the current process (used for pipelines).
 */
int SimpleCommand::execute(bool fork_process) {
    // Built-ins should only be handled in the parent shell if not in a pipeline
    if (fork_process && handle_builtins(this)) {
        return last_status;
    }

    // Lambda to handle redirections and execvp
    auto run_executable = [this]() {
        // 1. Process all redirections in the order they appeared
        for (const auto& redir : redirections) {
            if (redir.type == RedirType::DUP) {
                // Handle FD duplication: e.g., 2>&1
                try {
                    int target_fd = std::stoi(redir.target);
                    if (dup2(target_fd, redir.src_fd) == -1) {
                        perror("dup2");
                        exit(1);
                    }
                } catch (...) {
                    std::cerr << "sh: " << redir.target << ": bad file descriptor" << std::endl;
                    exit(1);
                }
            } else {
                // Handle File redirections: <, >, >>
                int flags = 0;
                if (redir.type == RedirType::OUT) 
                    flags = O_WRONLY | O_CREAT | O_TRUNC;
                else if (redir.type == RedirType::APPEND) 
                    flags = O_WRONLY | O_CREAT | O_APPEND;
                else if (redir.type == RedirType::IN) 
                    flags = O_RDONLY;

                int fd = open(redir.target.c_str(), flags, 0644);
                if (fd < 0) {
                    perror(redir.target.c_str());
                    exit(1);
                }
                
                if (dup2(fd, redir.src_fd) == -1) {
                    perror("dup2");
                    exit(1);
                }
                close(fd);
            }
        }

        // 2. Prepare arguments for execvp
        if (args.empty()) exit(0);

        std::vector<char*> c_args;
        for (auto& a : args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);

        // 3. Replace process image
        execvp(c_args[0], c_args.data());
        
        // If execvp returns, it failed
        report_error(c_args[0], "not found");
        exit(127);
    };

    if (fork_process) {
        // Standalone command mode
        pid_t pid = fork();
        if (pid == 0) {
            run_executable();
        } else if (pid < 0) {
            perror("fork");
            return 1;
        }

        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
        return 0;
    } else {
        // Pipeline mode: PipelineCommand already forked, just run logic
        run_executable();
        return 0; // Execution never reaches here
    }
}


/**
 * Executes a logical AND (&&) or OR (||) command sequence.
 */
int LogicalCommand::execute(bool fork_process) {
    // 1. Execute the left-hand command first
    int status = left->execute(fork_process);
    
    // Update global status so the right-hand command (or variables like $?) 
    // sees the result of the first command immediately.
    last_status = status;

    if (is_and) {
        // AND (&&): Only execute right if left succeeded (status 0)
        if (status == 0) {
            return right->execute(fork_process);
        }
    } else {
        // OR (||): Only execute right if left failed (status != 0)
        if (status != 0) {
            return right->execute(fork_process);
        }
    }

    // If we short-circuited, return the status of the left command
    return status;
}
