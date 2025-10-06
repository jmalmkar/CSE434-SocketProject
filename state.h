// state.h

#ifndef STATE_H
#define STATE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include "protocol.h"

#define MAX_USERS 64
#define MAX_DISKS 64

/* Manager-side records for users and disks. */
typedef struct {
    int                 in_use;
    char                name[MAX_NAME_LEN];
    struct sockaddr_in  addr;         /* user’s IP:port */
} user_rec_t;

typedef struct {
    int                 in_use;
    int                 in_dss;      
    char                name[MAX_NAME_LEN];
    uint32_t            capacity_blocks;
    struct sockaddr_in  addr;         /* disk’s IP:port */
} disk_rec_t;

typedef struct {
    user_rec_t  users[MAX_USERS];
    disk_rec_t  disks[MAX_DISKS];

    uint32_t    dss_n;
    uint32_t    dss_block_size;
} manager_state_t;

/* helpers */
static inline int find_user(manager_state_t *st, const char *name) {
    for (int i = 0; i < MAX_USERS; i++)
        if (st->users[i].in_use && strncmp(st->users[i].name, name, MAX_NAME_LEN) == 0)
            return i;
    return -1;
}
static inline int find_disk(manager_state_t *st, const char *name) {
    for (int i = 0; i < MAX_DISKS; i++)
        if (st->disks[i].in_use && strncmp(st->disks[i].name, name, MAX_NAME_LEN) == 0)
            return i;
    return -1;
}
static inline int add_user(manager_state_t *st, const char *name, const struct sockaddr_in *addr) {
    if (find_user(st, name) >= 0) return -1;
    for (int i = 0; i < MAX_USERS; i++) if (!st->users[i].in_use) {
        st->users[i].in_use = 1;
        snprintf(st->users[i].name, MAX_NAME_LEN, "%s", name);
        st->users[i].name[MAX_NAME_LEN-1] = '\0';
        st->users[i].addr = *addr;
        return i;
    }
    return -2;
}
static inline int add_disk(manager_state_t *st, const char *name, uint32_t cap, const struct sockaddr_in *addr) {
    if (find_disk(st, name) >= 0) return -1;
    for (int i = 0; i < MAX_DISKS; i++) if (!st->disks[i].in_use) {
        st->disks[i].in_use = 1;
        st->disks[i].in_dss = 0;   /* free by default */
        snprintf(st->disks[i].name, MAX_NAME_LEN, "%s", name);
        st->disks[i].capacity_blocks = cap;
        st->disks[i].addr = *addr;
        return i;
    }
    return -2;
}
static inline int count_free_disks(manager_state_t *st) {
    int c = 0;
    for (int i = 0; i < MAX_DISKS; i++)
        if (st->disks[i].in_use && !st->disks[i].in_dss) c++;
    return c;
}

#endif /* STATE_H */

