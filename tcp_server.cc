#include "tcp_server.h"

TCPserver::TCPserver(int port) {
    lfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(lfd, (sockaddr*)&addr, sizeof(addr));
    listen(lfd, 1024);
}

int TCPserver ::get_lfd() {
    return lfd;
}

int TCPserver ::get_cfd() {
    return cfd;
}

int TCPserver::accept_client() {
    cfd = accept(lfd, nullptr, nullptr);
    return cfd;
}

int TCPserver::get_port() {
    sockaddr_in sin{};
    socklen_t len = sizeof(sin);
    getsockname(lfd, (sockaddr*)&sin, &len);
    return ntohs(sin.sin_port);
}