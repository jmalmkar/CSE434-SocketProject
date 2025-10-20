// protocol.h

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTO_VERSION 1
#define MAX_NAME_LEN  32
#define MAX_MSG_LEN   256

#define MAX_FILENAME  64
#define MAX_CHUNK     512

/* Opcodes */
enum {
    OP_REGISTER_USER    = 1,
    OP_REGISTER_DISK    = 2,
    OP_CONFIGURE_DSS    = 3,
    OP_DEREGISTER_USER  = 4,
    OP_DEREGISTER_DISK  = 5,

    OP_PUT_OPEN         = 10,
    OP_PUT_CHUNK        = 11,
    OP_PUT_CLOSE        = 12,
    OP_LS               = 13,
    OP_READ_OPEN        = 14,
    OP_READ_CHUNK       = 15,
    OP_READ_CLOSE       = 16,
    OP_FAIL_DISK        = 17,
    OP_DECOMMISSION_DSS = 18,

    OP_ACK              = 100,
    OP_ERR              = 101
};

/* Status codes used in ACK/ERR */
enum {
    ST_OK                 = 0,
    ST_ALREADY_REGISTERED = 1,
    ST_NOT_REGISTERED     = 2,
    ST_INSUFFICIENT_DISKS = 3,
    ST_BAD_PARAMS         = 4,
    ST_INTERNAL           = 5,

    ST_NOT_OWNER          = 6,
    ST_NOT_FOUND          = 7,
    ST_DSS_NOT_READY      = 8
};

#pragma pack(push, 1)
typedef struct {
    uint8_t   version;
    uint8_t   opcode;
    uint16_t  reserved;
    uint32_t  req_id;     /* network order */
} msg_hdr_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char      user_name[MAX_NAME_LEN];
    uint16_t  listen_port; /* host order in code; convert at send/recv */
    uint16_t  pad;
} reg_user_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char      disk_name[MAX_NAME_LEN];
    uint32_t  capacity_blocks; /* host order in code; convert at send/recv */
    uint16_t  listen_port;     /* host order in code; convert at send/recv */
    uint16_t  pad;
} reg_disk_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint32_t  n;          /* host order in code; convert at send/recv */
    uint32_t  block_size; /* host order in code; convert at send/recv */
} cfg_dss_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char user_name[MAX_NAME_LEN];
} dereg_user_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char disk_name[MAX_NAME_LEN];
} dereg_disk_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t   status;    
    uint8_t   _pad[3];
    char      msg[MAX_MSG_LEN]; 
} resp_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char owner[MAX_NAME_LEN];     /* manager infers from sender */
    char filename[MAX_FILENAME];
    uint32_t total_size;          /* host order in code */
} put_open_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char filename[MAX_FILENAME];
    uint32_t seq;                 /* host order in code */
    uint32_t nbytes;              /* host order in code; <= MAX_CHUNK */
    uint8_t  data[MAX_CHUNK];
} put_chunk_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct { char filename[MAX_FILENAME]; } put_close_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct { char owner[MAX_NAME_LEN]; } ls_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint32_t nbytes;              /* host order */
    uint8_t  data[512];           /* newline-separated */
} ls_resp_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char owner[MAX_NAME_LEN];
    char filename[MAX_FILENAME];
} read_open_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char filename[MAX_FILENAME];
    uint32_t seq;                 /* host order */
} read_chunk_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct { char filename[MAX_FILENAME]; } read_close_t;
#pragma pack(pop)

/* Helpers implemented in protocol.c */
void proto_make_hdr(msg_hdr_t *h, uint8_t opcode, uint32_t req_id_host);

void proto_user_req_hton(reg_user_req_t *p);
void proto_user_req_ntoh(reg_user_req_t *p);

void proto_disk_req_hton(reg_disk_req_t *p);
void proto_disk_req_ntoh(reg_disk_req_t *p);

void proto_cfg_req_hton(cfg_dss_req_t *p);
void proto_cfg_req_ntoh(cfg_dss_req_t *p);

#endif /* PROTOCOL_H */



