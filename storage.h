#ifndef STORAGE_H
#define STORAGE_H

/* Manager-side utilities for DSS planning & random disk selection. */

#include <stdint.h>
#include "state.h"

void storage_seed_random(void); // seed rand()

/* Randomly pick n indices of Free disks (ordered), return count chosen or -1. */
int storage_pick_random_free_disks(manager_state_t *st, uint32_t n, int out_idx[]);

/* Build a plan_dss_t for a given DSS index. */
void storage_build_plan(manager_state_t *st, int dss_idx, plan_dss_t *out);

#endif /* STORAGE_H */

