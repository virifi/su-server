#include <iostream>
#include <sstream>
#include <vector>
#include <string>

#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/un.h>
#include <netinet/in.h>

#include <sys/wait.h>
#include <time.h>

#include <errno.h>
#include <pthread.h>

typedef unsigned char uchar;
typedef uint32_t data_size_t;

bool send_command(int sock, const std::string cmd)
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
    std::stringstream sstm;
    if (argc == 1) {
        sstm << "/system/bin/sh";
    } else if (argc == 3) {
        sstm << "/system/bin/sh -c " << argv[2];
    } else {
        printf("usage:\n");
        printf("%s\n", argv[0]);
        printf("%s -c 'some command'\n", argv[0]);
        exit(1);
    }
    std::string cmd = sstm.str();

    const std::string abstract_name("su_server");
    int sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sock == -1)
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

    if (connect(sock, reinterpret_cast<struct sockaddr *>(&addr), addr_len) < 0) {
        close(sock);
        perror("Could not connect to server");
        exit(1);
    }

    send_command(sock, cmd);

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
