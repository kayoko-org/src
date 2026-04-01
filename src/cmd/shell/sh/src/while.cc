#include <sh/while.hh>

int WhileCommand::execute(bool fork_process) {
    int last_ret = 0;
    // Shell while loop: continue while condition returns 0 (success)
    while (condition && condition->execute() == 0) {
        if (body) {
            last_ret = body->execute();
        } else {
            break; // Avoid infinite loop if body is null
        }
    }
    return last_ret;
}
