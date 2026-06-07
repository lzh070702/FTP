#include "command.h"

int CMD::parse_cmd() {
    cmd_line += ' ';
    string arg;
    for (auto ch : cmd_line) {
        if (ch != ' ') {
            arg += ch;
        } else if (!arg.empty()) {
            cmd_args.push_back(arg);
            arg.clear();
        }
    }
    if (!cmd_args.empty()) {
        if (cmd_args[0] == "lcd" && cmd_args.size() == 2) {
            return -3;
        } else if (cmd_args[0] == "!pwd" && cmd_args.size() == 1) {
            return -2;
        } else if (cmd_args[0] == "!ls") {
            return -1;
        } else if ((cmd_args[0] == "quit" || cmd_args[0] == "bye" ||
                    cmd_args[0] == "exit") &&
                   cmd_args.size() == 1) {
            cmd_args[0] = "quit";
            return 1;
        } else if (cmd_args[0] == "cd" && cmd_args.size() == 2) {
            return 2;
        } else if (cmd_args[0] == "pwd" && cmd_args.size() == 1) {
            return 3;
        } else if (cmd_args[0] == "ls" && cmd_args.size() == 1) {
            return 4;
        } else if (cmd_args[0] == "get" && cmd_args.size() == 3) {
            return 5;
        } else if (cmd_args[0] == "put" && cmd_args.size() == 3) {
            return 6;
        }
        return 0;
    }
    return 0;
}

CMD::CMD(string cmd_line) : cmd_line(move(cmd_line)) {
    cmd_code = parse_cmd();
    cmd_line.clear();
    for (const auto& arg : cmd_args) {
        cmd_line += arg + " ";
    }
}

string CMD::get_cmd_line() {
    return cmd_line;
}

int CMD::get_cmd_code() {
    return cmd_code;
}

vector<string> CMD::get_cmd_args() {
    return cmd_args;
}