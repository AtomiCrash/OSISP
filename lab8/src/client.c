#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 4096

char *read_line(int fd) {
    static char buf[BUF_SIZE];
    static int pos = 0, len = 0;
    char *out = malloc(BUF_SIZE);
    int oi = 0;
    while (1) {
        if (pos >= len) {
            len = recv(fd, buf, BUF_SIZE, 0);
            pos = 0;
            if (len <= 0) { free(out); return NULL; }
        }
        char c = buf[pos++];
        if (c == '\n') break;
        out[oi++] = c;
    }
    out[oi] = 0;
    return out;
}

int is_prompt(const char *s) {
    size_t L = strlen(s);
    return L > 0 && s[L-1] == '>';
}

void handle_server(int sock, char **prompt) {
    char *line;
    while ((line = read_line(sock))) {
        if (is_prompt(line)) {
            free(*prompt);
            *prompt = strdup(line);
            free(line);
            break;
        }
        printf("%s\n", line);
        if (strcmp(line, "BYE") == 0) {
            free(line);
            exit(0);
        }
        free(line);
    }
}

void send_cmd(int sock, const char *cmd) {
    send(sock, cmd, strlen(cmd), 0);
    send(sock, "\n", 1, 0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s server_host port\n", argv[0]);
        return 1;
    }
    const char *host = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in servaddr;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { 
        perror("socket"); return 1; 
    }

    struct hostent *he = gethostbyname(host);
    if (!he) { 
        fprintf(stderr, "Unknown host\n"); return 1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    memcpy(&servaddr.sin_addr, he->h_addr_list[0], he->h_length);
    servaddr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&servaddr, sizeof(servaddr))<0) {
        perror("connect"); return 1;
    }
    char *prompt = NULL;
    handle_server(sock, &prompt);

    char line[BUF_SIZE];
    while (1) {
        printf("%s ", prompt);
        if (!fgets(line, sizeof(line), stdin)) break;
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        if (line[0] == '@') {
            const char *fn = line + 1;
            FILE *f = fopen(fn, "r");
            if (!f) { 
                perror("fopen"); 
                continue;
            }
            while (fgets(line, sizeof(line), f)) {
                if ((nl = strchr(line, '\n'))) *nl = 0;
                if (strlen(line)==0) continue;
                send_cmd(sock, line);
                handle_server(sock, &prompt);
            }
            fclose(f);
        } else {
            if (strlen(line)==0) continue;
            send_cmd(sock, line);
            handle_server(sock, &prompt);
        }
    }
    free(prompt);
    close(sock);
    return 0;
}