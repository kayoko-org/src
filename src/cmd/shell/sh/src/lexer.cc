#include <sh/lexer.hh>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <map>
#include <set>

// Link to globals defined in main.cc
extern int last_status;
extern pid_t shell_pid;
extern pid_t last_bg_pid;
extern std::map<std::string, std::string> aliases;

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
 * Handles backtick command substitution via popen.
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
    
    // Remove trailing newlines
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

/**
 * Reads a single word, handling quotes and variable expansion.
 * Single quotes ('') keep contents literal (supporting ksh-style PS1).
 * Double quotes ("") and unquoted text allow $VAR and `cmd` expansion.
 */
std::string Lexer::read_word() {
    std::string word;
    char quote_char = 0; 

    // 1. Check for Tilde Expansion at the very start
    if (pos_ < input_.length() && input_[pos_] == '~') {
        size_t tilde_pos = pos_ + 1;
        std::string user_name;
        while (tilde_pos < input_.length() && 
               !isspace(static_cast<unsigned char>(input_[tilde_pos])) && 
               !strchr("/|><;&\"\'", input_[tilde_pos])) {
            user_name += input_[tilde_pos];
            tilde_pos++;
        }

        if (user_name.empty()) {
            char* home = std::getenv("HOME");
            word = home ? home : "";
            pos_++;
        } else {
            struct passwd* pw = getpwnam(user_name.c_str());
            if (pw) {
                word = pw->pw_dir;
                pos_ = tilde_pos;
            } else {
                word = "~";
                pos_++;
            }
        }
    }

    while (pos_ < input_.length()) {
        char c = input_[pos_];

        // Handle Single Quotes: NO expansion happens inside
        if (c == '\'' && quote_char == 0) {
            pos_++; 
            while (pos_ < input_.length() && input_[pos_] != '\'') {
                word += input_[pos_];
                pos_++;
            }
            if (pos_ < input_.length()) pos_++;
            continue;
        }

        // Handle Double Quotes: Expansion allowed
        if (c == '\"' && quote_char == 0) {
            quote_char = '\"';
            pos_++;
            continue;
        } else if (c == '\"' && quote_char == '\"') {
            quote_char = 0;
            pos_++;
            continue;
        }

        // Handle Command Substitution (Backticks)
        // Expanded immediately UNLESS we are in single quotes (handled above)
        if (c == '`') {
            pos_++;
            size_t start = pos_;
            while (pos_ < input_.length() && input_[pos_] != '`') {
                if (input_[pos_] == '\\' && pos_ + 1 < input_.length() && input_[pos_+1] == '`') {
                    pos_ += 2;
                } else {
                    pos_++;
                }
            }
            std::string cmd = input_.substr(start, pos_ - start);
            if (pos_ < input_.length()) pos_++;
            
            word += capture_exec(cmd);
            continue;
        }

        // Break on delimiters if not inside quotes
        if (quote_char == 0 && (isspace(static_cast<unsigned char>(c)) || strchr("|><;&", c))) break;

        // Variable expansion
        if (c == '$') {
            pos_++;
            if (pos_ >= input_.length() || 
                isspace(static_cast<unsigned char>(input_[pos_])) || 
                strchr("\"\'", input_[pos_])) {
                word += '$';
            } else if (strchr("$?!", input_[pos_])) {
                word += get_var_value(std::string(1, input_[pos_]));
                pos_++;
            } else {
                size_t start = pos_;
                while (pos_ < input_.length() && 
                      (isalnum(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) {
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

std::vector<Token> Lexer::tokenize(std::set<std::string> seen) {
    std::vector<Token> tokens;
    bool next_is_cmd = true; 

    while (pos_ < input_.length()) {
        char c = input_[pos_];

        if (isspace(static_cast<unsigned char>(c))) {
            pos_++;
            continue;
        }

        if (c == '|') {
            tokens.push_back({TokenType::PIPE, "|"});
            pos_++;
            next_is_cmd = true;
        } else if (c == ';') {
            tokens.push_back({TokenType::SEMICOLON, ";"});
            pos_++;
            next_is_cmd = true;
        } else if (c == '>') {
            if (pos_ + 1 < input_.length() && input_[pos_ + 1] == '>') {
                tokens.push_back({TokenType::APPEND, ">>"});
                pos_ += 2;
            } else {
                tokens.push_back({TokenType::REDIRECT_OUT, ">"});
                pos_++;
            }
            next_is_cmd = false;
        } else if (c == '<') {
            tokens.push_back({TokenType::REDIRECT_IN, "<"});
            pos_++;
            next_is_cmd = false;
        } else {
            std::string val = read_word();
            bool alias_trailing_space = false;

            if (next_is_cmd && aliases.count(val) && seen.find(val) == seen.end()) {
                std::string expanded = aliases[val];
                alias_trailing_space = (!expanded.empty() && expanded.back() == ' ');
                
                std::set<std::string> next_seen = seen;
                next_seen.insert(val);

                Lexer sub_lexer(expanded);
                auto sub_tokens = sub_lexer.tokenize(next_seen);
                
                if (!sub_tokens.empty()) {
                    for (size_t i = 0; i < sub_tokens.size() - 1; ++i) {
                        tokens.push_back(sub_tokens[i]);
                    }
                    val = sub_tokens.back().value; 
                } else {
                    val = ""; 
                }
            }

            if (val.empty()) continue;

            TokenType type = TokenType::WORD;
            if (val == "if") { type = TokenType::IF; next_is_cmd = true; }
            else if (val == "then") { type = TokenType::THEN; next_is_cmd = true; }
            else if (val == "else") { type = TokenType::ELSE; next_is_cmd = true; }
            else if (val == "fi") { type = TokenType::FI; next_is_cmd = false; }
            else if (val == "while") { type = TokenType::WHILE; next_is_cmd = true; }
            else if (val == "do") { type = TokenType::DO; next_is_cmd = true; }
            else if (val == "done") { type = TokenType::DONE; next_is_cmd = false; }
            else {
                next_is_cmd = alias_trailing_space; 
            }

            tokens.push_back({type, val});
        }
    }
    return tokens;
}

/**
 * POSIX-compliant string expansion for prompts (PS1/PS2).
 * This continues reading until the entire input string is processed.
 */
std::string Lexer::expand_string() {
    std::string result;
    while (pos_ < input_.length()) {
        // 1. Skip leading whitespace but preserve it in the output
        while (pos_ < input_.length() && isspace(static_cast<unsigned char>(input_[pos_]))) {
            result += input_[pos_];
            pos_++;
        }

        if (pos_ >= input_.length()) break;

        // 2. Use your existing logic to expand the next "chunk"
        result += read_word();

        // 3. If we stopped at a delimiter (like > or |), add it manually
        // and keep going. This is the key to fixing your PS1.
        if (pos_ < input_.length() && !isspace(static_cast<unsigned char>(input_[pos_]))) {
            // Check if it's an operator that read_word() bailed on
            if (strchr("|><;&", input_[pos_])) {
                result += input_[pos_];
                pos_++;
            }
        }
    }
    return result;
}
