#include <sh/lexer.hh>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <sys/wait.h>
#include <unistd.h>
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
 * Handles backtick or $(...) command substitution via popen.
 * Uses POSIX pclose/popen.
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
    
    // Remove trailing newlines to match standard shell behavior
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

        // Handle Quotes
        if ((c == '\'' || c == '\"') && quote_char == 0) {
            quote_char = c;
            pos_++;
            continue;
        } else if (c == quote_char) {
            quote_char = 0;
            pos_++;
            continue;
        }

        // Break on delimiters if not inside quotes
        if (quote_char == 0 && (isspace(static_cast<unsigned char>(c)) || strchr("|><;&", c))) break;

        // Variable expansion (skipping single quotes)
        if (c == '$' && quote_char != '\'') {
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

/**
 * Main tokenization loop with recursive alias support and cycle detection.
 */
std::vector<Token> Lexer::tokenize(std::set<std::string> seen) {
    std::vector<Token> tokens;
    bool next_is_cmd = true; 

    while (pos_ < input_.length()) {
        char c = input_[pos_];

        if (isspace(static_cast<unsigned char>(c))) {
            pos_++;
            continue;
        }

        // Structural Operators
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

            // --- Recursive Alias Expansion ---
            // Only expand if in command position AND not already expanded in this chain
            if (next_is_cmd && aliases.count(val) && seen.find(val) == seen.end()) {
                std::string expanded = aliases[val];
                
                // POSIX Rule: If alias ends in space, expand the next word too
                alias_trailing_space = (!expanded.empty() && expanded.back() == ' ');
                
                // Add current word to 'seen' to prevent infinite recursion
                std::set<std::string> next_seen = seen;
                next_seen.insert(val);

                Lexer sub_lexer(expanded);
                auto sub_tokens = sub_lexer.tokenize(next_seen);
                
                if (!sub_tokens.empty()) {
                    // Push all tokens from the alias expansion except the last one
                    for (size_t i = 0; i < sub_tokens.size() - 1; ++i) {
                        tokens.push_back(sub_tokens[i]);
                    }
                    // The last token's value is used for the current word's processing
                    val = sub_tokens.back().value; 
                } else {
                    val = ""; 
                }
            }

            if (val.empty()) continue;

            TokenType type = TokenType::WORD;

            // Keyword Promotion
            if (val == "if") { type = TokenType::IF; next_is_cmd = true; }
            else if (val == "then") { type = TokenType::THEN; next_is_cmd = true; }
            else if (val == "else") { type = TokenType::ELSE; next_is_cmd = true; }
            else if (val == "fi") { type = TokenType::FI; next_is_cmd = false; }
            else if (val == "while") { type = TokenType::WHILE; next_is_cmd = true; }
            else if (val == "do") { type = TokenType::DO; next_is_cmd = true; }
            else if (val == "done") { type = TokenType::DONE; next_is_cmd = false; }
            else {
                // If previous alias had a trailing space, next word is still a command
                next_is_cmd = alias_trailing_space; 
            }

            tokens.push_back({type, val});
        }
    }
    return tokens;
}
