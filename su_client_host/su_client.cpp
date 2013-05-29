#include <iostream>
#include <sstream>
#include <vector>
#include <string>

#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <netinet/in.h>

#include <sys/wait.h>
#include <time.h>

#include <errno.h>
#include <pthread.h>

typedef unsigned char uchar;
typedef uint32_t data_size_t;

int connect_to_server(std::string server_name, std::string port)
{
    int sock;
    struct sockaddr_in serv;
    unsigned short port_num;
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "could not get socket" << std::endl;
        return -1;
    }
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = PF_INET;
    port_num = (unsigned short) strtol(port.c_str(), NULL, 10);
    serv.sin_port = htons(port_num);
    inet_aton(server_name.c_str(), &(serv.sin_addr));

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&serv), sizeof(struct sockaddr_in)) < 0) {
        std::cerr << "could not connect to " << server_name << ":" << port << std::endl;
        return -1;
    }
    return sock;
}

bool receive_data(int sock, std::vector<uchar> &buff)
{
    data_size_t data_size_raw, data_size;
    int received = recv(sock, &data_size_raw, sizeof(data_size_raw), 0);
    if (received < 0)
        return false;
    data_size = ntohl(data_size_raw);

    std::cout << "data_size = " << data_size << std::endl;

    buff.resize(data_size);

    size_t remain = data_size;
    size_t i = 0;

    while (remain > 0) {
        received = recv(sock, &buff[i], remain, 0);
        if (received < 0) {
            std::cerr << "faied receiving data" << std::endl;
            return false;
        } else if (received == 0) {
            std::cerr << "connection closed" << std::endl;
            return false;
        }

        remain -= received;
        i += received;
    }

    return true;
}

bool send_arg_cnt(int sock, uint32_t cnt)
{
    uint32_t n_cnt = htonl(cnt);
    int sent = write(sock, &n_cnt, sizeof(n_cnt));
    if (sent == 0) {
        std::cerr << "connection closed" << std::endl;
        return false;
    } else if (sent == -1) {
        std::cerr << "could not write to socket" << std::endl;
        return false;
    }
    return true;
}

bool send_argument(int sock, const std::string cmd)
{
    std::vector<char> data;
    std::copy(cmd.begin(), cmd.end(), std::back_inserter(data));
    size_t remaining = data.size();
    size_t i = 0;
    uint32_t size_to_send = htonl(remaining);
    int sent = write(sock, &size_to_send, sizeof(size_to_send));
    if (sent == 0) {
        std::cerr << "connection closed" << std::endl;
        return false;
    } else if (sent == -1) {
        std::cerr << "could not write to socket" << std::endl;
        return false;
    }
    while (remaining > 0) {
        sent = write(sock, &data[i], remaining);
        if (sent == 0) {
            std::cerr << "connection closed" << std::endl;
            return false;
        } else if (sent == -1) {
            std::cerr << "could not write to socket" << std::endl;
            return false;
        }
        remaining -= sent;
        i += sent;
    }
    return true;
}

static void* remote_stdout_read_thread(void *x)
{
    int fd = -1;
    char buf[1024];
    int ret;
    int sock = *((int *)x);
    fd = dup(STDOUT_FILENO);
    if (fd == -1) {
        return NULL;
    }
    while (true) {
        ret = recv(sock, buf, sizeof(buf), 0);
        if (ret == 0)
            break;
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        write(fd, buf, ret);
    }
    close(fd);
    return NULL;
}

static void* remote_stdin_write_thread(void *x)
{
    int fd = -1;
    char buf[1024];
    int ret;
    int sock = *((int *)x);
    fd = dup(STDIN_FILENO);
    if (fd == -1) {
        return NULL;
    }
    while (true) {
        //ret = recv(sock, buf, sizeof(buf), 0);
        ret = read(fd, buf, sizeof(buf));
        if (ret == 0)
            break;
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        write(sock, buf, ret);
    }
    close(fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    int sock = connect_to_server("localhost", "1234");
    if (sock < 0) {
        std::cerr << "could not connect to localhost:1234" << std::endl;
        exit(1);
    }

    send_arg_cnt(sock, argc - 1);
    for (int i = 1; i < argc; ++i)
        send_argument(sock, argv[i]);

    pthread_t t_read, t_write;
    if (pthread_create(&t_read, NULL, remote_stdout_read_thread, &sock) == -1) {
        std::cerr << "Could not create thread" << std::endl;
        close(sock);
        exit(1);
    }
    if (pthread_create(&t_write, NULL, remote_stdin_write_thread, &sock) == -1) {
        std::cerr << "Could not create thread" << std::endl;
        close(sock);
        exit(1);
    }

    void* result;
    if (pthread_join(t_read, &result) == -1) {
        std::cerr << "Join failed" << std::endl;
    }
    //if (pthread_join(t_write, &result) == -1) {
    //    std::cerr << "Join failed" << std::endl;
    //}

    close(sock);


    return 0;
}
