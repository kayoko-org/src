#include <sh/lexer.hh>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cwctype>
#include <sys/wait.h>
#include <unistd.h>

// Link to globals in main.cc
extern int last_status;
extern pid_t shell_pid;
extern pid_t last_bg_pid;

Lexer::Lexer(const std::string& input) : input_(input), pos_(0) {}

/**
 * Resolves special shell variables ($?, $$, $!) and environment variables.
 */
std::string Lexer::get_var_value(const std::string& var_name) {
    if (var_name == "?") {
        if (WIFEXITED(last_status)) return std::to_string(WEXITSTATUS(last_status));
        if (WIFSIGNALED(last_status)) return std::to_string(128 + WTERMSIG(last_status));
        return "0";
    }
    if (var_name == "$") return std::to_string(shell_pid);
    if (var_name == "!") return last_bg_pid > 0 ? std::to_string(last_bg_pid) : "";
    
    char* val = std::getenv(var_name.c_str());
    return val ? std::string(val) : "";
}

/**
 * Handles backtick and $(...) command substitution via popen.
 */
std::string Lexer::capture_exec(const std::string& cmd) {
    std::string result;
    char buffer[128];
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
    pclose(pipe);
    // Remove trailing newlines to match POSIX behavior
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

/**
 * Reads a single word, handling quotes and variable expansion.
 */
std::string Lexer::read_word() {
    std::string word;
    char quote_char = 0; 

    while (pos_ < input_.length()) {
        char c = input_[pos_];

        // Toggle quotes
        if ((c == '\'' || c == '\"') && quote_char == 0) {
            quote_char = c;
            pos_++;
            continue;
        } else if (c == quote_char) {
            quote_char = 0;
            pos_++;
            continue;
        }

        // Break on delimiters if not quoted
        if (quote_char == 0 && (isspace(c) || strchr("|><;&", c))) break;

        // Variable expansion (skipping single quotes)
        if (c == '$' && quote_char != '\'') {
            pos_++;
            if (pos_ >= input_.length() || isspace(input_[pos_]) || strchr("\"\'", input_[pos_])) {
                word += '$';
            } else if (strchr("$?!", input_[pos_])) {
                word += get_var_value(std::string(1, input_[pos_]));
                pos_++;
            } else {
                size_t start = pos_;
                while (pos_ < input_.length() && (isalnum(input_[pos_]) || input_[pos_] == '_')) {
                    pos_++;
                }
                word += get_var_value(input_.substr(start, pos_ - start));
            }
            continue;
        }

        word += c;
        pos_++;
    }
    return word;
}

/**
 * Main tokenization loop.
 */
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (pos_ < input_.length()) {
        char c = input_[pos_];

        if (isspace(c)) {
            pos_++;
            continue;
        }

        // Handle structural operators
        if (c == '|') {
            tokens.push_back({TokenType::PIPE, "|"});
            pos_++;
        } else if (c == ';') {
            tokens.push_back({TokenType::SEMICOLON, ";"});
            pos_++;
        } else if (c == '>') {
            if (pos_ + 1 < input_.length() && input_[pos_ + 1] == '>') {
                tokens.push_back({TokenType::APPEND, ">>"});
                pos_ += 2;
            } else {
                tokens.push_back({TokenType::REDIRECT_OUT, ">"});
                pos_++;
            }
        } else if (c == '<') {
            tokens.push_back({TokenType::REDIRECT_IN, "<"});
            pos_++;
        } else {
            // Process a WORD and check for KEYWORD promotion
            std::string val = read_word();
            TokenType type = TokenType::WORD;

            // Reserved words promotion (Magic)
            if (val == "if") type = TokenType::IF;
            else if (val == "then") type = TokenType::THEN;
            else if (val == "else") type = TokenType::ELSE;
            else if (val == "fi") type = TokenType::FI;
            else if (val == "while") type = TokenType::WHILE;
            else if (val == "do") type = TokenType::DO;
            else if (val == "done") type = TokenType::DONE;

            tokens.push_back({type, val});
        }
    }
    return tokens;
}
