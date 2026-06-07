#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "command.h"
#include "tcp_client.h"

using namespace std;

string current_dir;              // 当前目录
TCPclient* ctrl_conn = nullptr;  // 数据连接
CMD* cmd = nullptr;              // 命令
string ip;                       // 服务器ip地址
struct response {                // 响应结构体
    int stus_code = 0;
    char text[1024];
    int data_port = 0;
    size_t file_size = 0;
};

int exec_cmd(int cmd_code);                 // 执行命令
int exec_cmd(int cmd_code, int);            // 执行命令
void print_response(const response& resp);  // 打印响应
string get_current_dir();                   // 获取当前目录

int main(int argc, char* argv[]) {
    current_dir = get_current_dir();  // 初始化当前目录
    if (argc == 2) {
        ctrl_conn = new TCPclient(argv[1], 2100);
        ip = argv[1];
    } else {
        return 1;
    }
    while (1) {
        cout << "ftp> ";
        string cmd_line;
        if (!getline(cin, cmd_line)) {
            cout << endl;
            break;
        }
        if (cmd_line.empty()) {
            continue;
        }
        cmd = new CMD(cmd_line);
        int cmd_code = cmd->get_cmd_code();
        if (cmd_code == 0) {
            if (!cmd->get_cmd_args().empty()) {
                cout << "?Invalid command." << endl;
            }
            delete cmd;
            continue;
        } else if (cmd_code < 0) {
            exec_cmd(cmd_code);
        } else {
            if (exec_cmd(cmd_code, 1) == 221) {
                delete cmd;
                break;
            }
        }
        delete cmd;
    }
    close(ctrl_conn->get_fd());
    delete ctrl_conn;
    return 0;
}

int exec_cmd(int cmd_code) {
    if (cmd_code == -3) {
        string target_dir = cmd->get_cmd_args()[1];
        if (chdir(target_dir.c_str()) != 0) {
            cout << "ftp: Can't chdir `" << target_dir
                 << "': 没有那个文件或目录" << endl;
            return 1;
        }
        current_dir = get_current_dir();
    } else if (cmd_code == -2) {
        system("pwd");
    } else if (cmd_code == -1) {
        system(cmd->get_cmd_line().substr(1).c_str());
    }
    return 0;
}

int exec_cmd(int cmd_code, int) {
    if (cmd_code == 5) {
        cout << "local: " << cmd->get_cmd_args()[2]
             << " remote: " << cmd->get_cmd_args()[1] << endl;
    } else if (cmd_code == 6) {
        cout << "local: " << cmd->get_cmd_args()[1]
             << " remote: " << cmd->get_cmd_args()[2] << endl;
        if (access(cmd->get_cmd_args()[1].c_str(), F_OK) != 0) {
            cout << "ftp : Can 't open `" << cmd->get_cmd_args()[1]
                 << "' : 没有那个文件或目录 " << endl;
            return 1;
        }
    }

    // 仅控制连接
    int ctrl_fd = ctrl_conn->get_fd();
    send(ctrl_fd, cmd->get_cmd_line().c_str(), cmd->get_cmd_line().size(), 0);
    response response;
    recv(ctrl_fd, (char*)&response, sizeof(response), 0);
    print_response(response);
    if (cmd_code < 4) {
        return response.stus_code;
    }

    // 控制连接+数据连接
    TCPclient data_client(ip, response.data_port);
    size_t file_size = 0;
    time_t t = 0;
    while (true) {
        recv(ctrl_fd, (char*)&response, sizeof(response), 0);
        print_response(response);
        if (response.stus_code == 550) {
            return response.stus_code;
        } else if (response.stus_code == 226) {
            if (cmd_code > 4) {
                int min = t / 60;
                int sec = t % 60;
                if (cmd_code == 5) {
                    cout << file_size << " bytes received in " << setfill('0')
                         << setw(2) << min << ":" << setw(2) << sec << endl;
                } else {
                    cout << file_size << " bytes send in " << setfill('0')
                         << setw(2) << min << ":" << setw(2) << sec << endl;
                }
            }
            return response.stus_code;
        } else if (response.stus_code == 150) {
            if (cmd_code == 4) {
                char buf[4096];
                while (true) {
                    int len = recv(data_client.get_fd(), buf, sizeof(buf), 0);
                    if (len <= 0) {
                        break;
                    }
                    cout.write(buf, len);
                }
            } else if (cmd_code == 5) {
                int out_fd = open(cmd->get_cmd_args()[2].c_str(),
                                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
                char buf[4096];
                ssize_t n;
                time_t start_time = time(nullptr);
                while ((n = read(data_client.get_fd(), buf, sizeof(buf))) > 0) {
                    file_size += n;
                    write(out_fd, buf, n);
                }
                time_t end_time = time(nullptr);
                t = end_time - start_time;
                close(out_fd);
            } else if (cmd_code == 6) {
                int out_fd = open(cmd->get_cmd_args()[1].c_str(), O_RDONLY);
                file_size = lseek(out_fd, 0, SEEK_END);
                lseek(out_fd, 0, SEEK_SET);
                off_t offset = 0;
                time_t start_time = time(nullptr);
                while (offset < file_size) {
                    ssize_t sent = sendfile(data_client.get_fd(), out_fd,
                                            &offset, file_size - offset);
                    if (sent <= 0) {
                        break;
                    }
                }
                time_t end_time = time(nullptr);
                t = end_time - start_time;
                close(out_fd);
            }
            close(data_client.get_fd());
        }
    }
}

void print_response(const response& resp) {
    if (resp.stus_code != 0) {
        cout << resp.stus_code << " ";
    }
    string text = resp.text;
    cout << text;
    if (resp.stus_code == 229 && resp.data_port != 0) {
        int p1 = resp.data_port / 256;
        int p2 = resp.data_port % 256;
        cout << "(" << ip << "." << p1 << "." << p2 << ")";
    }
    if (resp.stus_code == 150 && resp.file_size != 0) {
        cout << " (" << resp.file_size << " bytes).";
    }
    cout << endl;
}

string get_current_dir() {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) != nullptr) {
        return string(buf);
    }
    return "";
}