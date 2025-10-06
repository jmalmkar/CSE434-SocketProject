# CSE434-SocketProject
# Distributed Storage System (DSS) — Milestone 1

## Overall Goal
Implement a socket-based Distributed Storage System (DSS) with three programs, **manager**, **user**, and **disk**, that communicate over UDP. The manager maintains global DSS state; users and disks register with the manager; a user can request configuration of a DSS of size _n_ using free disks. This repository contains the **Milestone 1** implementation (registration, configuration, and deregistration).

> **Port ranges:** set your group once per shell (`export DSS_GROUP=<G>`). The tools warn if you use a UDP port outside your group’s 100-port block. This specifically will be G = 145.

---

## Directory Contents

- `Makefile` — builds `manager`, `user`, and `disk` from the C sources.
- `common.h` — small helpers shared by all binaries (fatal error handler, header builder, group-port warning).
- `protocol.h` — protocol constants, message opcodes/status codes, and packed wire-format structs.
- `protocol.c` — minimal helpers for host/network byte order for message bodies (not required by all call sites).
- `state.h` — in-memory manager state and tiny helper functions (add/find/count) for users/disks and DSS params.
- `manager.c` — UDP server loop; parses requests by opcode; mutates state; responds with ACK/ERR.
- `user.c` — simple UDP client supporting `register-user`, `configure-dss`, `deregister-user`.
- `disk.c` — simple UDP client supporting `register-disk`, `deregister-disk`.

---

## Build

```bash
make
# produces: ./manager ./user ./disk
