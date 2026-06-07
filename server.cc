#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include "command.h"
#include "tcp_server.h"

using namespace std;

string prev_dir;                 // 上次目录
string current_dir;              // 当前目录
TCPserver* ctrl_conn = nullptr;  // 控制连接
struct response {                // 响应结构体
    int stus_code = 0;
    char text[1024];
    int data_port = 0;
    size_t file_size = 0;
};

void set_nonblock(int fd);
string get_current_dir();  // 获取当前目录

int main() {
    current_dir = get_current_dir();  // 初始化当前目录
    ctrl_conn = new TCPserver(2100);
    int ctrl_lfd = ctrl_conn->get_lfd();
    set_nonblock(ctrl_lfd);
    int epfd = epoll_create(1);
    if (epfd == -1) {
        perror("epoll_create failed");
        exit(1);
    }
    epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = ctrl_lfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, ctrl_lfd, &event);
    epoll_event events[1024];
    int events_size = sizeof(events) / sizeof(events[0]);
    while (true) {
        int num = epoll_wait(epfd, events, events_size, -1);
        for (int i = 0; i < num; i++) {
            int fd = events[i].data.fd;
            if (fd == ctrl_lfd) {
                int ctrl_cfd = ctrl_conn->accept_client();
                set_nonblock(ctrl_cfd);
                event.events = EPOLLIN;
                event.data.fd = ctrl_cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, ctrl_cfd, &event);
            } else {
                char cmd_line[1024];
                memset(cmd_line, 0, sizeof(cmd_line));
                int len = recv(fd, cmd_line, sizeof(cmd_line), 0);
                if (len <= 0) {
                    if (len < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        continue; 
                    }
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    continue;
                }
                cmd_line[len] = '\0';
                CMD cmd(cmd_line);
                int cmd_code = cmd.get_cmd_code();
                response response;
                if (cmd_code == 1) {
                    // 221 Goodbye.
                    response.stus_code = 221;
                    strcpy(response.text, "Goodbye.");
                } else if (cmd_code == 2) {
                    string target_dir = cmd.get_cmd_args()[1];
                    if (chdir(target_dir.c_str()) != 0) {
                        // 550 Failed to change directory.
                        response.stus_code = 550;
                        strcpy(response.text, "Failed to change directory.");
                    } else {
                        // 250 Directory successfully changed.
                        response.stus_code = 250;
                        strcpy(response.text,
                               "Directory successfully changed.");
                    }
                    prev_dir = current_dir;
                    current_dir = get_current_dir();
                } else if (cmd_code == 3) {
                    // Remote directory: /
                    response.stus_code = 0;
                    strcpy(response.text,
                           ("Remote directory: " + get_current_dir()).c_str());
                } else {
                    TCPserver data_conn(0);
                    // 229 Entering Extended Passive Mode (127.0.0.1.p1.p2)
                    response.stus_code = 229;
                    response.data_port = data_conn.get_port();
                    strcpy(response.text, "Entering Extended Passive Mode ");
                    send(fd, (char*)&response, sizeof(response), 0);
                    int data_cfd = data_conn.accept_client();
                    if (cmd_code == 4) {
                        // 150 Here comes the directory listing.
                        FILE* fp = popen("ls -l", "r");
                        response.stus_code = 150;
                        strcpy(response.text,
                               "Here comes the directory listing.");
                        response.data_port = 0;
                        response.file_size = 0;
                        send(fd, (char*)&response, sizeof(response), 0);
                        char buf[4096];
                        while (fgets(buf, sizeof(buf), fp) != nullptr) {
                            send(data_cfd, buf, strlen(buf), 0);
                        }
                        // 226 Directory send OK.
                        response.stus_code = 226;
                        strcpy(response.text, "Directory send OK.");
                        pclose(fp);
                    } else if (cmd_code == 5) {
                        // 150 Opening BINARY mode data connection for file_name
                        // (file_size bytes).
                        int out_fd =
                            open(cmd.get_cmd_args()[1].c_str(), O_RDONLY);
                        if (out_fd < 0) {
                            response.stus_code = 550;
                            strcpy(response.text,
                                   "Failed to open file for writing.");
                            send(fd, (char*)&response, sizeof(response), 0);
                            continue;
                        }
                        response.stus_code = 150;
                        strcpy(response.text,
                               "Opening BINARY mode data connection for");
                        response.file_size = lseek(out_fd, 0, SEEK_END);
                        lseek(out_fd, 0, SEEK_SET);
                        send(fd, (char*)&response, sizeof(response), 0);
                        off_t offset = 0;
                        while (offset < response.file_size) {
                            ssize_t sent =
                                sendfile(data_cfd, out_fd, &offset,
                                         response.file_size - offset);
                            if (sent <= 0) {
                                break;
                            }
                        }
                        // 226 Transfer complete.
                        response.stus_code = 226;
                        strcpy(response.text, "Transfer complete.");
                        close(out_fd);
                    } else if (cmd_code == 6) {
                        // 150 Ok to send data.
                        int out_fd = open(cmd.get_cmd_args()[2].c_str(),
                                          O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        response.stus_code = 150;
                        strcpy(response.text, "Ok to send data.");
                        send(fd, (char*)&response, sizeof(response), 0);
                        char buf[4096];
                        ssize_t n;
                        while ((n = read(data_cfd, buf, sizeof(buf))) > 0) {
                            write(out_fd, buf, n);
                        }
                        // 226 Transfer complete.
                        response.stus_code = 226;
                        strcpy(response.text, "Transfer complete.");
                        close(out_fd);
                    }
                    close(data_cfd);
                    close(data_conn.get_lfd());
                }
                send(fd, (char*)&response, sizeof(response), 0);
            }
        }
    }
    close(ctrl_lfd);
    return 0;
}

string get_current_dir() {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) != nullptr) {
        return string(buf);
    }
    return "";
}

void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
// cout << "2" << endl; ///////////////////////////