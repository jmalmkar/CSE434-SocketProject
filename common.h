#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "protocol.h"

static inline void DieWithError(const char *msg) {
    perror(msg);
    exit(1);
}

static inline void maybe_warn_port_range(unsigned short port, const char *who) {
    const char *grp = getenv("DSS_GROUP");
    if (!grp) return;
    int g = atoi(grp);
    int lo = 15900, hi = 15999;
    if (port < lo || port > hi) {
        fprintf(stderr, "%s: WARNING: port %u not in group %d range [%d,%d]\n",
                who, port, g, lo, hi);
    }
}

static inline void make_hdr(msg_hdr_t *h, uint8_t opcode, uint32_t req_id_host) {
    proto_make_hdr(h, opcode, req_id_host);
}

static inline int is_power_of_two(uint32_t x){ return x && !(x & (x-1)); }

#endif /* COMMON_H */
