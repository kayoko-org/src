#ifndef SH_PIPELINE_HH
#define SH_PIPELINE_HH

#include <sh/command.hh>
#include <vector>
#include <memory>

/**
 * Represents a sequence of commands connected by pipes: cmd1 | cmd2 | cmd3
 */
class PipelineCommand : public Command {
public:
    std::vector<std::shared_ptr<Command>> commands;

    // Overriding the base execute with the matching signature
    int execute(bool fork_process = true) override;
};

#endif
