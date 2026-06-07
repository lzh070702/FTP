#pragma once
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;

class TCPclient {
   private:
    int fd;

   public:
    TCPclient() = default;
    TCPclient(const string& ip, int port_num);

   public:
    int get_fd();
    void set_fd(int new_fd);
};