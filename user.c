// user.c

#include "common.h"
#include "protocol.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr,
          "Usage:\n"
          "  %s <manager_ip> <manager_port> register-user <user_name> <user_listen_port>\n"
          "  %s <manager_ip> <manager_port> configure-dss <n> <block_size>\n"
          "  %s <manager_ip> <manager_port> deregister-user <user_name>\n",
          argv[0], argv[0], argv[0]);
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

    uint8_t out[1024], inb[1024];
    msg_hdr_t h; make_hdr(&h, 0, req_id);

    if (strcmp(cmd, "register-user") == 0) {
        if (argc != 6) { fprintf(stderr, "Bad args for register-user\n"); exit(1); }
        h.opcode = OP_REGISTER_USER;

        reg_user_req_t p; memset(&p, 0, sizeof(p));
        strncpy(p.user_name, argv[4], MAX_NAME_LEN-1);
        p.listen_port = htons((unsigned short)atoi(argv[5]));

        memcpy(out, &h, sizeof(h));
        memcpy(out + sizeof(h), &p, sizeof(p));
        printf("user: tx REGISTER_USER req_id=%u name=%s port=%u\n",
               req_id, argv[4], (unsigned)ntohs(p.listen_port));

        if (sendto(sock, out, sizeof(h)+sizeof(p), 0, (struct sockaddr *)&mgr, sizeof(mgr)) < 0)
            DieWithError("user: sendto failed");
    }
    else if (strcmp(cmd, "configure-dss") == 0) {
        if (argc != 6) { fprintf(stderr, "Bad args for configure-dss\n"); exit(1); }
        h.opcode = OP_CONFIGURE_DSS;

        cfg_dss_req_t p; memset(&p, 0, sizeof(p));
        p.n          = htonl((uint32_t)strtoul(argv[4], NULL, 10));
        p.block_size = htonl((uint32_t)strtoul(argv[5], NULL, 10));

        memcpy(out, &h, sizeof(h));
        memcpy(out + sizeof(h), &p, sizeof(p));
        printf("user: tx CONFIGURE_DSS req_id=%u n=%s block_size=%s\n", req_id, argv[4], argv[5]);

        if (sendto(sock, out, sizeof(h)+sizeof(p), 0, (struct sockaddr *)&mgr, sizeof(mgr)) < 0)
            DieWithError("user: sendto failed");
    }
    else if (strcmp(cmd, "deregister-user") == 0) {
        if (argc != 5) { fprintf(stderr, "Bad args for deregister-user\n"); exit(1); }
        h.opcode = OP_DEREGISTER_USER;

        dereg_user_req_t p; memset(&p, 0, sizeof(p));
        strncpy(p.user_name, argv[4], MAX_NAME_LEN-1);

        memcpy(out, &h, sizeof(h));
        memcpy(out + sizeof(h), &p, sizeof(p));
        printf("user: tx DEREGISTER_USER req_id=%u name=%s\n", req_id, argv[4]);

        if (sendto(sock, out, sizeof(h)+sizeof(p), 0, (struct sockaddr *)&mgr, sizeof(mgr)) < 0)
            DieWithError("user: sendto failed");
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        exit(1);
    }

    /* receive ACK/ERR */
    ssize_t got = recvfrom(sock, inb, sizeof(inb), 0, (struct sockaddr *)&from, &fromlen);
    if (got < (ssize_t)(sizeof(msg_hdr_t) + sizeof(resp_t))) DieWithError("user: recvfrom failed");

    msg_hdr_t rh; memcpy(&rh, inb, sizeof(rh));
    resp_t rr; memcpy(&rr, inb + sizeof(rh), sizeof(rr));
    printf("user: rx %s req_id=%u status=%d msg=\"%s\"\n",
           (rh.opcode==OP_ACK?"ACK":"ERR"), ntohl(rh.req_id), rr.status, rr.msg);

    close(sock);
    return 0;
}

