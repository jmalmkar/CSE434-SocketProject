#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTO_VERSION 1
#define MAX_NAME_LEN  32
#define MAX_MSG_LEN   256

/* Opcodes (match project text) */
enum {
    OP_REGISTER_USER    = 1,
    OP_REGISTER_DISK    = 2,
    OP_CONFIGURE_DSS    = 3,
    OP_DEREGISTER_USER  = 4,
    OP_DEREGISTER_DISK  = 5,
    OP_ACK              = 6,
    OP_ERR              = 7
};

/* Return/status codes (stick to project’s set) */
enum {
    ST_OK                   = 0,
    ST_ALREADY_REGISTERED   = 1,
    ST_NOT_REGISTERED       = 2,
    ST_INSUFFICIENT_DISKS   = 3,
    ST_BAD_PARAMS           = 4,
    ST_INTERNAL             = 5
};

/* Common header */
#pragma pack(push, 1)
typedef struct {
    uint8_t   version;   /* PROTO_VERSION */
    uint8_t   opcode;    /* OP_* */
    uint16_t  reserved;  /* 0 */
    uint32_t  req_id;    /* network order */
} msg_hdr_t;
#pragma pack(pop)

/* REGISTER-USER request */
#pragma pack(push, 1)
typedef struct {
    char      user_name[MAX_NAME_LEN];  /* NUL-terminated */
    uint16_t  listen_port;              /* network order */
    uint16_t  pad;
} reg_user_req_t;
#pragma pack(pop)

/* REGISTER-DISK request */
#pragma pack(push, 1)
typedef struct {
    char      disk_name[MAX_NAME_LEN];
    uint32_t  capacity_blocks;          /* network order */
    uint16_t  listen_port;              /* network order */
    uint16_t  pad;
} reg_disk_req_t;
#pragma pack(pop)

/* CONFIGURE-DSS request */
#pragma pack(push, 1)
typedef struct {
    uint32_t  n;          /* network order */
    uint32_t  block_size; /* network order */
} cfg_dss_req_t;
#pragma pack(pop)

/* DEREGISTER-USER request */
#pragma pack(push, 1)
typedef struct {
    char user_name[MAX_NAME_LEN];
} dereg_user_req_t;
#pragma pack(pop)

/* DEREGISTER-DISK request */
#pragma pack(push, 1)
typedef struct {
    char disk_name[MAX_NAME_LEN];
} dereg_disk_req_t;
#pragma pack(pop)

/* ACK/ERR response */
#pragma pack(push, 1)
typedef struct {
    uint8_t   status;             /* ST_* */
    uint8_t   _pad[3];
    char      msg[MAX_MSG_LEN];   /* NUL-terminated */
} resp_t;
#pragma pack(pop)

/* Optional helper prototypes implemented in protocol.c */
void proto_make_hdr(msg_hdr_t *h, uint8_t opcode, uint32_t req_id_host);

void proto_user_req_hton(reg_user_req_t *p);
void proto_user_req_ntoh(reg_user_req_t *p);

void proto_disk_req_hton(reg_disk_req_t *p);
void proto_disk_req_ntoh(reg_disk_req_t *p);

void proto_cfg_req_hton(cfg_dss_req_t *p);
void proto_cfg_req_ntoh(cfg_dss_req_t *p);

#endif /* PROTOCOL_H */
