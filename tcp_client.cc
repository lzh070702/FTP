#include "tcp_client.h"

TCPclient::TCPclient(const string& ip, int port_num) {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        throw runtime_error("socket create failed");
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_num);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        close(fd);
        throw runtime_error("invalid ip address");
    }
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        close(fd);
        throw runtime_error("connect to ftp server failed");
    }
}

int TCPclient::get_fd() {
    return fd;
}

void TCPclient::set_fd(int new_fd) {
    fd = new_fd;
}
