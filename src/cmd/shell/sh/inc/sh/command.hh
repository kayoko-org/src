#ifndef SH_COMMAND_HH
#define SH_COMMAND_HH

#include <string>
#include <vector>
#include <memory>

// Define types of redirection
enum class RedirType {
    IN,      // <
    OUT,     // >
    APPEND,  // >>
    DUP      // >& or <& (e.g., 2>&1)
};

// Structure to hold redirection data
struct Redirection {
    int src_fd;         // The FD to redirect (e.g., 2)
    RedirType type;     // The operator type
    std::string target; // Filename or target FD string (e.g., "1")
};

class Command {
public:
    virtual ~Command() = default;
    
    /**
     * @param fork_process If true, the command manages its own fork.
     * If false, it executes in the current process.
     */
    virtual int execute(bool fork_process = true) = 0;
};

// Represents a single executable and its arguments/redirections
class SimpleCommand : public Command {
public:
    std::vector<std::string> args;
    std::vector<Redirection> redirections;

    int execute(bool fork_process = true) override;
};

// Represents a logical grouping (CMD1 && CMD2 or CMD1 || CMD2)
class LogicalCommand : public Command {
public:
    std::shared_ptr<Command> left;
    std::shared_ptr<Command> right;
    
    // True if operator is '&&', False if operator is '||'
    bool is_and; 

    int execute(bool fork_process = true) override;
};

#endif
