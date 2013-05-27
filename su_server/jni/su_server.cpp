#include <iostream>
#include <sstream>
#include <vector>

#include <cstring>
#include <string>

#include <stdlib.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <arpa/inet.h>

#include <sys/wait.h>

typedef unsigned char uchar;
typedef uint32_t data_size_t;

static bool send_data(int client_sock, const std::vector<uchar> &buff)
{
    size_t remain = buff.size();
    size_t i = 0;
    data_size_t size_to_send = htonl(remain);
    int sent = write(client_sock, reinterpret_cast<void*>(&size_to_send), sizeof(size_to_send));
    if (sent == 0) {
        std::cerr << "client closed" << std::endl;
        return false;
    } else if (sent == -1) {
        std::cerr << "could not write to socket" << std::endl;
        return false;
    }
    while (remain > 0) {
        sent = write(client_sock, &buff[i], remain);
        std::cout << "send " << sent << " bytes" << std::endl;
        if (sent == 0) {
            std::cerr << "client closed" << std::endl;
            return false;
        } else if (sent == -1) {
            std::cerr << "could not write to socket" << std::endl;
            return false;
        }
        remain -= sent;
        i += sent;
    }
    return true;
}

static std::string receive_command(int sock)
{
    uint32_t raw_len;
    int ret = recv(sock, &raw_len, sizeof(raw_len), 0);
    if (ret < 0) {
        std::cerr << "Could not receive command length" << std::endl;
        return NULL;
    }
    uint32_t len = ntohl(raw_len);

    std::vector<char> buff;
    buff.resize(len);
    int remaining = len, received, i = 0;
    while (remaining > 0) {
        received = recv(sock, &buff[i], remaining, 0);
        if (received < 0) {
            std::cerr << "Failed receiving data" << std::endl;
            return NULL;
        } else if (received == 0) {
            std::cerr << "Connection closed" << std::endl;
            return NULL;
        }
        remaining -= received;
        i += received;
    }
    buff.push_back('\0');

    std::stringstream sstm;
    sstm << &buff[0];
    return sstm.str();
}


int main(int argc, char *argv[])
{
    int server_sock;
    const std::string abstract_name("su_server");
    server_sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (server_sock == -1)
        perror("cannot open socket");

    struct sockaddr_un addr;
    static const size_t kPathMax = sizeof(addr.sun_path);
    if (1 /* first null byte */ + abstract_name.size() + 1 /* '\0' */ > kPathMax)
        perror("abstract namespace length is too long");

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    socklen_t addr_len;
    memcpy(addr.sun_path + 1, abstract_name.c_str(), abstract_name.size());
    addr_len = abstract_name.size() + offsetof(struct sockaddr_un, sun_path) + 1;

    if (bind(server_sock, reinterpret_cast<sockaddr*>(&addr), addr_len)) {
        std::cerr << "Could not bind unix domain socket to" << abstract_name;
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 10) == -1) {
        close(server_sock);
        perror("Could not listen");
    }

    struct sockaddr_storage client_addr;
    socklen_t address_size = sizeof(client_addr);

    int connect_d;
    pid_t cpid;
    while (true) {
        connect_d = accept(server_sock, reinterpret_cast<sockaddr*>(&client_addr), &address_size);
        if (connect_d == -1)
            perror("Failed to connect to client");

        cpid = fork();
        if (cpid < 0) {
            std::cerr << "Fork failed" << std::endl;
            exit(1);
        }
        if (cpid) {
            // parent
            close(connect_d);
            waitpid(cpid, NULL, 0);
        } else {
            std::string cmd = receive_command(connect_d);
            dup2(connect_d, STDOUT_FILENO);
            dup2(connect_d, STDERR_FILENO);
            dup2(connect_d, STDIN_FILENO);
            close(connect_d);

            if (cmd == "/system/bin/sh") {
                char *command = "/system/bin/sh";
                char *cmd_args[2];
                cmd_args[0] = command;
                cmd_args[1] = NULL;
                execvp(command, cmd_args);
            } else { // /system/bin/sh -c 'some command'
                char *command = "/system/bin/sh";
                char *cmd_args[4];
                char *cmd_cstr = (char *)cmd.c_str();
                cmd_args[0] = command;
                cmd_args[1] = "-c";
                cmd_args[2] = &cmd_cstr[18];
                cmd_args[3] = NULL;
                execvp(command, cmd_args);
            }
        }
    }

    close(server_sock);

    return 0;
}

