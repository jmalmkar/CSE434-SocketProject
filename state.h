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

#define MAX_DSS   64         
#define MAX_FILES 256        
#define MAX_READS 128   

typedef enum { DISK_FREE=0, DISK_IN_DSS=1 } disk_state_t; 

/* Manager-side records for users and disks. */
typedef struct {
    int                 in_use;
    char                name[MAX_NAME_LEN];
    struct sockaddr_in  addr;         /* user’s IP:manager_port (m-port) */
    uint32_t            ip_be;      
    uint16_t            m_port;      
    uint16_t            c_port; 
} user_rec_t;

typedef struct {
    int                 in_use;
    disk_state_t        state;      
    char                name[MAX_NAME_LEN];
    uint32_t            capacity_blocks;
    struct sockaddr_in  addr;         /* disk’s IP:manager_port (m-port) */
    uint32_t            ip_be;      
    uint16_t            m_port;     
    uint16_t            c_port;      
} disk_rec_t;

/* DSS + files */
typedef struct {
    int        in_use;
    char       name[16];              /* dss-name */
    uint32_t   n;
    uint32_t   striping_unit;
    int        disk_idx[MAX_DISKS];   /* ordered indices into disks[] */
    int        failed_mask;           /* bitmask of failed disks (sim) */
    int        critical;              /* critical section flag */
} dss_rec_t;

typedef struct {
    int        in_use;
    char       dss_name[16];
    char       file_name[MAX_FILENAME];
    uint32_t   file_size;
    char       owner[MAX_NAME_LEN];
} file_rec_t;

typedef struct {
    int        in_use;
    char       user_name[MAX_NAME_LEN];
    char       dss_name[16];
    char       file_name[MAX_FILENAME];
} read_session_t;

typedef struct {
    user_rec_t     users[MAX_USERS];
    disk_rec_t     disks[MAX_DISKS];

    dss_rec_t      dsses[MAX_DSS];
    file_rec_t     files[MAX_FILES];
    read_session_t reads[MAX_READS];
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
static inline int add_user(manager_state_t *st, const char *name, const struct sockaddr_in *addr,
                           uint32_t ip_be, uint16_t m_port, uint16_t c_port) {
    if (find_user(st, name) >= 0) return -1;
    /* enforce (m_port,c_port) uniqueness among all processes */
    for (int i=0;i<MAX_USERS;i++) if (st->users[i].in_use) {
        if (st->users[i].m_port==m_port && st->users[i].c_port==c_port) return -2;
    }
    for (int i=0;i<MAX_DISKS;i++) if (st->disks[i].in_use) {
        if (st->disks[i].m_port==m_port && st->disks[i].c_port==c_port) return -2;
    }
    for (int i = 0; i < MAX_USERS; i++) if (!st->users[i].in_use) {
        st->users[i].in_use = 1;
        snprintf(st->users[i].name, MAX_NAME_LEN, "%s", name);
        st->users[i].name[MAX_NAME_LEN-1] = '\0';
        st->users[i].addr = *addr;
        st->users[i].ip_be = ip_be;
        st->users[i].m_port = m_port;
        st->users[i].c_port = c_port;
        return i;
    }
    return -3;
}
static inline int add_disk(manager_state_t *st, const char *name, uint32_t cap,
                           const struct sockaddr_in *addr, uint32_t ip_be, uint16_t m_port, uint16_t c_port) {
    if (find_disk(st, name) >= 0) return -1;
    for (int i=0;i<MAX_USERS;i++) if (st->users[i].in_use) {
        if (st->users[i].m_port==m_port && st->users[i].c_port==c_port) return -2;
    }
    for (int i=0;i<MAX_DISKS;i++) if (st->disks[i].in_use) {
        if (st->disks[i].m_port==m_port && st->disks[i].c_port==c_port) return -2;
    }
    for (int i = 0; i < MAX_DISKS; i++) if (!st->disks[i].in_use) {
        st->disks[i].in_use = 1;
        st->disks[i].state  = DISK_FREE;
        snprintf(st->disks[i].name, MAX_NAME_LEN, "%s", name);
        st->disks[i].capacity_blocks = cap;
        st->disks[i].addr = *addr;
        st->disks[i].ip_be = ip_be;
        st->disks[i].m_port = m_port;
        st->disks[i].c_port = c_port;
        return i;
    }
    return -3;
}
static inline int count_free_disks(manager_state_t *st) {
    int c = 0;
    for (int i = 0; i < MAX_DISKS; i++)
        if (st->disks[i].in_use && st->disks[i].state==DISK_FREE) c++;
    return c;
}

/* DSS helpers */
static inline int find_dss(manager_state_t *st, const char *dss_name){
    for (int i=0;i<MAX_DSS;i++) if (st->dsses[i].in_use && strncmp(st->dsses[i].name, dss_name, 16)==0) return i;
    return -1;
}
static inline int add_dss(manager_state_t *st, const char *dss_name, uint32_t n, uint32_t striping){
    if (find_dss(st,dss_name)>=0) return -1;
    for (int i=0;i<MAX_DSS;i++) if (!st->dsses[i].in_use){
        memset(&st->dsses[i],0,sizeof(st->dsses[i]));
        st->dsses[i].in_use = 1;
        snprintf(st->dsses[i].name,16,"%s",dss_name);
        st->dsses[i].n = n;
        st->dsses[i].striping_unit = striping;
        st->dsses[i].failed_mask = 0;
        st->dsses[i].critical = 0;
        return i;
    }
    return -2;
}
static inline file_rec_t* find_file_in_dss(manager_state_t *st, const char *dss_name, const char *fname){
    for (int i=0;i<MAX_FILES;i++){
        if (st->files[i].in_use && strncmp(st->files[i].dss_name,dss_name,16)==0 &&
            strncmp(st->files[i].file_name,fname,MAX_FILENAME)==0) return &st->files[i];
    }
    return NULL;
}
static inline file_rec_t* add_file(manager_state_t *st, const char *dss_name, const char *fname, uint32_t size, const char *owner){
    for (int i=0;i<MAX_FILES;i++) if (!st->files[i].in_use){
        st->files[i].in_use=1;
        snprintf(st->files[i].dss_name,16,"%s",dss_name);
        snprintf(st->files[i].file_name,MAX_FILENAME,"%s",fname);
        st->files[i].file_size=size;
        snprintf(st->files[i].owner,MAX_NAME_LEN,"%s",owner);
        return &st->files[i];
    }
    return NULL;
}
static inline read_session_t* add_read_session(manager_state_t *st, const char *user, const char *dss, const char *file){
    for (int i=0;i<MAX_READS;i++) if (!st->reads[i].in_use){
        st->reads[i].in_use=1;
        snprintf(st->reads[i].user_name,MAX_NAME_LEN,"%s",user);
        snprintf(st->reads[i].dss_name,16,"%s",dss);
        snprintf(st->reads[i].file_name,MAX_FILENAME,"%s",file);
        return &st->reads[i];
    }
    return NULL;
}
static inline void del_read_session(manager_state_t *st, const char *user, const char *dss, const char *file){
    for (int i=0;i<MAX_READS;i++) if (st->reads[i].in_use){
        if (strncmp(st->reads[i].user_name,user,MAX_NAME_LEN)==0 &&
            strncmp(st->reads[i].dss_name,dss,16)==0 &&
            strncmp(st->reads[i].file_name,file,MAX_FILENAME)==0) {
            st->reads[i].in_use=0; break;
        }
    }
}
static inline int reads_in_progress_for_dss(manager_state_t *st, const char *dss){
    for (int i=0;i<MAX_READS;i++) if (st->reads[i].in_use){
        if (strncmp(st->reads[i].dss_name,dss,16)==0) return 1;
    }
    return 0;
}

#endif /* STATE_H */

