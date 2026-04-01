#ifndef SH_LEXER_HH
#define SH_LEXER_HH

#include <sh/token.hh>
#include <string>
#include <vector>

class Lexer {
public:
    Lexer(const std::string& input);
    std::vector<Token> tokenize();

private:
    std::string input_;
    size_t pos_;
    
    std::string read_word();
    std::string get_var_value(const std::string& var_name);
    std::string capture_exec(const std::string& cmd);
};

#endif
