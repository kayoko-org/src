#include <sh/if.hh>

int IfCommand::execute() {
    // In shell logic, 0 is "true/success"
    if (condition->execute() == 0) {
        return then_part->execute();
    } else if (else_part) {
        return else_part->execute();
    }
    return 0; // if failed but no else, status is success
}
