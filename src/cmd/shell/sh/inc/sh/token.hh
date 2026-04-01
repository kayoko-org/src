#ifndef SH_TOKEN_HH
#define SH_TOKEN_HH

#include <string>

/**
 * TokenType defines the atoms of our shell grammar.
 */
enum class TokenType {
    // Basic types
    WORD,           // Any string (ls, "hello", /tmp, $VAR)
    
    // Operators
    PIPE,           // |
    SEMICOLON,      // ;
    REDIRECT_IN,    // <
    REDIRECT_OUT,   // >
    APPEND,         // >>
    REDIRECT_DUP,   // >& or <& (e.g., 2>&1)
    
    // Control Flow
    IF,             // if
    THEN,           // then
    ELSE,           // else
    FI,             // fi
    WHILE,          // while
    DO,             // do
    DONE,           // done
    
    // Grouping
    LBRACE,         // {
    RBRACE          // }
};

struct Token {
    TokenType type;
    std::string value;
};

#endif
