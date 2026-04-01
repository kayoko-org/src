#ifndef SH_WHILE_HH
#define SH_WHILE_HH

#include <sh/command.hh>

class WhileCommand : public Command {
public:
    std::shared_ptr<Command> condition;
    std::shared_ptr<Command> body;
    int execute() override;
};

#endif
