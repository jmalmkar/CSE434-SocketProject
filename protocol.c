#include <string.h>
#include <arpa/inet.h>
#include "protocol.h"

void proto_make_hdr(msg_hdr_t *h, uint8_t opcode, uint32_t req_id_host)
{
    h->version  = PROTO_VERSION;
    h->opcode   = opcode;
    h->reserved = 0;
    h->req_id   = htonl(req_id_host);
}

void proto_user_req_hton(reg_user_req_t *p)
{
    p->listen_port = htons(p->listen_port);
}
void proto_user_req_ntoh(reg_user_req_t *p)
{
    p->listen_port = ntohs(p->listen_port);
}

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

void proto_cfg2_req_hton(cfg_dss2_req_t *p)
{
    p->n            = htonl(p->n);
    p->striping_unit= htonl(p->striping_unit);
}
void proto_cfg2_req_ntoh(cfg_dss2_req_t *p)
{
    p->n            = ntohl(p->n);
    p->striping_unit= ntohl(p->striping_unit);
}

void proto_ls_req_hton(ls_req_t *p)
{
    (void)p;
}
void proto_ls_req_ntoh(ls_req_t *p)
{
    (void)p;
}

void proto_put_req_hton(put_file_req_t *p)
{
    p->file_size = htonl(p->file_size);
}
void proto_put_req_ntoh(put_file_req_t *p)
{
    p->file_size = ntohl(p->file_size);
}

void proto_read_req_hton(read_file_req_t *p)
{
    (void)p;
}
void proto_read_req_ntoh(read_file_req_t *p)
{
    (void)p;
}

void proto_fail_disk_req_hton(fail_disk_req_t *p)
{
    (void)p;
}
void proto_fail_disk_req_ntoh(fail_disk_req_t *p)
{
    (void)p;
}

void proto_decom_req_hton(decom_dss_req_t *p)
{
    (void)p;
}
void proto_decom_req_ntoh(decom_dss_req_t *p)
{
    (void)p;
}
