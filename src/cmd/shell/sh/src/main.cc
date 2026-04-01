#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>
#include <cstring>

#include <sh/lexer.hh>
#include <sh/parser.hh>

// POSIX Global State
int last_status = 0;
pid_t shell_pid;
pid_t last_bg_pid = 0;
const char* shell_name; 
std::map<std::string, std::string> aliases;
void run_shell(std::istream& input, bool interactive);

/**
 * Helper to report errors using the shell's name (argv[0])
 */
void report_error(const std::string& context, const std::string& message) {
    std::cerr << shell_name << ": " << context << ": " << message << std::endl;
}

/**
 * Built-in logic for state-changing commands.
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
            last_status = 1;
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

    // 3. export / Environment Assignments
    if (name == "export" || (name.find('=') != std::string::npos && name[0] != '=')) {
        std::string assign = (name == "export" && cmd->args.size() > 1) ? cmd->args[1] : name;
        size_t eq = assign.find('=');
        if (eq != std::string::npos) {
            setenv(assign.substr(0, eq).c_str(), assign.substr(eq+1).c_str(), 1);
            last_status = 0;
        }
        return true;
    }

    // 4. alias [name[=value] ...]
    if (name == "alias") {
        last_status = 0;
        if (cmd->args.size() == 1) {
            for (const auto& pair : aliases) {
                std::cout << pair.first << "='" << pair.second << "'" << std::endl;
            }
        } else {
            for (size_t i = 1; i < cmd->args.size(); ++i) {
                size_t eq = cmd->args[i].find('=');
                if (eq != std::string::npos) {
                    aliases[cmd->args[i].substr(0, eq)] = cmd->args[i].substr(eq+1);
                } else {
                    if (aliases.count(cmd->args[i])) {
                        std::cout << cmd->args[i] << "='" << aliases[cmd->args[i]] << "'" << std::endl;
                    } else {
                        report_error("alias", cmd->args[i] + " not found");
                        last_status = 1;
                    }
                }
            }
        }
        return true;
    }

    // 5. unalias [-a] name...
    if (name == "unalias") {
        last_status = 0;
        if (cmd->args.size() == 1) {
            report_error("unalias", "usage: unalias [-a] name...");
            last_status = 2;
            return true;
        }
        for (size_t i = 1; i < cmd->args.size(); ++i) {
            if (cmd->args[i] == "-a") {
                aliases.clear();
                break;
            } else {
                if (aliases.erase(cmd->args[i]) == 0) {
                    report_error("unalias", cmd->args[i] + " not found");
                    last_status = 1;
                }
            }
        }
        return true;
    }

    // 6. . (dot) builtin
    if (name == ".") {
        if (cmd->args.size() < 2) {
            report_error(".", "filename argument required");
            last_status = 1;
            return true;
        }

        std::string path = cmd->args[1];
        std::ifstream script_stream(path);

        // POSIX: If path doesn't contain a '/', search PATH
        if (!script_stream.is_open() && path.find('/') == std::string::npos) {
            char* env_p = getenv("PATH");
            if (env_p) {
                std::string path_list(env_p);
                size_t start = 0, end;
                while ((end = path_list.find(':', start)) != std::string::npos) {
                    std::string full_path = path_list.substr(start, end - start) + "/" + path;
                    script_stream.open(full_path);
                    if (script_stream.is_open()) break;
                    start = end + 1;
                }
                // Check the last entry in PATH
                if (!script_stream.is_open()) {
                    std::string full_path = path_list.substr(start) + "/" + path;
                    script_stream.open(full_path);
                }
            }
        }

        if (!script_stream.is_open()) {
            report_error(".", path + ": not found");
            last_status = 1;
            return true;
        }

        // Execute in CURRENT process context
        // This modifies global 'aliases' and 'last_status' directly
        run_shell(script_stream, false);

        return true;
    }


    return false;
}

/**
 * Signal handler for SIGINT (Ctrl+C)
 */
void handle_sigint(int sig) {
    // 1. Move to a new line
    std::cout << "\n";

    // 2. Get the prompt template
    const char* ps1_raw = getenv("PS1");
    std::string prompt_template = ps1_raw ? ps1_raw : ((geteuid() == 0) ? "# " : "$ ");

    // 3. Use the Lexer to expand backticks/variables (the ksh behavior)
    // Note: We use expand_string() so delimiters like '>' aren't lost
    Lexer prompt_lexer(prompt_template);
    std::string expanded_prompt = prompt_lexer.expand_string();

    // 4. Print and flush
    std::cout << expanded_prompt << std::flush;
}

/**
 * Validates and executes tokens.
 */
void execute_tokens(const std::vector<Token>& tokens) {
    if (tokens.empty()) return;

    TokenType first = tokens[0].type;
    if (first == TokenType::FI || first == TokenType::DONE || 
        first == TokenType::THEN || first == TokenType::DO) {
        std::cerr << shell_name << ": syntax error near unexpected token '" 
                  << tokens[0].value << "'" << std::endl;
        last_status = 2;
        return;
    }

    auto root = Parser::parse(tokens);
    if (root != nullptr) {
        // Here, the AST node's execute() should call handle_builtins internally
        // or you check if root is a SimpleCommand and call handle_builtins here.
        last_status = root->execute();
    } else {
        std::cerr << shell_name << ": syntax error: incomplete or invalid command" << std::endl;
        last_status = 2;
    }
}

/**
 * Loads and executes commands from ~/.shrc
 */
void load_shrc() {
    char* home = getenv("HOME");
    if (!home) return;
    std::string path = std::string(home) + "/.shrc";
    std::ifstream file(path);
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        Lexer lexer(line);
        execute_tokens(lexer.tokenize());
    }
}


/**
 * Core loop logic extracted to handle both cin and file streams.
 */
void run_shell(std::istream& input, bool interactive) {
    std::string line;
    std::string full_input;

    while (true) {
        if (interactive) {
            // 1. Determine which prompt variable to look up
            // PS1 for new commands, PS2 for continuation lines
            const char* prompt_var_name = full_input.empty() ? "PS1" : "PS2";
            const char* prompt_raw_val = getenv(prompt_var_name);
            
            // 2. Define the raw string to expand (fallback to defaults if env is null)
            std::string prompt_template = prompt_raw_val ? prompt_raw_val : (full_input.empty() ? "$ " : "> ");

            // 3. Create a Lexer for the prompt and use the expansion-only method.
            // This preserves characters like '>' and '|' that read_word() would normally skip.
            Lexer prompt_lexer(prompt_template);
            std::string expanded_prompt = prompt_lexer.expand_string();

            // 4. Output the live prompt to the user
            std::cout << expanded_prompt << std::flush;
        }

        // Read line from input (stdin or script file)
        if (!std::getline(input, line)) {
            if (interactive) std::cout << "exit" << std::endl;
            break;
        }

        // Append the new line to our accumulation buffer
        full_input += line;

        // Tokenize the current accumulated input
        Lexer lexer(full_input);
        std::vector<Token> tokens = lexer.tokenize();

        // Handle empty input (just whitespace/newlines)
        if (tokens.empty()) {
            if (full_input.find_first_not_of(" \t\n\r") == std::string::npos) {
                full_input.clear();
            } else {
                full_input += "\n";
            }
            continue;
        }

        // Check if the command is syntactically complete (e.g., closed quotes/brackets)
        if (Parser::is_complete(tokens)) {
            execute_tokens(tokens);
            full_input.clear(); // Reset for the next command
        } else {
            // Command is incomplete; add a newline and loop back to show PS2
            full_input += "\n";
        }
    }
}

int main(int argc, char** argv) {
    shell_name = argv[0];
    shell_pid = getpid();

    // Default to stdin
    bool interactive = isatty(STDIN_FILENO);
    std::ifstream script_file;

    // Check if a filename was passed: sh <filename>
    if (argc > 1) {
        script_file.open(argv[1]);
        if (!script_file.is_open()) {
            report_error(argv[1], "No such file or directory");
            return 1;
        }
        interactive = false; // Scripts are non-interactive
    }

    load_shrc();

    if (interactive) {
        struct sigaction sa;
        sa.sa_handler = handle_sigint;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sa, NULL);

        if (!getenv("PS1")) setenv("PS1", (geteuid() == 0) ? "# " : "$ ", 0);
        if (!getenv("PS2")) setenv("PS2", "> ", 0);
    }

    // Direct input from file or stdin
    if (script_file.is_open()) {
        run_shell(script_file, false);
    } else {
        run_shell(std::cin, interactive);
    }

    return WEXITSTATUS(last_status);
}
