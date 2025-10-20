#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "storage.h"

void storage_seed_random(void){
    static int seeded=0;
    if(!seeded){ srand((unsigned)time(NULL)); seeded=1; }
}

int storage_pick_random_free_disks(manager_state_t *st, uint32_t n, int out_idx[]){
    int tmp[MAX_DISKS]; int m=0;
    for (int i=0;i<MAX_DISKS;i++){
        if (st->disks[i].in_use && st->disks[i].state==DISK_FREE) tmp[m++]=i;
    }
    if ((uint32_t)m < n) return -1;
    for (int i=m-1;i>0;i--){
        int j = rand() % (i+1);
        int t = tmp[i]; tmp[i]=tmp[j]; tmp[j]=t;
    }
    for (uint32_t k=0;k<n;k++) out_idx[k]=tmp[k];
    return (int)n;
}

void storage_build_plan(manager_state_t *st, int dss_idx, plan_dss_t *out){
    memset(out,0,sizeof(*out));
    snprintf(out->dss_name,16,"%s",st->dsses[dss_idx].name);
    out->n = st->dsses[dss_idx].n;
    out->striping_unit = st->dsses[dss_idx].striping_unit;
    for (uint32_t i=0;i<out->n;i++){
        int di = st->dsses[dss_idx].disk_idx[i];
        snprintf(out->disks[i].disk_name, MAX_NAME_LEN, "%s", st->disks[di].name);
        out->disks[i].ip_be = st->disks[di].ip_be ? st->disks[di].ip_be : st->disks[di].addr.sin_addr.s_addr;
        out->disks[i].c_port = st->disks[di].c_port ? st->disks[di].c_port : ntohs(st->disks[di].addr.sin_port);
    }
}
