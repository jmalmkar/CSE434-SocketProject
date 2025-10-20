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
    OP_REGISTER_USER      = 1,
    OP_REGISTER_DISK      = 2,
    OP_CONFIGURE_DSS      = 3,
    OP_DEREGISTER_USER    = 4,
    OP_DEREGISTER_DISK    = 5,

    /* Named DSS & two-phase operations */
    OP_LS                 = 10,

    OP_COPY_BEGIN         = 20,
    OP_COPY_PLAN          = 21,
    OP_COPY_COMPLETE      = 22,

    OP_READ_BEGIN         = 30,
    OP_READ_PLAN          = 31,
    OP_READ_COMPLETE      = 32,

    OP_FAIL_BEGIN         = 40,
    OP_FAIL_PLAN          = 41,
    OP_RECOVERY_COMPLETE  = 42,

    OP_DECOM_BEGIN        = 50,
    OP_DECOM_PLAN         = 51,
    OP_DECOM_COMPLETE     = 52,

    OP_ACK                = 100,
    OP_ERR                = 101
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
    ST_DSS_NOT_READY      = 8,
    ST_BUSY               = 9
};

#pragma pack(push, 1)
typedef struct {
    uint8_t   version;
    uint8_t   opcode;
    uint16_t  reserved;
    uint32_t  req_id;     /* network order */
} msg_hdr_t;
#pragma pack(pop)

/* Registration */

#pragma pack(push, 1)
typedef struct {
    char      user_name[MAX_NAME_LEN];
    uint16_t  listen_port;   /* user listen (c-port); host order in code */
    uint16_t  pad0;
    uint32_t  ipv4_be;       /* optional; if zero manager uses src */
    uint16_t  m_port;        /* manager->user port */
    uint16_t  c_port;        /* peer command port */
} reg_user_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char      disk_name[MAX_NAME_LEN];
    uint32_t  capacity_blocks; 
    uint16_t  listen_port;     
    uint16_t  pad1;
    uint32_t  ipv4_be;         /* optional; if zero manager uses src */
    uint16_t  m_port;          /* manager->disk port */
    uint16_t  c_port;          /* peer command port */
} reg_disk_req_t;
#pragma pack(pop)

/* Configure DSS (Named) */

#pragma pack(push, 1)
typedef struct {
    char      dss_name[16];  
    uint32_t  n;               /* host order */
    uint32_t  striping_unit;   /* host order */
} cfg_dss_req_t;
#pragma pack(pop)

/* De/Register simple */

#pragma pack(push, 1)
typedef struct { char user_name[MAX_NAME_LEN]; } dereg_user_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct { char disk_name[MAX_NAME_LEN]; } dereg_disk_req_t;
#pragma pack(pop)

/* Generic Response */

#pragma pack(push, 1)
typedef struct {
    uint8_t   status;    
    uint8_t   _pad[3];
    char      msg[MAX_MSG_LEN]; 
} resp_t;
#pragma pack(pop)

/* LS */

#pragma pack(push, 1)
typedef struct {
    /* List all DSSs */  
    uint8_t dummy;
} ls_req_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint32_t nbytes;              /* host order */
    uint8_t  data[1024];       
} ls_resp_t;
#pragma pack(pop)

/* COPY */

#pragma pack(push, 1)
typedef struct {
    char dss_name[16];            /* if empty: manager chooses a DSS owned by user */
    char file_name[MAX_FILENAME];
    uint32_t file_size;           /* host order */
    char owner[MAX_NAME_LEN];
} copy_begin_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char disk_name[MAX_NAME_LEN];
    uint32_t ip_be;
    uint16_t c_port;
    uint16_t _pad;
} disk_triplet_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char dss_name[16];
    uint32_t n;                   /* host */
    uint32_t striping_unit;       /* host */
    disk_triplet_t disks[64];     /* subset used: n */
} plan_dss_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char dss_name[16];
    char file_name[MAX_FILENAME];
    uint32_t file_size;           /* host order */
    char owner[MAX_NAME_LEN];
} copy_complete_t;
#pragma pack(pop)

/* READ */

#pragma pack(push, 1)
typedef struct {
    char dss_name[16];
    char file_name[MAX_FILENAME];
    char user_name[MAX_NAME_LEN];   /* attempting reader, must equal owner */
} read_begin_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char dss_name[16];
    char file_name[MAX_FILENAME];
    uint32_t file_size;             /* host */
    plan_dss_t plan;                /* nested plan for convenience */
} read_plan_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char dss_name[16];
    char file_name[MAX_FILENAME];
    char user_name[MAX_NAME_LEN];
} read_complete_t;
#pragma pack(pop)

/* FAIL */

#pragma pack(push, 1)
typedef struct { char dss_name[16]; } fail_begin_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct { char dss_name[16]; } recovery_complete_t;
#pragma pack(pop)

/* DECOMMISSION */

#pragma pack(push, 1)
typedef struct { char dss_name[16]; } decom_begin_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct { char dss_name[16]; } decom_complete_t;
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



