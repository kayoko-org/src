#include <sh/parser.hh>
#include <sh/command.hh>
#include <sh/if.hh>
#include <sh/while.hh>
#include <sh/pipeline.hh>
#include <vector>
#include <memory>
#include <iostream>

/**
 * Internal State Tracker to manage token position and provide
 * boundary-safe access to the token stream.
 */
struct ParserState {
    const std::vector<Token>& tokens;
    size_t pos = 0;

    // Safe peek: returns a static empty token if out of bounds to prevent segfaults
    const Token& peek() const { 
        static Token empty_token{TokenType::WORD, ""};
        if (pos >= tokens.size()) return empty_token;
        return tokens[pos]; 
    }

    void advance() { 
        if (pos < tokens.size()) pos++; 
    }

    bool at_end() const { 
        return pos >= tokens.size(); 
    }

    // Returns true and advances if the current token matches the type
    bool expect(TokenType type) {
        if (peek().type == type) {
            advance();
            return true;
        }
        return false;
    }
};

// Forward declarations for recursive descent
std::shared_ptr<Command> parse_pipeline(ParserState& state);
std::shared_ptr<Command> parse_command(ParserState& state);
std::shared_ptr<Command> parse_if(ParserState& state);
std::shared_ptr<Command> parse_while(ParserState& state);
std::shared_ptr<SimpleCommand> parse_simple(ParserState& state);

/**
 * Public: Returns true if the token stream represents a complete grammatical unit.
 * Used by the main loop to determine if PS2 (continuation prompt) is needed.
 */
bool Parser::is_complete(const std::vector<Token>& tokens) {
    if (tokens.empty()) return true;

    // A pipeline cannot end with a trailing pipe character
    if (tokens.back().type == TokenType::PIPE) return false;

    int control_stack = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::IF || t.type == TokenType::WHILE) control_stack++;
        else if (t.type == TokenType::FI || t.type == TokenType::DONE) control_stack--;
    }
    
    // If stack > 0, we have unclosed if/while blocks
    return control_stack <= 0;
}

/**
 * Public: Main entry point for the Parser.
 */
std::shared_ptr<Command> Parser::parse(const std::vector<Token>& tokens) {
    if (tokens.empty()) return nullptr;
    ParserState state{tokens, 0};
    return parse_pipeline(state);
}

/**
 * Parses a pipeline: command | command | command
 */
std::shared_ptr<Command> parse_pipeline(ParserState& state) {
    auto first = parse_command(state);
    if (!first) return nullptr;

    if (state.peek().type == TokenType::PIPE) {
        auto pipe_cmd = std::make_shared<PipelineCommand>();
        pipe_cmd->commands.push_back(first);

        while (state.expect(TokenType::PIPE)) {
            auto next = parse_command(state);
            if (!next) return nullptr; // Syntax error: nothing after pipe
            pipe_cmd->commands.push_back(next);
        }
        return pipe_cmd;
    }

    return first;
}

/**
 * Determines the specific command type to parse.
 */
std::shared_ptr<Command> parse_command(ParserState& state) {
    if (state.at_end()) return nullptr;

    TokenType t = state.peek().type;
    if (t == TokenType::IF) {
        return parse_if(state);
    } else if (t == TokenType::WHILE) {
        return parse_while(state);
    } else if (t == TokenType::FI || t == TokenType::DONE || 
               t == TokenType::THEN || t == TokenType::DO) {
        // Keywords encountered out of context
        return nullptr;
    } else {
        return parse_simple(state);
    }
}

/**
 * Parses IF blocks: if pipeline then pipeline [else pipeline] fi
 */
std::shared_ptr<Command> parse_if(ParserState& state) {
    auto node = std::make_shared<IfCommand>();
    
    state.advance(); // consume 'if'
    
    node->condition = parse_pipeline(state);
    if (!node->condition) return nullptr;
    
    if (!state.expect(TokenType::THEN)) return nullptr;
    
    node->then_part = parse_pipeline(state);
    if (!node->then_part) return nullptr;

    if (state.peek().type == TokenType::ELSE) {
        state.advance(); // consume 'else'
        node->else_part = parse_pipeline(state);
        if (!node->else_part) return nullptr;
    }

    if (!state.expect(TokenType::FI)) return nullptr;
    
    return node;
}

/**
 * Parses WHILE blocks: while pipeline do pipeline done
 */
std::shared_ptr<Command> parse_while(ParserState& state) {
    auto node = std::make_shared<WhileCommand>();
    
    state.advance(); // consume 'while'
    
    node->condition = parse_pipeline(state);
    if (!node->condition) return nullptr;
    
    if (!state.expect(TokenType::DO)) return nullptr;
    
    node->body = parse_pipeline(state);
    if (!node->body) return nullptr;

    if (!state.expect(TokenType::DONE)) return nullptr;
    
    return node;
}

/**
 * Parses a simple command with arguments and redirections.
 */
std::shared_ptr<SimpleCommand> parse_simple(ParserState& state) {
    auto cmd = std::make_shared<SimpleCommand>();
    
    while (!state.at_end()) {
        const Token& t = state.peek();
        
        // Terminate simple command on separators or control keywords
        if (t.type == TokenType::PIPE || t.type == TokenType::SEMICOLON ||
            t.type == TokenType::THEN || t.type == TokenType::ELSE || 
            t.type == TokenType::FI   || t.type == TokenType::DO   || 
            t.type == TokenType::DONE) {
            
            if (t.type == TokenType::SEMICOLON) state.advance();
            break;
        }

        if (t.type == TokenType::WORD) {
            cmd->args.push_back(t.value);
            state.advance();
        } else if (t.type == TokenType::REDIRECT_OUT || t.type == TokenType::APPEND) {
            bool is_append = (t.type == TokenType::APPEND);
            state.advance();
            if (state.peek().type == TokenType::WORD) {
                cmd->output_file = state.peek().value;
                cmd->append = is_append;
                state.advance();
            } else {
                return nullptr; // Syntax error: no file for redirection
            }
        } else if (t.type == TokenType::REDIRECT_IN) {
            state.advance();
            if (state.peek().type == TokenType::WORD) {
                cmd->input_file = state.peek().value;
                state.advance();
            } else {
                return nullptr;
            }
        } else {
            // Skip unknown tokens to avoid infinite loops
            state.advance();
        }
    }
    
    // Valid simple command must have at least one argument (the executable)
    return (cmd->args.empty()) ? nullptr : cmd;
}
