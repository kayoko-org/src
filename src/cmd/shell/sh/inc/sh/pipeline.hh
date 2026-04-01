#ifndef SH_PIPELINE_HH
#define SH_PIPELINE_HH

#include <sh/command.hh>
#include <vector>
#include <memory>

/**
 * PipelineCommand represents a series of commands connected by pipes (|).
 * Example: ls | grep ".cc" | wc -l
 */
class PipelineCommand : public Command {
public:
    PipelineCommand() = default;
    virtual ~PipelineCommand() = default;

    /**
     * Executes each command in the pipeline, setting up pipes 
     * between them and waiting for the final command in the chain.
     */
    virtual int execute() override;

    // The list of commands in the pipeline
    std::vector<std::shared_ptr<Command>> commands;
};

#endif
