#ifndef SH_TOKEN_HH
#define SH_TOKEN_HH

#include <string>

/**
 * TokenType defines the atoms of our shell grammar.
 * Keywords like IF/WHILE are treated as special WORDs by the Lexer
 * only when they are not quoted.
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
    
    // Control Flow "Magic"
    IF,             // if
    THEN,           // then
    ELSE,           // else
    FI,             // fi
    WHILE,          // while
    DO,             // do
    DONE,           // done
    
    // Grouping (for future expansion)
    LBRACE,         // {
    RBRACE          // }
};

/**
 * Token structure pairs the type with its literal string value.
 * This is passed from the Lexer to the Parser.
 */
struct Token {
    TokenType type;
    std::string value;
};

#endif
