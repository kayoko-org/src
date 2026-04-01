#ifndef SH_TOKEN_HH
#define SH_TOKEN_HH

#include <string>
#include <vector>

enum class TokenType {
    WORD, PIPE, REDIRECT_IN, REDIRECT_OUT, APPEND, SEMICOLON, AMPERSAND
};

struct Token {
    TokenType type;
    std::string value;
};

struct Command {
    std::vector<std::string> args;
    std::string input_file;
    std::string output_file;
    bool append = false;
};

#endif
