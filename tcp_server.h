#pragma once
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

using namespace std;

class TCPserver {
   private:
    int lfd;
    int cfd;

   public:
    TCPserver(int port_num);

   public:
    int get_lfd();
    int get_cfd();
    int accept_client();
    int get_port();
};