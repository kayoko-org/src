#include <sh/parser.hh>
#include <sh/command.hh>
#include <sh/if.hh>
#include <sh/while.hh>
#include <vector>
#include <memory>

/**
 * Helper to manage the current token position during recursion
 */
struct ParserState {
    const std::vector<Token>& tokens;
    size_t pos = 0;

    const Token& peek() const { return tokens[pos]; }
    void advance() { if (pos < tokens.size()) pos++; }
    bool at_end() const { return pos >= tokens.size(); }
};

// Internal recursive functions
std::shared_ptr<Command> parse_command(ParserState& state);
std::shared_ptr<Command> parse_if(ParserState& state);
std::shared_ptr<Command> parse_while(ParserState& state);
std::shared_ptr<SimpleCommand> parse_simple(ParserState& state);

/**
 * Public: Returns true if all opened blocks (if/while) have been closed.
 */
bool Parser::is_complete(const std::vector<Token>& tokens) {
    int control_stack = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::IF || t.type == TokenType::WHILE) control_stack++;
        else if (t.type == TokenType::FI || t.type == TokenType::DONE) control_stack--;
    }
    return control_stack <= 0;
}

/**
 * Public: Entry point for the Recursive Descent Parser
 */
std::shared_ptr<Command> Parser::parse(const std::vector<Token>& tokens) {
    if (tokens.empty()) return nullptr;
    ParserState state{tokens, 0};
    return parse_command(state);
}

std::shared_ptr<Command> parse_command(ParserState& state) {
    if (state.at_end()) return nullptr;

    if (state.peek().type == TokenType::IF) {
        return parse_if(state);
    } else if (state.peek().type == TokenType::WHILE) {
        return parse_while(state);
    } else {
        return parse_simple(state);
    }
}

std::shared_ptr<Command> parse_if(ParserState& state) {
    auto node = std::make_shared<IfCommand>();
    
    state.advance(); // consume 'if'
    node->condition = parse_command(state);
    
    if (!state.at_end() && state.peek().type == TokenType::THEN) {
        state.advance(); // consume 'then'
        node->then_part = parse_command(state);
    }

    if (!state.at_end() && state.peek().type == TokenType::ELSE) {
        state.advance(); // consume 'else'
        node->else_part = parse_command(state);
    }

    if (!state.at_end() && state.peek().type == TokenType::FI) {
        state.advance(); // consume 'fi'
    }
    
    return node;
}

std::shared_ptr<Command> parse_while(ParserState& state) {
    auto node = std::make_shared<WhileCommand>();
    
    state.advance(); // consume 'while'
    node->condition = parse_command(state);
    
    if (!state.at_end() && state.peek().type == TokenType::DO) {
        state.advance(); // consume 'do'
        node->body = parse_command(state);
    }

    if (!state.at_end() && state.peek().type == TokenType::DONE) {
        state.advance(); // consume 'done'
    }
    
    return node;
}

std::shared_ptr<SimpleCommand> parse_simple(ParserState& state) {
    auto cmd = std::make_shared<SimpleCommand>();
    
    while (!state.at_end()) {
        const Token& t = state.peek();
        
        // Stop if we hit a control flow keyword or separator
        if (t.type == TokenType::THEN || t.type == TokenType::ELSE || 
            t.type == TokenType::FI || t.type == TokenType::DO || 
            t.type == TokenType::DONE || t.type == TokenType::SEMICOLON) {
            if (t.type == TokenType::SEMICOLON) state.advance();
            break;
        }

        if (t.type == TokenType::WORD) {
            cmd->args.push_back(t.value);
        } else if (t.type == TokenType::REDIRECT_OUT) {
            state.advance();
            if (!state.at_end()) cmd->output_file = state.peek().value;
        } else if (t.type == TokenType::APPEND) {
            state.advance();
            cmd->append = true;
            if (!state.at_end()) cmd->output_file = state.peek().value;
        } else if (t.type == TokenType::REDIRECT_IN) {
            state.advance();
            if (!state.at_end()) cmd->input_file = state.peek().value;
        }
        
        state.advance();
    }
    
    return cmd->args.empty() ? nullptr : cmd;
}
