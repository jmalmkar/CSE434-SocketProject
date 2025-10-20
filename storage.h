#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <netinet/in.h>
#include "protocol.h"
#include "state.h"

typedef struct {
    char filename[MAX_FILENAME];
    uint32_t total_size;
    uint32_t n_chunks;
    uint32_t chunk_len[4096];
    uint8_t  chunks[4096][MAX_CHUNK];
} file_rec_t;

typedef struct {
    int in_use;
    char owner[MAX_NAME_LEN];
    uint32_t n;                  /* # of disks in this DSS (replication factor) */
    uint32_t block_size;
    int disk_idx[MAX_DISKS];     /* indices into manager_state_t.disks[] */
    int failed[MAX_DISKS];       /* mark failed disks among the selected n */
    file_rec_t files[128];
} dss_rec_t;

typedef struct {
    dss_rec_t dsses[MAX_USERS];  /* one DSS per owner slot */
} storage_state_t;

void storage_init(storage_state_t *ss);
int storage_configure_dss(manager_state_t *st, storage_state_t *ss, const char *owner, uint32_t n, uint32_t block_sz);
int storage_put_open(storage_state_t *ss, const char *owner, const char *filename, uint32_t total_size);
int storage_put_chunk(storage_state_t *ss, const char *owner, const char *filename, uint32_t seq, const uint8_t *data, uint32_t nbytes);
int storage_put_close(storage_state_t *ss, const char *owner, const char *filename);
int storage_ls(storage_state_t *ss, const char *owner, uint8_t *out, uint32_t *out_len);
int storage_read_open(storage_state_t *ss, const char *owner, const char *filename, uint32_t *total_chunks);
int storage_read_chunk(storage_state_t *ss, const char *owner, const char *filename, uint32_t seq, uint8_t *buf, uint32_t *nbytes);
int storage_read_close(storage_state_t *ss, const char *owner, const char *filename);
int storage_fail_disk(manager_state_t *st, storage_state_t *ss, const char *owner, const char *disk_name);
int storage_decommission_dss(manager_state_t *st, storage_state_t *ss, const char *owner);

#endif /* STORAGE_H */

