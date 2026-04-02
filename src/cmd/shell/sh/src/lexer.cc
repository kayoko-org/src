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
#include <vector>
#include <string>

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
 * Reads a single word, handling quotes, expansion, and POSIX backslash rules.
 */
std::string Lexer::read_word() {
    std::string word;
    char quote_char = 0; 

    // 1. Tilde Expansion (Unquoted start only)
    if (pos_ < input_.length() && input_[pos_] == '~') {
        size_t tilde_pos = pos_ + 1;
        std::string user_name;
        while (tilde_pos < input_.length() && 
               !isspace(static_cast<unsigned char>(input_[tilde_pos])) && 
               !strchr("/|><;&\"\'\\", input_[tilde_pos])) {
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

        // 2. Handle Backslash based on POSIX context
        if (c == '\\' && quote_char != '\'') { 
            if (pos_ + 1 < input_.length()) {
                char next = input_[pos_ + 1];
                
                if (next == '\n') {
                    // Line continuation: Discard both and move on
                    pos_ += 2;
                    // If we haven't started a word yet, keep skipping
                    if (word.empty() && quote_char == 0) continue; 
                    else continue; 
                }
                
                if (quote_char == '\"') {
                    // Double Quote: \ only escapes $, `, ", \, and newline
                    if (strchr("$`\"\\", next)) {
                        word += next;
                        pos_ += 2;
                    } else {
                        word += c; 
                        pos_++;
                    }
                } else {
                    // Unquoted: Escape literally
                    word += next;
                    pos_ += 2;
                }
                continue;
            } else {
                pos_++;
                continue;
            }
        }

        // 3. Single Quotes
        if (c == '\'' && quote_char == 0) {
            pos_++; 
            while (pos_ < input_.length() && input_[pos_] != '\'') {
                word += input_[pos_];
                pos_++;
            }
            if (pos_ < input_.length()) pos_++;
            continue;
        }

        // 4. Double Quotes
        if (c == '\"') {
            if (quote_char == 0) quote_char = '\"';
            else if (quote_char == '\"') quote_char = 0;
            else word += c; 
            pos_++;
            continue;
        }

        // 5. Backticks
        if (c == '`' && quote_char != '\'') {
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

        // Break on delimiters
        if (quote_char == 0 && (isspace(static_cast<unsigned char>(c)) || strchr("|><;&", c))) {
            break;
        }

        // 6. Variable Expansion
        if (c == '$' && quote_char != '\'') {
            pos_++;
            if (pos_ >= input_.length() || isspace(static_cast<unsigned char>(input_[pos_])) || strchr("\"\'", input_[pos_])) {
                word += '$';
            } else if (strchr("$?!", input_[pos_])) {
                word += get_var_value(std::string(1, input_[pos_]));
                pos_++;
            } else {
                size_t start = pos_;
                while (pos_ < input_.length() && (isalnum(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) {
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

        // 1. Skip whitespace AND Handle POSIX line continuation as "nothing"
        if (isspace(static_cast<unsigned char>(c))) {
            pos_++;
            continue;
        }

        if (c == '\\' && pos_ + 1 < input_.length() && input_[pos_ + 1] == '\n') {
            pos_ += 2;
            continue; // Continue skipping to avoid creating empty tokens
        }

        // 2. Redirections
        if (isdigit(c) || c == '>' || c == '<') {
            size_t temp_pos = pos_;
            std::string fd_prefix = "";
            if (isdigit(c)) {
                while (temp_pos < input_.length() && isdigit(input_[temp_pos])) {
                    fd_prefix += input_[temp_pos];
                    temp_pos++;
                }
            }

            if (temp_pos < input_.length() && (input_[temp_pos] == '>' || input_[temp_pos] == '<')) {
                char op = input_[temp_pos];
                std::string full_op = fd_prefix + op;
                temp_pos++;
                TokenType type = (op == '>') ? TokenType::REDIRECT_OUT : TokenType::REDIRECT_IN;

                if (temp_pos < input_.length()) {
                    if (input_[temp_pos] == '>') {
                        full_op += '>';
                        type = TokenType::APPEND;
                        temp_pos++;
                    } else if (input_[temp_pos] == '&') {
                        full_op += '&';
                        type = TokenType::REDIRECT_DUP;
                        temp_pos++;
                    }
                }
                tokens.push_back({type, full_op});
                pos_ = temp_pos;
                next_is_cmd = false;
                continue;
            }
        }

        // 3. Control Operators
	if (c == '|') {
 	   if (pos_ + 1 < input_.length() && input_[pos_ + 1] == '|') {
    	     tokens.push_back({TokenType::OR_IF, "||"});
   	     pos_ += 2;
    } else {
        tokens.push_back({TokenType::PIPE, "|"});
        pos_++;
    }
    next_is_cmd = true;
	} else if (c == '&') {
  	  if (pos_ + 1 < input_.length() && input_[pos_ + 1] == '&') {
    	     tokens.push_back({TokenType::AND_IF, "&&"});
   	     pos_ += 2;
    } else {
        // Handle single '&' for background execution if your shell supports it
        tokens.push_back({TokenType::AMP, "&"});
        pos_++;
    }
    next_is_cmd = true;
	} else if (c == ';') {
  	   tokens.push_back({TokenType::SEMICOLON, ";"});
           pos_++;
 	   next_is_cmd = true;
	}
	// 4. Words
        else {
            std::string val = read_word();
            if (val.empty()) continue; 

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
            else { next_is_cmd = alias_trailing_space; }

            tokens.push_back({type, val});
        }
    }
    return tokens;
}

std::string Lexer::expand_string() {
    std::string result;
    while (pos_ < input_.length()) {
        while (pos_ < input_.length() && isspace(static_cast<unsigned char>(input_[pos_]))) {
            result += input_[pos_];
            pos_++;
        }
        if (pos_ >= input_.length()) break;
        result += read_word();
        if (pos_ < input_.length() && !isspace(static_cast<unsigned char>(input_[pos_]))) {
            if (strchr("|><;&", input_[pos_])) {
                result += input_[pos_];
                pos_++;
            }
        }
    }
    return result;
}
