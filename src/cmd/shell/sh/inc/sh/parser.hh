#ifndef SH_PARSER_HH
#define SH_PARSER_HH

#include <sh/token.hh>
#include <sh/command.hh> // Crucial: defines Command class
#include <memory>
#include <vector>

class Parser {
public:
    // Returns the root of the command tree
    static std::shared_ptr<Command> parse(const std::vector<Token>& tokens);

    // Returns true if all if/while blocks are closed
    static bool is_complete(const std::vector<Token>& tokens);
};

#endif
