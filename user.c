// user.c

#include "common.h"
#include "protocol.h"

/* Send a prepared buffer and print response */
static void send_and_recv(int sock, struct sockaddr_in *mgr, const void *out, size_t outlen,
                          const char *label)
{
    if (sendto(sock, out, outlen, 0, (struct sockaddr *)mgr, sizeof(*mgr)) < 0)
        DieWithError(label);

    uint8_t inb[2048]; struct sockaddr_in from; socklen_t fromlen=sizeof(from);
    ssize_t got = recvfrom(sock, inb, sizeof(inb), 0, (struct sockaddr *)&from, &fromlen);
    if (got < (ssize_t)(sizeof(msg_hdr_t))) DieWithError("user: recvfrom failed");

    msg_hdr_t rh; memcpy(&rh, inb, sizeof(rh));
    if (rh.opcode==OP_ACK || rh.opcode==OP_ERR) {
        resp_t rr; memcpy(&rr, inb+sizeof(rh), sizeof(rr));
        printf("user: rx %s req_id=%u status=%d msg=\"%s\"\n",
               (rh.opcode==OP_ACK?"ACK":"ERR"), ntohl(rh.req_id), rr.status, rr.msg);
    } else if (rh.opcode==OP_LS) {
        ls_resp_t lr; memcpy(&lr, inb+sizeof(rh), sizeof(lr));
        printf("user: ls results (%u bytes):\n%.*s", lr.nbytes, (int)lr.nbytes, lr.data);
    } else if (rh.opcode==OP_READ_CHUNK) {
        put_chunk_t pc; memcpy(&pc, inb+sizeof(rh), sizeof(pc));
        fwrite(pc.data, 1, pc.nbytes, stdout);
    } else {
        printf("user: rx opcode=%u len=%zd\n", rh.opcode, got);
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr,
          "Usage:\n"
          "  %s <manager_ip> <manager_port> register-user <user_name> <user_listen_port>\n"
          "  %s <manager_ip> <manager_port> configure-dss <n> <block_size>\n"
          "  %s <manager_ip> <manager_port> copy <filename>\n"
          "  %s <manager_ip> <manager_port> ls <owner>\n"
          "  %s <manager_ip> <manager_port> read <owner> <filename>\n"
          "  %s <manager_ip> <manager_port> fail-disk <disk_name>\n"
          "  %s <manager_ip> <manager_port> decommission-dss\n"
          "  %s <manager_ip> <manager_port> deregister-user <user_name>\n",
          argv[0],argv[0],argv[0],argv[0],argv[0],argv[0],argv[0],argv[0]);
        exit(1);
    }
    const char *mgr_ip = argv[1];
    unsigned short mgr_port = (unsigned short)atoi(argv[2]);
    maybe_warn_port_range(mgr_port, "user");

    int sock;
    struct sockaddr_in mgr, from;
    socklen_t fromlen = sizeof(from);

    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("user: socket() failed");

    memset(&mgr, 0, sizeof(mgr));
    mgr.sin_family      = AF_INET;
    mgr.sin_addr.s_addr = inet_addr(mgr_ip);
    mgr.sin_port        = htons(mgr_port);

    const char *cmd = argv[3];
    uint32_t req_id = (uint32_t)time(NULL) ^ (uint32_t)getpid();

    
    uint8_t out[2048];

    if (strcmp(cmd, "register-user") == 0) {
        if (argc != 6) { fprintf(stderr, "Bad args for register-user\n"); exit(1); }
        msg_hdr_t h; make_hdr(&h, OP_REGISTER_USER, req_id);

        reg_user_req_t p; memset(&p, 0, sizeof(p));
        strncpy(p.user_name, argv[4], MAX_NAME_LEN-1);
        p.listen_port = htons((unsigned short)atoi(argv[5]));

        memcpy(out, &h, sizeof(h));
        memcpy(out + sizeof(h), &p, sizeof(p));
        printf("user: tx REGISTER_USER req_id=%u name=%s port=%u\n",
               req_id, argv[4], (unsigned)ntohs(p.listen_port));

        send_and_recv(sock, &mgr, out, sizeof(h)+sizeof(p), "user: send REG_USER failed");
    }
    else if (strcmp(cmd, "configure-dss") == 0) {
        if (argc != 6) { fprintf(stderr, "Bad args for configure-dss\n"); exit(1); }
        msg_hdr_t h; make_hdr(&h, OP_CONFIGURE_DSS, req_id);

        cfg_dss_req_t p; memset(&p, 0, sizeof(p));
        p.n          = htonl((uint32_t)strtoul(argv[4], NULL, 10));
        p.block_size = htonl((uint32_t)strtoul(argv[5], NULL, 10));

        memcpy(out, &h, sizeof(h));
        memcpy(out + sizeof(h), &p, sizeof(p));
        printf("user: tx CONFIGURE_DSS req_id=%u n=%s block_size=%s\n", req_id, argv[4], argv[5]);

        send_and_recv(sock, &mgr, out, sizeof(h)+sizeof(p), "user: send CONFIGURE_DSS failed");
    }
    else if (strcmp(cmd, "copy") == 0) {
        if (argc != 5) { fprintf(stderr, "Bad args for copy\n"); exit(1); }
        const char *fname = argv[4];

        FILE *fp = fopen(fname, "rb");
        if (!fp) { perror("open file"); exit(1); }
        fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);

        /* PUT_OPEN */
        msg_hdr_t h; make_hdr(&h, OP_PUT_OPEN, req_id);
        put_open_t po; memset(&po, 0, sizeof(po));
        strncpy(po.filename, fname, MAX_FILENAME-1);
        po.total_size = (uint32_t)sz; /* host order */

        memcpy(out, &h, sizeof(h));
        memcpy(out + sizeof(h), &po, sizeof(po));
        printf("user: tx PUT_OPEN req_id=%u file=%s size=%ld\n", req_id, fname, sz);
        send_and_recv(sock, &mgr, out, sizeof(h)+sizeof(po), "user: PUT_OPEN failed");

        /* PUT_CHUNK loop */
        uint32_t seq=0;
        for (;;) {
            uint8_t buf[MAX_CHUNK];
            size_t n = fread(buf, 1, MAX_CHUNK, fp);

            msg_hdr_t hc; make_hdr(&hc, OP_PUT_CHUNK, req_id);
            put_chunk_t pc; memset(&pc, 0, sizeof(pc));
            strncpy(pc.filename, fname, MAX_FILENAME-1);
            pc.seq    = seq;             /* host order */
            pc.nbytes = (uint32_t)n;     /* host order */
            memcpy(pc.data, buf, n);

            memcpy(out, &hc, sizeof(hc));
            memcpy(out + sizeof(hc), &pc, sizeof(pc));
            printf("user: tx PUT_CHUNK req_id=%u file=%s seq=%u n=%zu\n", req_id, fname, seq, n);
            send_and_recv(sock, &mgr, out, sizeof(hc)+sizeof(pc), "user: PUT_CHUNK failed");

            if (n < MAX_CHUNK) break;
            seq++;
        }
        fclose(fp);

        /* PUT_CLOSE */
        msg_hdr_t ht; make_hdr(&ht, OP_PUT_CLOSE, req_id);
        put_close_t cl; memset(&cl, 0, sizeof(cl));
        strncpy(cl.filename, fname, MAX_FILENAME-1);

        memcpy(out, &ht, sizeof(ht));
        memcpy(out + sizeof(ht), &cl, sizeof(cl));
        printf("user: tx PUT_CLOSE req_id=%u file=%s\n", req_id, fname);
        send_and_recv(sock, &mgr, out, sizeof(ht)+sizeof(cl), "user: PUT_CLOSE failed");
    }
    else if (strcmp(cmd, "ls") == 0) {
        if (argc != 5) { fprintf(stderr, "Bad args for ls\n"); exit(1); }
        const char *owner = argv[4];

        msg_hdr_t h; make_hdr(&h, OP_LS, req_id);
        ls_req_t rq; memset(&rq, 0, sizeof(rq));
        strncpy(rq.owner, owner, MAX_NAME_LEN-1);

        memcpy(out, &h, sizeof(h));
        memcpy(out + sizeof(h), &rq, sizeof(rq));
        printf("user: tx LS req_id=%u owner=%s\n", req_id, owner);
        send_and_recv(sock, &mgr, out, sizeof(h)+sizeof(rq), "user: LS failed");
    }
    else if (strcmp(cmd, "read") == 0) {
        if (argc != 6) { fprintf(stderr, "Bad args for read\n"); exit(1); }
        const char *owner = argv[4];
        const char *fname = argv[5];

        /* READ_OPEN */
        msg_hdr_t h; make_hdr(&h, OP_READ_OPEN, req_id);
        read_open_t ro; memset(&ro, 0, sizeof(ro));
        strncpy(ro.owner, owner, MAX_NAME_LEN-1);
        strncpy(ro.filename, fname, MAX_FILENAME-1);

        memcpy(out, &h, sizeof(h));
        memcpy(out + sizeof(h), &ro, sizeof(ro));
        printf("user: tx READ_OPEN req_id=%u owner=%s file=%s\n", req_id, owner, fname);
        send_and_recv(sock, &mgr, out, sizeof(h)+sizeof(ro), "user: READ_OPEN failed");

        for (uint32_t seq=0; seq<128; seq++) {
            msg_hdr_t hc; make_hdr(&hc, OP_READ_CHUNK, req_id);
            read_chunk_req_t rq; memset(&rq, 0, sizeof(rq));
            strncpy(rq.filename, fname, MAX_FILENAME-1);
            rq.seq = seq; /* host order */

            memcpy(out, &hc, sizeof(hc));
            memcpy(out + sizeof(hc), &rq, sizeof(rq));
            printf("user: tx READ_CHUNK req_id=%u file=%s seq=%u\n", req_id, fname, seq);
            send_and_recv(sock, &mgr, out, sizeof(hc)+sizeof(rq), "user: READ_CHUNK failed");
        }

        /* READ_CLOSE */
        msg_hdr_t ht; make_hdr(&ht, OP_READ_CLOSE, req_id);
        read_close_t rc; memset(&rc, 0, sizeof(rc));
        strncpy(rc.filename, fname, MAX_FILENAME-1);

        memcpy(out, &ht, sizeof(ht));
        memcpy(out + sizeof(ht), &rc, sizeof(rc));
        printf("user: tx READ_CLOSE req_id=%u file=%s\n", req_id, fname);
        send_and_recv(sock, &mgr, out, sizeof(ht)+sizeof(rc), "user: READ_CLOSE failed");
    }
    else if (strcmp(cmd, "fail-disk") == 0) {
        if (argc != 5) { fprintf(stderr, "Bad args for fail-disk\n"); exit(1); }
        const char *dname = argv[4];

        msg_hdr_t h; make_hdr(&h, OP_FAIL_DISK, req_id);
        dereg_disk_req_t rq; memset(&rq, 0, sizeof(rq));
        strncpy(rq.disk_name, dname, MAX_NAME_LEN-1);

        memcpy(out, &h, sizeof(h));
        memcpy(out + sizeof(h), &rq, sizeof(rq));
        printf("user: tx FAIL_DISK req_id=%u disk=%s\n", req_id, dname);
        send_and_recv(sock, &mgr, out, sizeof(h)+sizeof(rq), "user: FAIL_DISK failed");
    }
    else if (strcmp(cmd, "decommission-dss") == 0) {
        msg_hdr_t h; make_hdr(&h, OP_DECOMMISSION_DSS, req_id);

        memcpy(out, &h, sizeof(h));
        printf("user: tx DECOMMISSION_DSS req_id=%u\n", req_id);
        send_and_recv(sock, &mgr, out, sizeof(h), "user: DECOMMISSION_DSS failed");
    }
    else if (strcmp(cmd, "deregister-user") == 0) {
        if (argc != 5) { fprintf(stderr, "Bad args for deregister-user\n"); exit(1); }
        msg_hdr_t h; make_hdr(&h, OP_DEREGISTER_USER, req_id);

        dereg_user_req_t p; memset(&p, 0, sizeof(p));
        strncpy(p.user_name, argv[4], MAX_NAME_LEN-1);

        memcpy(out, &h, sizeof(h));
        memcpy(out + sizeof(h), &p, sizeof(p));
        printf("user: tx DEREGISTER_USER req_id=%u name=%s\n", req_id, argv[4]);

        send_and_recv(sock, &mgr, out, sizeof(h)+sizeof(p), "user: send DEREG failed");
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        exit(1);
    }

    close(sock);
    return 0;
}

