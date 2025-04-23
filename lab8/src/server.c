
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>

#define BACKLOG 10
#define BUF_SIZE 4096

#ifndef NAME_MAX
#define NAME_MAX 255
#endif
#define PATHBUF (PATH_MAX + NAME_MAX + 2)

typedef struct {
    int client_fd;
    char root[PATH_MAX];
} client_args_t;

void log_event(const char *fmt, ...) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y.%m.%d-%H:%M:%S", &tm);
    int ms = tv.tv_usec / 1000;
    fprintf(stdout, "%s.%03d ", ts, ms);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fprintf(stdout, "\n");
    fflush(stdout);
}

static char *trim(char *s) {
    char *end;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

int build_path(const char *root, const char *cwd, const char *target, char *out) {
    char tmp[PATHBUF];
    if (target[0] == '/') {
        snprintf(tmp, sizeof(tmp), "%s%s", root, target);
    } else {
        snprintf(tmp, sizeof(tmp), "%s/%s/%s", root, cwd, target);
    }
    char realp[PATH_MAX];
    if (!realpath(tmp, realp)) return -1;
    if (strncmp(realp, root, strlen(root)) != 0) return -1;
    strcpy(out, realp);
    return 0;
}
void sendall(int fd, const char *s) {
    size_t len = strlen(s), sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, s + sent, len - sent, 0);
        if (n <= 0) return;
        sent += n;
    }
}

void handle_echo(int fd, const char *arg) {
    sendall(fd, arg);
    sendall(fd, "\n");
}

void handle_info(int fd) {
    sendall(fd, "Hello from 'myserver'\n");
}

void handle_list(int fd, const char *root, const char *cwd) {
    char path[PATHBUF];
    if (build_path(root, cwd, ".", path) < 0) return;
    DIR *d = opendir(path);
    if (!d) { perror("opendir"); return; }
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;
        char full[PATHBUF];
        size_t plen = strlen(path);
        size_t nlen = strlen(ent->d_name);
        if (plen + 1 + nlen >= sizeof(full))
            continue;
        memcpy(full, path, plen);
        full[plen] = '/';
        memcpy(full + plen + 1, ent->d_name, nlen);
        full[plen + 1 + nlen] = '\0';
        struct stat st;
        if (lstat(full, &st) < 0) continue;
        if (S_ISDIR(st.st_mode)) {
            sendall(fd, ent->d_name);
            sendall(fd, "/\n");
        } else if (S_ISLNK(st.st_mode)) {
            char linkto[PATHBUF];
            ssize_t r = readlink(full, linkto, sizeof(linkto)-1);
            if (r < 0) continue;
            linkto[r] = '\0';
            char target[PATHBUF];
            if (build_path(root, cwd, linkto, target) == 0) {
                struct stat st2;
                if (lstat(target, &st2) == 0 && S_ISLNK(st2.st_mode)) {
                    char real2[PATHBUF];
                    ssize_t r2 = readlink(target, real2, sizeof(real2)-1);
                    if (r2 > 0) {
                        real2[r2] = '\0';
                        sendall(fd, ent->d_name);
                        sendall(fd, " -->> ");
                        sendall(fd, real2);
                        sendall(fd, "\n");
                    }
                } else {
                    sendall(fd, ent->d_name);
                    sendall(fd, " --> ");
                    sendall(fd, linkto);
                    sendall(fd, "\n");
                }
            }
        } else {
            sendall(fd, ent->d_name);
            sendall(fd, "\n");
        }
    }
    closedir(d);
}

void *client_thread(void *arg) {
    client_args_t *ca = arg;
    int fd = ca->client_fd;
    char cwd[PATH_MAX] = "";
    handle_info(fd);
    sendall(fd, ">\n");
    log_event("Client %d connected, greeting sent", fd);

    char buf[BUF_SIZE];
    while (1) {
        ssize_t n = recv(fd, buf, sizeof(buf)-1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        char *nl = strchr(buf, '\n'); if (nl) *nl = '\0';
        char *cmd = trim(buf);
        if (*cmd == '\0') continue;
        log_event("Client %d sent: %s", fd, cmd);

        if (strncasecmp(cmd, "ECHO ", 5) == 0) {
            handle_echo(fd, cmd + 5);
        } else if (strcasecmp(cmd, "QUIT") == 0) {
            sendall(fd, "BYE\n");
            log_event("Client %d disconnected", fd);
            break;
        } else if (strcasecmp(cmd, "INFO") == 0) {
            handle_info(fd);
        } else if (strncasecmp(cmd, "CD ", 3) == 0) {
            const char *t = cmd + 3;
            if (strcasecmp(t, "/") == 0) {
                cwd[0] = '\0';
            } else if (strcasecmp(t, "..") == 0) {
                char *p = strrchr(cwd, '/'); if (p) *p = '\0'; else cwd[0] = '\0';
            } else {
                char newp[PATHBUF];
                if (build_path(ca->root, cwd, t, newp) == 0) {
                    const char *rel = newp + strlen(ca->root);
                    if (rel[0] == '/') rel++;
                    size_t rlen = strlen(rel);
                    if (rlen < sizeof(cwd)) memcpy(cwd, rel, rlen+1);
                }
            }
        } else if (strcasecmp(cmd, "LIST") == 0) {
            handle_list(fd, ca->root, cwd);
        } else {
            sendall(fd, "Unknown command\n");
        }
        if (cwd[0] == '\0') sendall(fd, ">\n");
         else { 
        sendall(fd, cwd); sendall(fd, ">\n");
     }
    }
    close(fd);
    free(ca);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s root_dir port\n", argv[0]);
        return 1;
    }
    char root[PATH_MAX]; 
    if (!realpath(argv[1], root)) { 
        perror("realpath"); return 1;
     }
    int port = atoi(argv[2]);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { 
        perror("socket"); return 1; 
    }
    int opt = 1; 
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv)); 
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY; 
    serv.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&serv, sizeof(serv)) < 0) { 
        perror("bind"); return 1;
     }
    if (listen(listen_fd, BACKLOG) < 0) { 
        perror("listen"); return 1; 
    }
    log_event("Server started port=%d, root=%s", port, root);

    while (1) {
        struct sockaddr_in cli; 
        socklen_t len = sizeof(cli);
        int fd = accept(listen_fd, (struct sockaddr*)&cli, &len);
        if (fd < 0) { 
            perror("accept"); continue;
        }
        client_args_t *ca = malloc(sizeof(*ca));
        ca->client_fd = fd;
        strncpy(ca->root, root, PATH_MAX);
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, ca);
        pthread_detach(tid);
    }
    close(listen_fd);
    return 0;
}