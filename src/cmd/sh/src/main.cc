#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <cstring>
#include <sh/lexer.hh>
#include <sh/parser.hh>

// POSIX Shell State
int last_status = 0;
pid_t shell_pid = getpid();
pid_t last_bg_pid = 0;
const char* shell_name;

void report_error(const std::string& cmd, const std::string& err) {
    std::cerr << shell_name << ": " << cmd << ": " << err << std::endl;
}

bool handle_builtins(const Command& cmd) {
    if (cmd.args.empty()) return false;
    const std::string& name = cmd.args[0];

    // 1. exit [n]
    // Exits the shell with a given status, or the status of the last command.
    if (name == "exit") {
        int code = (cmd.args.size() > 1) ? std::stoi(cmd.args[1]) : WEXITSTATUS(last_status);
        exit(code);
    }

    // 2. cd [path | -]
    // Handles directory changes and updates PWD/OLDPWD environment variables.
    if (name == "cd") {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            report_error("cd", "cannot determine current directory");
            last_status = 256;
            return true;
        }

        std::string target;
        bool print_dir = false;

        if (cmd.args.size() > 1) {
            if (cmd.args[1] == "-") {
                char* old = getenv("OLDPWD");
                if (!old) {
                    report_error("cd", "OLDPWD not set");
                    last_status = 256;
                    return true;
                }
                target = old;
                print_dir = true; // POSIX: cd - must print the new directory
            } else {
                target = cmd.args[1];
            }
        } else {
            char* home = getenv("HOME");
            if (!home) {
                report_error("cd", "HOME not set");
                last_status = 256;
                return true;
            }
            target = home;
        }

        if (chdir(target.c_str()) != 0) {
            report_error("cd", strerror(errno));
            last_status = 256;
        } else {
            // Update environment for sub-processes and $PWD/$OLDPWD expansion
            setenv("OLDPWD", cwd, 1);
            char new_cwd[PATH_MAX];
            if (getcwd(new_cwd, sizeof(new_cwd)) != NULL) {
                setenv("PWD", new_cwd, 1);
                if (print_dir) std::cout << new_cwd << std::endl;
            }
            last_status = 0;
        }
        return true;
    }

    // 3. export VAR=VAL or VAR=VAL
    // Handles environment variable assignments.
    if (name == "export" || (name.find('=') != std::string::npos && name[0] != '=')) {
        std::string assignment;
        if (name == "export") {
            if (cmd.args.size() < 2) return true; // Plain 'export' usually lists env, skipped for brevity
            assignment = cmd.args[1];
        } else {
            assignment = name;
        }

        size_t eq = assignment.find('=');
        if (eq != std::string::npos) {
            std::string var = assignment.substr(0, eq);
            std::string val = assignment.substr(eq + 1);
            if (setenv(var.c_str(), val.c_str(), 1) != 0) {
                report_error(name, strerror(errno));
                last_status = 256;
            } else {
                last_status = 0;
            }
        }
        return true;
    }

    // 4. unset VAR
    // Removes a variable from the environment.
    if (name == "unset") {
        if (cmd.args.size() > 1) {
            if (unsetenv(cmd.args[1].c_str()) != 0) {
                report_error("unset", strerror(errno));
                last_status = 256;
            } else {
                last_status = 0;
            }
        }
        return true;
    }

    return false;
}

void run_pipeline(const std::vector<Command>& pipeline) {
    int n = pipeline.size();
    int pipefds[2 * (n - 1)];
    for (int i = 0; i < n - 1; i++) pipe(pipefds + i * 2);

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            if (i > 0) dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            if (i < n - 1) dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            for (int j = 0; j < 2 * (n - 1); j++) close(pipefds[j]);

            if (!pipeline[i].output_file.empty()) {
                int f = O_WRONLY | O_CREAT | (pipeline[i].append ? O_APPEND : O_TRUNC);
                int fd = open(pipeline[i].output_file.c_str(), f, 0644);
                if (fd < 0) { report_error(pipeline[i].output_file, strerror(errno)); exit(1); }
                dup2(fd, STDOUT_FILENO); close(fd);
            }

            std::vector<char*> args;
            for (const auto& a : pipeline[i].args) args.push_back(const_cast<char*>(a.c_str()));
            args.push_back(nullptr);

            execvp(args[0], args.data());
            report_error(args[0], "not found");
            exit(127);
        }
    }
    for (int i = 0; i < 2 * (n - 1); i++) close(pipefds[i]);
    for (int i = 0; i < n; i++) wait(&last_status);
}

int main(int argc, char** argv) {
    shell_name = argv[0];
    signal(SIGINT, SIG_IGN);

    while (true) {
        char* ps1 = getenv("PS1");
        std::cout << (ps1 ? ps1 : "# ") << std::flush;

        std::string line;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        Lexer lex(line);
        auto pipeline = Parser::parse(lex.tokenize());
        if (pipeline.empty()) continue;

        if (pipeline.size() == 1 && handle_builtins(pipeline[0])) continue;
        run_pipeline(pipeline);
    }
    return 0;
}
