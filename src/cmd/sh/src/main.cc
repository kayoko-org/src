#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <cstring>
#include <setjmp.h>

#include <sh/lexer.hh>
#include <sh/parser.hh>
#include <sh/command.hh>
#include <sh/if.hh>
#include <sh/while.hh>

// POSIX Shell Global State
int last_status = 0;
pid_t shell_pid = getpid();
pid_t last_bg_pid = 0;
const char* shell_name;
static sigjmp_buf ctrlc_buf;

/**
 * SVR4 Error Formatting: argv[0]: cmd: error
 */
void report_error(const std::string& cmd, const std::string& err) {
    std::cerr << shell_name << ": " << cmd << ": " << err << std::endl;
}

/**
 * SIGINT Handler: Wipes the line and restarts the prompt loop
 */
void handle_sigint(int sig) {
    (void)sig; 
    std::cout << "\n" << std::flush;
    siglongjmp(ctrlc_buf, 1);
}

/**
 * Built-in logic for state-changing commands
 */
bool handle_builtins(SimpleCommand* cmd) {
    if (cmd->args.empty()) return false;
    const std::string& name = cmd->args[0];

    // 1. exit [n]
    if (name == "exit") {
        int code = (cmd->args.size() > 1) ? std::stoi(cmd->args[1]) : WEXITSTATUS(last_status);
        exit(code);
    }

    // 2. cd [path | -]
    if (name == "cd") {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            report_error("cd", "cannot determine directory");
            return true;
        }
        
        std::string target;
        bool print_dir = false;

        if (cmd->args.size() > 1) {
            if (cmd->args[1] == "-") {
                char* old = getenv("OLDPWD");
                if (!old) { report_error("cd", "OLDPWD not set"); return true; }
                target = old;
                print_dir = true;
            } else {
                target = cmd->args[1];
            }
        } else {
            char* home = getenv("HOME");
            target = home ? home : "";
        }

        if (chdir(target.c_str()) != 0) {
            report_error("cd", strerror(errno));
            last_status = 256;
        } else {
            setenv("OLDPWD", cwd, 1);
            if (getcwd(cwd, sizeof(cwd))) {
                setenv("PWD", cwd, 1);
                if (print_dir) std::cout << cwd << std::endl;
            }
            last_status = 0;
        }
        return true;
    }

    // 3. export / Assignments
    if (name == "export" || (name.find('=') != std::string::npos && name[0] != '=')) {
        std::string assign = (name == "export" && cmd->args.size() > 1) ? cmd->args[1] : name;
        size_t eq = assign.find('=');
        if (eq != std::string::npos) {
            setenv(assign.substr(0, eq).c_str(), assign.substr(eq+1).c_str(), 1);
        }
        return true;
    }

    if (name == "unset" && cmd->args.size() > 1) {
        unsetenv(cmd->args[1].c_str());
        return true;
    }

    return false;
}

/**
 * Main Shell Entry Point
 */
int main(int argc, char** argv) {
    shell_name = argv[0];
    bool interactive = isatty(STDIN_FILENO);

    if (argc > 1) {
        int fd = open(argv[1], O_RDONLY);
        if (fd < 0) { report_error(argv[1], strerror(errno)); return 1; }
        dup2(fd, STDIN_FILENO);
        close(fd);
        interactive = false;
    }

    if (interactive) {
        struct sigaction sa;
        sa.sa_handler = handle_sigint;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sa, NULL);
        
        // Initialize PS1/PS2 if not set
        if (!getenv("PS1")) setenv("PS1", "# ", 0);
        if (!getenv("PS2")) setenv("PS2", "> ", 0);
    }

    // Jump point for Ctrl+C wiping the line
    sigsetjmp(ctrlc_buf, 1);

    while (true) {
        std::string full_input;
        std::string line;
        bool need_more = false;

        do {
            if (interactive) {
                const char* prompt = need_more ? getenv("PS2") : getenv("PS1");
                std::cout << (prompt ? prompt : (need_more ? "> " : "# ")) << std::flush;
            }

            if (!std::getline(std::cin, line)) {
                if (std::cin.eof()) {
                    if (interactive && full_input.empty()) std::cout << "\n";
                    return WEXITSTATUS(last_status);
                }
                std::cin.clear();
                break;
            }

            full_input += line + " ";

            // Check if the input is syntactically complete
            Lexer lex(full_input);
            auto tokens = lex.tokenize();
            
            // The Parser::is_complete helper checks for hanging 'if', 'while', 'do', etc.
            if (Parser::is_complete(tokens)) {
                need_more = false;
                
                auto root = Parser::parse(tokens);
                if (root) {
                    last_status = root->execute();
                }
            } else {
                need_more = true;
            }

        } while (need_more);
    }

    return WEXITSTATUS(last_status);
}
