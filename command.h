#pragma once
#include <string>
#include <vector>
#include "tcp_client.h"

using namespace std;

class CMD {
   private:
    string cmd_line;
    vector<string> cmd_args;
    int cmd_code;

   private:
    int parse_cmd();

   public:
    CMD() = default;
    CMD(string cmd_line);

   public:
    string get_cmd_line();
    vector<string> get_cmd_args();
    int get_cmd_code();
};