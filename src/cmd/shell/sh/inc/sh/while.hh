#ifndef SH_WHILE_HH
#define SH_WHILE_HH

#include <sh/command.hh>
#include <memory>

class WhileCommand : public Command {
public:
    std::shared_ptr<Command> condition;
    std::shared_ptr<Command> body;
    
    // Updated signature
    int execute(bool fork_process = true) override;
};

#endif
