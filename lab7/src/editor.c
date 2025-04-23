#define _GNU_SOURCE
#include "record.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

int fd = -1;
size_t record_count = 0;
record_t orig_rec, work_rec;
int last_rec_no = -1;
int has_work = 0;

int lock_rec(off_t offset, short type, int wait) {
    struct flock fl = { .l_type=type, .l_whence=SEEK_SET, .l_start=offset, .l_len=sizeof(record_t) };
    int cmd = wait ? F_OFD_SETLKW : F_OFD_SETLK;
    return fcntl(fd, cmd, &fl);
}

int unlock_rec(off_t offset) {
    struct flock fl = { .l_type=F_UNLCK, .l_whence=SEEK_SET, .l_start=offset, .l_len=sizeof(record_t) };
    return fcntl(fd, F_OFD_SETLK, &fl);
}

void do_list(void) {
    if (lseek(fd, 0, SEEK_SET) < 0) {
        fprintf(stderr, "Seek error: %s\n", strerror(errno));
        return;
    }
    printf("\n#   Name                           Sem  Address\n");
    record_t r;
    for (size_t i = 0; i < record_count; i++) {
        if (read(fd, &r, sizeof(r)) != sizeof(r)) break;
        printf("%2zu  %-30.30s %3u  %-30.30s\n", i, r.name, r.semester, r.address);
    }
    printf("\n");
}

void do_get(void) {
    printf("Enter record number to edit (0..%zu): ", record_count - 1);
    int rn;
    if (scanf("%d", &rn) != 1 || rn < 0 || (size_t)rn >= record_count) {
        printf("Invalid input.\n");
        while (getchar() != '\n');
        return;
    }
    off_t off = (off_t)rn * sizeof(record_t);
    if (lseek(fd, off, SEEK_SET) < 0 || read(fd, &orig_rec, sizeof(orig_rec)) != sizeof(orig_rec)) {
        fprintf(stderr, "Read error: %s\n", strerror(errno));
        return;
    }
    work_rec = orig_rec;
    last_rec_no = rn;
    has_work = 1;

    printf("\nCurrent record #%d:\n  Name: %s\n  Address: %s\n  Semester: %u\n\n",
           last_rec_no, orig_rec.name, orig_rec.address, orig_rec.semester);
    while (getchar() != '\n');

    char buf[ADDR_LEN];
    while (1) {
        printf("New name (1-%d chars, empty to keep): ", NAME_LEN - 1);
        if (!fgets(buf, sizeof(buf), stdin)) return;
        if (buf[0] == '\n') break;
        buf[strcspn(buf, "\n")] = '\0';
        size_t len = strlen(buf);
        if (len > 0 && len < NAME_LEN) {
            strncpy(work_rec.name, buf, NAME_LEN);
            break;
        }
        printf("Invalid name length.\n");
    }
    while (1) {
        printf("New address (1-%d chars, empty to keep): ", ADDR_LEN - 1);
        if (!fgets(buf, sizeof(buf), stdin)) return;
        if (buf[0] == '\n') break;
        buf[strcspn(buf, "\n")] = '\0';
        size_t len = strlen(buf);
        if (len > 0 && len < ADDR_LEN) {
            strncpy(work_rec.address, buf, ADDR_LEN);
            break;
        }
        printf("Invalid address length.\n");
    }
    while (1) {
        printf("New semester (1-12, empty to keep): ");
        if (!fgets(buf, sizeof(buf), stdin)) return;
        if (buf[0] == '\n') break;
        char *endptr;
        long s = strtol(buf, &endptr, 10);
        if (*endptr == '\n' || *endptr == '\0') {
            if (s >= 1 && s <= 12) {
                work_rec.semester = (uint8_t)s;
                break;
            }
        }
        printf("Invalid semester. Must be a number 1-12.\n");
    }
    printf("\n");
}

void do_put(void) {
    if (!has_work) {
        printf("No pending edits. Use 'G' to fetch a record first.\n");
        return;
    }
    if (memcmp(&orig_rec, &work_rec, sizeof(orig_rec)) == 0) {
        printf("No changes to save.\n");
        has_work = 0;
        return;
    }
    off_t off = (off_t)last_rec_no * sizeof(record_t);
    int retry = 1;
    while (retry) {
        retry = 0;
        struct flock fl = { .l_type=F_WRLCK, .l_whence=SEEK_SET, .l_start=off, .l_len=sizeof(record_t) };
        if (fcntl(fd, F_GETLK, &fl) < 0) {
            fprintf(stderr, "Lock query error: %s\n", strerror(errno));
            return;
        }
        if (fl.l_type != F_UNLCK) {
            printf("Record %d is locked by PID %d, waiting...\n", last_rec_no, fl.l_pid);
        }
        if (lock_rec(off, F_WRLCK, 1) < 0) {
            fprintf(stderr, "Lock error: %s\n", strerror(errno));
            return;
        }
        record_t current;
        if (lseek(fd, off, SEEK_SET) < 0 || read(fd, &current, sizeof(current)) != sizeof(current)) {
            fprintf(stderr, "Re-read error: %s\n", strerror(errno));
            unlock_rec(off);
            return;
        }
        if (memcmp(&current, &orig_rec, sizeof(orig_rec)) != 0) {
            printf("Conflict detected: another process updated record.\n");
            orig_rec = current;
            work_rec = current;
            unlock_rec(off);
            retry = 1;
            continue;
        }
        if (lseek(fd, off, SEEK_SET) < 0 || write(fd, &work_rec, sizeof(work_rec)) != sizeof(work_rec)) {
            fprintf(stderr, "Write error: %s\n", strerror(errno));
            unlock_rec(off);
            return;
        }
        unlock_rec(off);
        printf("Record %d updated successfully.\n", last_rec_no);
        has_work = 0;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <datafile>\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Cannot open '%s': %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }
    off_t sz = lseek(fd, 0, SEEK_END);
    if (sz < 0 || sz % sizeof(record_t) != 0) {
        fprintf(stderr, "Invalid data file size.\n");
        close(fd);
        return EXIT_FAILURE;
    }
    record_count = sz / sizeof(record_t);
    if (record_count < 10) {
        printf("Warning: file has only %zu records (<10)\n", record_count);
    }

    printf("\n*** Concurrent File Editor (Lab 7) ***\n");
    printf("Records loaded: %zu\n", record_count);
    while (1) {
        printf("\nCommands: [L]ist, [G]et, [P]ut, [E]xit\n");
        printf("Select: ");
        char cmd;
        if (scanf(" %c", &cmd) != 1) break;
        while (getchar() != '\n');
        switch (cmd) {
            case 'L': case 'l': do_list(); break;
            case 'G': case 'g': do_get();  break;
            case 'P': case 'p': do_put();  break;
            case 'E': case 'e':
                printf("Exiting.\n");
                close(fd);
                return EXIT_SUCCESS;
            default:
                printf("Unknown command.\n");
        }
    }
    close(fd);
    return EXIT_SUCCESS;
}
