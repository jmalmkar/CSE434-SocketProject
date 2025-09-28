// protocol.c — helpers to keep htons/ntohl and header setup in one place
#include <string.h>
#include <arpa/inet.h>
#include "protocol.h"

/* Header */
void proto_make_hdr(msg_hdr_t *h, uint8_t opcode, uint32_t req_id_host)
{
    h->version  = PROTO_VERSION;
    h->opcode   = opcode;
    h->reserved = 0;
    h->req_id   = htonl(req_id_host);
}

/* REGISTER_USER */
void proto_user_req_hton(reg_user_req_t *p)
{
    p->listen_port = htons(p->listen_port);
}
void proto_user_req_ntoh(reg_user_req_t *p)
{
    p->listen_port = ntohs(p->listen_port);
}

/* REGISTER_DISK */
void proto_disk_req_hton(reg_disk_req_t *p)
{
    p->capacity_blocks = htonl(p->capacity_blocks);
    p->listen_port     = htons(p->listen_port);
}
void proto_disk_req_ntoh(reg_disk_req_t *p)
{
    p->capacity_blocks = ntohl(p->capacity_blocks);
    p->listen_port     = ntohs(p->listen_port);
}

/* CONFIGURE_DSS */
void proto_cfg_req_hton(cfg_dss_req_t *p)
{
    p->n          = htonl(p->n);
    p->block_size = htonl(p->block_size);
}
void proto_cfg_req_ntoh(cfg_dss_req_t *p)
{
    p->n          = ntohl(p->n);
    p->block_size = ntohl(p->block_size);
}

/* DEREGISTER_* payloads are plain text; no byte-order work needed */
/* RESP is mostly text; status is a byte */
