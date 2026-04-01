#ifndef SH_LEXER_HH
#define SH_LEXER_HH

#include <sh/token.hh>
#include <string>
#include <vector>
#include <set>

class Lexer {
public:
    Lexer(const std::string& input);

    /**
     * Main tokenization loop.
     * @param seen A set of aliases already expanded in this chain to prevent infinite recursion.
     */
    std::vector<Token> tokenize(std::set<std::string> seen = {});

    std::string capture_exec(const std::string& cmd);
    std::string read_word();
    std::string expand_string();
private:
    std::string input_;
    size_t pos_;
    
    std::string get_var_value(const std::string& var_name);
};

#endif
