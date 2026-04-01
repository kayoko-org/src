#include <sh/if.hh>

int IfCommand::execute(bool fork_process) {
    // Note: We ignore fork_process here as the IF structure itself 
    // doesn't fork, but its children (SimpleCommands) will.
    if (condition && condition->execute() == 0) {
        return then_part ? then_part->execute() : 0;
    } else if (else_part) {
        return else_part->execute();
    }
    return 0; 
}
