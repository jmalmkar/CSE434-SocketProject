// common.h
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "protocol.h"

/* Abort on fatal socket/IO errors. */
static inline void DieWithError(const char *msg) {
    perror(msg);
    exit(1);
}

/* Compose a header with version/opcode/req_id (req_id in host order). */
static inline void make_hdr(msg_hdr_t *h, uint8_t opcode, uint32_t req_id_host) {
    h->version  = PROTO_VERSION;
    h->opcode   = opcode;
    h->reserved = 0;
    h->req_id   = htonl(req_id_host);
}

/* Warning: check chosen UDP port against DSS_GROUP’s block. */
static inline void maybe_warn_port_range(unsigned short port, const char *who) {
    const char *gstr = getenv("DSS_GROUP");
    int G = gstr ? atoi(gstr) : 1;
    int lo = 1500 + (G - 1) * 100;
    int hi = lo + 99;
    if (!(port >= lo && port <= hi)) {
        fprintf(stderr, "%s: WARNING: port %u not in group %d range [%d,%d]\n",
                who, port, G, lo, hi);
    }
}

#endif /* COMMON_H */


