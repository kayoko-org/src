#include <sh/parser.hh>

std::vector<Command> Parser::parse(const std::vector<Token>& tokens) {
    std::vector<Command> pipeline;
    Command current;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& t = tokens[i];
        switch (t.type) {
            case TokenType::WORD: current.args.push_back(t.value); break;
            case TokenType::PIPE:
                pipeline.push_back(current);
                current = Command();
                break;
            case TokenType::REDIRECT_OUT:
            case TokenType::APPEND:
                if (i + 1 < tokens.size()) {
                    current.output_file = tokens[++i].value;
                    current.append = (t.type == TokenType::APPEND);
                }
                break;
            case TokenType::REDIRECT_IN:
                if (i + 1 < tokens.size()) current.input_file = tokens[++i].value;
                break;
            case TokenType::AMPERSAND:
                current.background = true;
                break;
            default: break;
        }
    }
    if (!current.args.empty()) pipeline.push_back(current);
    return pipeline;
}
