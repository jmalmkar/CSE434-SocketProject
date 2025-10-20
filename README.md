# CSE434-SocketProject
# Distributed Storage System (DSS) — Milestone 1

## Overall Goal
Implement a socket-based Distributed Storage System (DSS) with three programs, **manager**, **user**, and **disk**, that communicate over UDP. The manager maintains global DSS state; users and disks register with the manager; a user can request configuration of a DSS of size _n_ using free disks. Users can configure named DSS arrays, copy and read files, simulate disk failures, and decommission complete DSS configurations.

This final version extends Milestone 1 to support all commands required by the project specification, including multi-DSS management, two-phase operations, ownership enforcement, and failure handling.

> **Port ranges:** set your group once per shell (`export DSS_GROUP=<G>`). The tools warn if you use a UDP port outside your group’s 100-port block. This specifically will be G = 145.

---

## Directory Contents

- `Makefile` — builds `manager`, `user`, and `disk` from the C sources.
- `common.h` — small helpers shared by all binaries (fatal error handler, header builder, group-port warning).
- `protocol.h` — protocol constants, message opcodes/status codes, and packed wire-format structs.
- `protocol.c` — minimal helpers for host/network byte order for message bodies (not required by all call sites).
- `state.h` — in-memory manager state and helper functions (add/find/count) for users/disks and DSS params.
- `manager.c` — UDP server loop; parses requests by opcode; mutates state; responds with ACK/ERR.
- `user.c` — simple UDP client supporting `register-user`, `configure-dss`, `deregister-user`, `copy`, `read`, `ls`, `disk-failue`, and `decommission-dss`.
- `disk.c` — simple UDP client supporting `register-disk`, `deregister-disk`.
- `day-of-affirmation.txt`, `in-flanders.txt`, `tale-of-two-cities.txt`, `wizard-of-oz.txt` — sample text files used for copy/read testing
---

## Design Decisions
● UDP chosen for simplicity and alignment with project requirements.
● Manager remains single-threaded (event loop). Disk and user programs each run as single processes without extra threads.
● Two-phase commands (copy, read, disk-failure, decommission) implemented as blocking critical sections in the manager loop to prevent interleaved state updates.
● Each user can own multiple DSSs; each disk belongs to at most one DSS.
● All parameters checked for duplicates, port uniqueness, and value ranges before acceptance.
● Uniform trace format (“user: tx”, “manager: rx”) for grading clarity.
● Up to 64 users/disks/DSS records stored in static arrays.
● Disk failures do not erase file metadata; reads succeed if one replica remains logical.
---

## Implemented Functionality
Supported Commands
1	register-user	
  - Register user: unique name, IPv4, manager port, client port.
2	register-disk
  - Register disk: unique name, IPv4, capacity, command port.
3	configure-dss
  - Configure named DSS with n ≥ 3 Free disks and power-of-two striping unit (128 B–1 MB).
4	ls
  - List all configured DSS arrays and their files with sizes and owners.
5	copy	
  - COPY_BEGIN → manager returns plan → COPY_COMPLETE records file.
6	read	
  - ownership enforced; returns plan → READ_COMPLETE clears session.
7	disk-failure	
  - manager enters critical section, returns plan → RECOVERY_COMPLETE.
8	decommission-dss	
  - delete all files on DSS; mark disks Free; remove DSS.
9	deregister-user
  - Remove a user and associated session state.
10	deregister-disk
  - Remove a disk (fails if disk is currently in an active DSS).
---

## Build

```bash
make
# produces: ./manager ./user ./disk

