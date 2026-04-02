#include <sh/parser.hh>
#include <sh/command.hh>
#include <sh/if.hh>
#include <sh/while.hh>
#include <sh/pipeline.hh>
#include <vector>
#include <memory>
#include <iostream>
#include <cctype>

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
std::shared_ptr<Command> parse_logical(ParserState& state);
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

    // Cannot end with a connector
    TokenType last = tokens.back().type;
    if (last == TokenType::PIPE || last == TokenType::AND_IF || last == TokenType::OR_IF) {
        return false;
    }

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
    return parse_logical(state);
}

/**
 * Parses logical operators: pipeline && pipeline || pipeline
 */
std::shared_ptr<Command> parse_logical(ParserState& state) {
    auto left = parse_pipeline(state);
    if (!left) return nullptr;

    while (state.peek().type == TokenType::AND_IF || state.peek().type == TokenType::OR_IF) {
        TokenType op_type = state.peek().type;
        state.advance(); // consume && or ||

        auto right = parse_pipeline(state);
        if (!right) return nullptr; // Syntax error: trailing operator

        auto logical_cmd = std::make_shared<LogicalCommand>();
        logical_cmd->left = left;
        logical_cmd->right = right;
        logical_cmd->is_and = (op_type == TokenType::AND_IF);
        
        // Wrap the result to allow chaining (e.g., A && B || C)
        left = logical_cmd;
    }

    return left;
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
 * Parses IF blocks: if logical then logical [else logical] fi
 */
std::shared_ptr<Command> parse_if(ParserState& state) {
    auto node = std::make_shared<IfCommand>();
    
    state.advance(); // consume 'if'
    
    node->condition = parse_logical(state);
    if (!node->condition) return nullptr;
    
    if (!state.expect(TokenType::THEN)) return nullptr;
    
    node->then_part = parse_logical(state);
    if (!node->then_part) return nullptr;

    if (state.peek().type == TokenType::ELSE) {
        state.advance(); // consume 'else'
        node->else_part = parse_logical(state);
        if (!node->else_part) return nullptr;
    }

    if (!state.expect(TokenType::FI)) return nullptr;
    
    return node;
}

/**
 * Parses WHILE blocks: while logical do logical done
 */
std::shared_ptr<Command> parse_while(ParserState& state) {
    auto node = std::make_shared<WhileCommand>();
    
    state.advance(); // consume 'while'
    
    node->condition = parse_logical(state);
    if (!node->condition) return nullptr;
    
    if (!state.expect(TokenType::DO)) return nullptr;
    
    node->body = parse_logical(state);
    if (!node->body) return nullptr;

    if (!state.expect(TokenType::DONE)) return nullptr;
    
    return node;
}

/**
 * Parses a simple command with arguments and redirections.
 * Updated to support multiple redirections, FD duplication (e.g., 2>&1),
 * and look-back parsing to handle separated leading digits (e.g., the '1' in '1>&2').
 */
std::shared_ptr<SimpleCommand> parse_simple(ParserState& state) {
    auto cmd = std::make_shared<SimpleCommand>();

    while (!state.at_end()) {
        const Token& t = state.peek();

        // Terminate simple command on separators or control keywords
        if (t.type == TokenType::PIPE   || t.type == TokenType::SEMICOLON ||
            t.type == TokenType::AND_IF || t.type == TokenType::OR_IF     ||
            t.type == TokenType::THEN   || t.type == TokenType::ELSE      ||
            t.type == TokenType::FI     || t.type == TokenType::DO        ||
            t.type == TokenType::DONE) {

            if (t.type == TokenType::SEMICOLON) state.advance();
            break;
        }

        if (t.type == TokenType::WORD) {
            cmd->args.push_back(t.value);
            state.advance();
        }
        // Handle Redirections: >, >>, <, 2>, 2>&1, etc.
        else if (t.type == TokenType::REDIRECT_OUT || t.type == TokenType::APPEND ||
                 t.type == TokenType::REDIRECT_IN  || t.type == TokenType::REDIRECT_DUP) {

            Redirection redir;
            int manual_fd = -1;

            // Look-back: Check if the previous argument was just a single digit.
            // If so, it's likely the src_fd for this redirection (e.g., the '1' in '1>&2').
        	if (!cmd->args.empty() && cmd->args.back().size() == 1 && 
   		 std::isdigit(static_cast<unsigned char>(cmd->args.back()[0]))) {    
	           manual_fd = cmd->args.back()[0] - '0';
                   cmd->args.pop_back(); // Remove it from arguments
            }

            // 1. Determine the source FD and RedirType
            if (t.type == TokenType::REDIRECT_OUT) {
                redir.type = RedirType::OUT;
                redir.src_fd = (manual_fd != -1) ? manual_fd : 1;
            } else if (t.type == TokenType::APPEND) {
                redir.type = RedirType::APPEND;
                redir.src_fd = (manual_fd != -1) ? manual_fd : 1;
            } else if (t.type == TokenType::REDIRECT_IN) {
                redir.type = RedirType::IN;
                redir.src_fd = (manual_fd != -1) ? manual_fd : 0;
            } else if (t.type == TokenType::REDIRECT_DUP) {
                redir.type = RedirType::DUP;
                redir.src_fd = (manual_fd != -1) ? manual_fd : 1;
            }

            state.advance(); // consume the operator (e.g., ">" or ">&")

            // 2. Determine the target (filename or target FD)
            if (state.peek().type == TokenType::WORD) {
                redir.target = state.peek().value;
                cmd->redirections.push_back(redir);
                state.advance();
            } else {
                // Syntax error: redirection operator not followed by a word/FD
                return nullptr;
            }
        } else {
            // Skip unknown tokens
            state.advance();
        }
    }

    return (cmd->args.empty() && cmd->redirections.empty()) ? nullptr : cmd;
}
