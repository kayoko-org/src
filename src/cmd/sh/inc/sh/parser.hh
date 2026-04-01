#ifndef SH_PARSER_HH
#define SH_PARSER_HH

#include <sh/token.hh>

class Parser {
public:
    static std::vector<Command> parse(const std::vector<Token>& tokens);
};

#endif
