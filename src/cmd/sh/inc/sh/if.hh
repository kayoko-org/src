#ifndef SH_IF_HH
#define SH_IF_HH

#include <sh/command.hh>

class IfCommand : public Command {
public:
    std::shared_ptr<Command> condition;
    std::shared_ptr<Command> then_part;
    std::shared_ptr<Command> else_part; // Can be nullptr
    int execute() override;
};

#endif
