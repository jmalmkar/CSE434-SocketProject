#include <string.h>
#include <stdio.h>
#include "storage.h"

/* Manager-side DSS storage helpers */

static dss_rec_t* find_dss_by_owner(storage_state_t *ss, const char *owner) {
    for (int i=0; i<(int)(sizeof(ss->dsses)/sizeof(ss->dsses[0])); ++i) {
        if (ss->dsses[i].in_use && strncmp(ss->dsses[i].owner, owner, MAX_NAME_LEN)==0) return &ss->dsses[i];
    }
    return NULL;
}
static dss_rec_t* ensure_slot(storage_state_t *ss, const char *owner) {
    dss_rec_t *d = find_dss_by_owner(ss, owner);
    if (d) return d;
    for (int i=0; i<(int)(sizeof(ss->dsses)/sizeof(ss->dsses[0])); ++i) {
        if (!ss->dsses[i].in_use) {
            memset(&ss->dsses[i], 0, sizeof(ss->dsses[i]));
            ss->dsses[i].in_use = 1;
            snprintf(ss->dsses[i].owner, MAX_NAME_LEN, "%s", owner);
            return &ss->dsses[i];
        }
    }
    return NULL;
}

void storage_init(storage_state_t *ss) { memset(ss, 0, sizeof(*ss)); }

int storage_configure_dss(manager_state_t *st, storage_state_t *ss,
                          const char *owner, uint32_t n, uint32_t block_sz)
{
    if (!owner || n==0) return -1;
    if (count_free_disks(st) < (int)n) return -2;
    dss_rec_t *d = ensure_slot(ss, owner);
    if (!d) return -3;

    /* pick first n free disks and mark them in both places */
    int marked = 0;
    for (int i=0; i<MAX_DISKS && marked<(int)n; i++) {
        if (st->disks[i].in_use && !st->disks[i].in_dss) {
            st->disks[i].in_dss = 1;
            d->disk_idx[marked] = i;
            d->failed[marked]   = 0;
            marked++;
        }
    }
    d->n = n;
    d->block_size = block_sz;
    return 0;
}

static file_rec_t* get_file_slot(dss_rec_t *d, const char *filename) {
    for (int i=0; i<128; i++) {
        if (d->files[i].total_size && strncmp(d->files[i].filename, filename, MAX_FILENAME)==0) return &d->files[i];
    }
    for (int i=0; i<128; i++) {
        if (d->files[i].total_size == 0) {
            memset(&d->files[i], 0, sizeof(d->files[i]));
            snprintf(d->files[i].filename, MAX_FILENAME, "%s", filename);
            return &d->files[i];
    }   }
    return NULL;
}

int storage_put_open(storage_state_t *ss, const char *owner,
                     const char *filename, uint32_t total_size)
{
    dss_rec_t *d = find_dss_by_owner(ss, owner);
    if (!d || d->n==0) return -1;
    file_rec_t *f = get_file_slot(d, filename);
    if (!f) return -2;
    f->total_size = total_size;
    f->n_chunks = 0;
    return 0;
}

int storage_put_chunk(storage_state_t *ss, const char *owner,
                      const char *filename, uint32_t seq, const uint8_t *data, uint32_t nbytes)
{
    dss_rec_t *d = find_dss_by_owner(ss, owner);
    if (!d || d->n==0) return -1;

    file_rec_t *f = NULL;
    for (int i=0; i<128; i++) {
        if (d->files[i].total_size && strncmp(d->files[i].filename, filename, MAX_FILENAME)==0) { f=&d->files[i]; break; }
    }
    if (!f) return -2;
    if (seq >= 4096 || nbytes > MAX_CHUNK) return -3;

    memcpy(f->chunks[seq], data, nbytes);
    f->chunk_len[seq] = nbytes;
    if (seq+1 > f->n_chunks) f->n_chunks = seq+1;
    /* replication simulated and data considered on all disks unless all failed */
    return 0;
}

int storage_put_close(storage_state_t *ss, const char *owner, const char *filename) {
    (void)ss; (void)owner; (void)filename;
    return 0;
}

int storage_ls(storage_state_t *ss, const char *owner, uint8_t *out, uint32_t *out_len) {
    dss_rec_t *d = find_dss_by_owner(ss, owner);
    if (!d || d->n==0) return -1;
    uint32_t w=0;
    for (int i=0; i<128; i++) {
        if (d->files[i].total_size) {
            int n = snprintf((char*)out+w, 512-w, "%s\n", d->files[i].filename);
            if (n<0) break;
            w += (uint32_t)n;
            if (w>=512) break;
        }
    }
    *out_len = w;
    return 0;
}

int storage_read_open(storage_state_t *ss, const char *owner, const char *filename,
                      uint32_t *total_chunks)
{
    dss_rec_t *d = find_dss_by_owner(ss, owner);
    if (!d || d->n==0) return -1;

    for (int i=0; i<128; i++) {
        if (d->files[i].total_size && strncmp(d->files[i].filename, filename, MAX_FILENAME)==0) {
            *total_chunks = d->files[i].n_chunks;
            int ok = 0;
            for (uint32_t k=0; k<d->n; k++) if (!d->failed[k]) { ok=1; break; }
            return ok ? 0 : -2; /* -2 means all replicas failed */
        }
    }
    return -3; /* not found */
}

int storage_read_chunk(storage_state_t *ss, const char *owner, const char *filename,
                       uint32_t seq, uint8_t *buf, uint32_t *nbytes)
{
    dss_rec_t *d = find_dss_by_owner(ss, owner);
    if (!d || d->n==0) return -1;
    for (int i=0; i<128; i++) {
        if (d->files[i].total_size && strncmp(d->files[i].filename, filename, MAX_FILENAME)==0) {
            if (seq >= d->files[i].n_chunks) return -2;
            *nbytes = d->files[i].chunk_len[seq];
            memcpy(buf, d->files[i].chunks[seq], *nbytes);
            return 0;
        }
    }
    return -3;
}

int storage_read_close(storage_state_t *ss, const char *owner, const char *filename) {
    (void)ss; (void)owner; (void)filename;
    return 0;
}

int storage_fail_disk(manager_state_t *st, storage_state_t *ss, const char *owner, const char *disk_name) {
    dss_rec_t *d = find_dss_by_owner(ss, owner);
    if (!d || d->n==0) return -1;
    for (uint32_t k=0; k<d->n; k++) {
        int di = d->disk_idx[k];
        if (di>=0 && di<MAX_DISKS && st->disks[di].in_use &&
            strncmp(st->disks[di].name, disk_name, MAX_NAME_LEN)==0) {
            d->failed[k] = 1;
            return 0;
        }
    }
    return -2;
}

int storage_decommission_dss(manager_state_t *st, storage_state_t *ss, const char *owner) {
    dss_rec_t *d = find_dss_by_owner(ss, owner);
    if (!d) return -1;
    for (uint32_t k=0; k<d->n; k++) {
        int di = d->disk_idx[k];
        if (di>=0 && di<MAX_DISKS) st->disks[di].in_dss = 0;
    }
    memset(d, 0, sizeof(*d));
    return 0;
}

