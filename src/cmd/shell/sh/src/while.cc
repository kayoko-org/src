#include <sh/while.hh>

int WhileCommand::execute() {
    int last_ret = 0;
    while (condition->execute() == 0) {
        last_ret = body->execute();
    }
    return last_ret;
}
