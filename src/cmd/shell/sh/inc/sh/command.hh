#ifndef SH_COMMAND_HH
#define SH_COMMAND_HH

#include <string>
#include <vector>
#include <memory>

class Command {
public:
    virtual ~Command() = default;
    virtual int execute() = 0; // Returns exit status
};

// Represents: ls -l /tmp
class SimpleCommand : public Command {
public:
    std::vector<std::string> args;
    std::string input_file, output_file;
    bool append = false;
    int execute() override;
};

// Represents: ls | grep sh
class Pipeline : public Command {
public:
    std::vector<std::shared_ptr<Command>> stages;
    int execute() override;
};

#endif
