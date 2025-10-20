// user.c

#include "common.h"
#include "protocol.h"

/* Common send/recv with milestone logging */
static void send_and_recv_print(int sock, struct sockaddr_in *mgr, const void *out, size_t outlen,
                          const char *label)
{
    if (sendto(sock, out, outlen, 0, (struct sockaddr *)mgr, sizeof(*mgr)) < 0)
        DieWithError(label);

    uint8_t inb[4096]; struct sockaddr_in from; socklen_t fromlen=sizeof(from);
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
    } else if (rh.opcode==OP_COPY_PLAN) {
        plan_dss_t plan; memcpy(&plan, inb+sizeof(rh), sizeof(plan));
        printf("user: rx COPY_PLAN dss=%s n=%u striping=%u\n", plan.dss_name, plan.n, plan.striping_unit);
        for (uint32_t i=0;i<plan.n;i++){
            struct in_addr a; a.s_addr = plan.disks[i].ip_be;
            printf("  disk%u %s %s %u\n", i, plan.disks[i].disk_name, inet_ntoa(a), plan.disks[i].c_port);
        }
    } else if (rh.opcode==OP_READ_PLAN) {
        read_plan_t rp; memcpy(&rp, inb+sizeof(rh), sizeof(rp));
        printf("user: rx READ_PLAN dss=%s file=%s size=%u n=%u striping=%u\n",
               rp.plan.dss_name, rp.file_name, rp.file_size, rp.plan.n, rp.plan.striping_unit);
    } else if (rh.opcode==OP_FAIL_PLAN) {
        plan_dss_t plan; memcpy(&plan, inb+sizeof(rh), sizeof(plan));
        printf("user: rx FAIL_PLAN dss=%s n=%u\n", plan.dss_name, plan.n);
    } else if (rh.opcode==OP_DECOM_PLAN) {
        plan_dss_t plan; memcpy(&plan, inb+sizeof(rh), sizeof(plan));
        printf("user: rx DECOM_PLAN dss=%s n=%u\n", plan.dss_name, plan.n);
    } else {
        printf("user: rx opcode=%u len=%zd\n", rh.opcode, got);
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr,
          "Usage (back-compat + final):\n"
          "  %s <manager_ip> <manager_port> register-user <user_name> <m_port> <c_port>\n"
          "  %s <manager_ip> <manager_port> register-user <user_name> <legacy_listen_port>\n"
          "  %s <manager_ip> <manager_port> configure-dss <dss_name> <n> <striping_unit>\n"
          "  %s <manager_ip> <manager_port> ls\n"
          "  %s <manager_ip> <manager_port> copy <dss_name|-> <file_name> <owner>\n"
          "  %s <manager_ip> <manager_port> read <dss_name> <file_name> <user_name>\n"
          "  %s <manager_ip> <manager_port> disk-failure <dss_name>\n"
          "  %s <manager_ip> <manager_port> decommission-dss <dss_name>\n"
          "  %s <manager_ip> <manager_port> deregister-user <user_name>\n",
          argv[0],argv[0],argv[0],argv[0],argv[0],argv[0],argv[0],argv[0],argv[0]);
        exit(1);
    }
    const char *mgr_ip = argv[1];
    unsigned short mgr_port = (unsigned short)atoi(argv[2]);
    maybe_warn_port_range(mgr_port, "user");

    int sock;
    struct sockaddr_in mgr;
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("user: socket() failed");

    memset(&mgr, 0, sizeof(mgr));
    mgr.sin_family      = AF_INET;
    mgr.sin_addr.s_addr = inet_addr(mgr_ip);
    mgr.sin_port        = htons(mgr_port);

    const char *cmd = argv[3];
    uint32_t req_id = (uint32_t)time(NULL) ^ (uint32_t)getpid();

    uint8_t out[4096];

    if (strcmp(cmd, "register-user") == 0) {
        /* back-compat (5 args) or final (6 args) */
        if (argc != 6 && argc != 7) { fprintf(stderr, "Bad args for register-user\n"); exit(1); }
        msg_hdr_t h; make_hdr(&h, OP_REGISTER_USER, req_id);

        reg_user_req_t p; memset(&p, 0, sizeof(p));
        strncpy(p.user_name, argv[4], MAX_NAME_LEN-1);
        if (argc==6) {
            p.listen_port = (uint16_t)atoi(argv[5]);
        } else {
            p.m_port = (uint16_t)atoi(argv[5]);
            p.c_port = (uint16_t)atoi(argv[6]);
        }

        memcpy(out, &h, sizeof(h)); memcpy(out + sizeof(h), &p, sizeof(p));
        printf("user: tx REGISTER_USER req_id=%u name=%s\n", req_id, argv[4]);
        send_and_recv_print(sock,&mgr,out,sizeof(h)+sizeof(p),"user: send REG_USER failed");
    }
    else if (strcmp(cmd, "configure-dss") == 0) {
        if (argc != 7) { fprintf(stderr, "Bad args for configure-dss\n"); exit(1); }
        msg_hdr_t h; make_hdr(&h, OP_CONFIGURE_DSS, req_id);

        cfg_dss_req_t p; memset(&p, 0, sizeof(p));
        strncpy(p.dss_name, argv[4], 15);
        p.n            = (uint32_t)strtoul(argv[5], NULL, 10);
        p.striping_unit= (uint32_t)strtoul(argv[6], NULL, 10);

        memcpy(out,&h,sizeof(h)); memcpy(out+sizeof(h),&p,sizeof(p));
        printf("user: tx CONFIGURE_DSS req_id=%u dss=%s n=%s striping=%s\n", req_id, argv[4], argv[5], argv[6]);
        send_and_recv_print(sock,&mgr,out,sizeof(h)+sizeof(p),"user: CONFIGURE_DSS failed");
    }
    else if (strcmp(cmd, "ls") == 0) {
        msg_hdr_t h; make_hdr(&h, OP_LS, req_id);
        ls_req_t rq; memset(&rq,0,sizeof(rq));
        memcpy(out,&h,sizeof(h)); memcpy(out+sizeof(h),&rq,sizeof(rq));
        printf("user: tx LS req_id=%u\n", req_id);
        send_and_recv_print(sock,&mgr,out,sizeof(h)+sizeof(rq),"user: LS failed");
    }
    else if (strcmp(cmd, "copy") == 0) {
        if (argc != 7) { fprintf(stderr, "Bad args for copy\n"); exit(1); }
        const char *dss = argv[4]; 
        const char *fname = argv[5];
        const char *owner = argv[6];

        /* Stat file to get size */
        FILE *fp = fopen(fname, "rb");
        if (!fp) { perror("open file"); exit(1); }
        fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
        fclose(fp);

        /* Phase 1: COPY_BEGIN -> expect COPY_PLAN */
        msg_hdr_t h; make_hdr(&h, OP_COPY_BEGIN, req_id);
        copy_begin_t cb; memset(&cb,0,sizeof(cb));
        if (strcmp(dss,"-")!=0) strncpy(cb.dss_name,dss,15);
        strncpy(cb.file_name,fname,MAX_FILENAME-1);
        cb.file_size = (uint32_t)sz;
        strncpy(cb.owner,owner,MAX_NAME_LEN-1);

        memcpy(out,&h,sizeof(h)); memcpy(out+sizeof(h),&cb,sizeof(cb));
        printf("user: tx COPY_BEGIN req_id=%u dss=%s file=%s size=%ld owner=%s\n", req_id, dss, fname, sz, owner);
        send_and_recv_print(sock,&mgr,out,sizeof(h)+sizeof(cb),"user: COPY_BEGIN failed");

        /* (Data to disks would happen here; simulated.) */

        /* Phase 2: COPY_COMPLETE */
        msg_hdr_t hc; make_hdr(&hc, OP_COPY_COMPLETE, req_id);
        copy_complete_t cc; memset(&cc,0,sizeof(cc));
        if (strcmp(dss,"-")!=0) strncpy(cc.dss_name,dss,15); // safe; manager accepts as chosen previously
        strncpy(cc.file_name,fname,MAX_FILENAME-1);
        cc.file_size=(uint32_t)sz;
        strncpy(cc.owner,owner,MAX_NAME_LEN-1);

        memcpy(out,&hc,sizeof(hc)); memcpy(out+sizeof(hc),&cc,sizeof(cc));
        printf("user: tx COPY_COMPLETE req_id=%u file=%s\n", req_id, fname);
        send_and_recv_print(sock,&mgr,out,sizeof(hc)+sizeof(cc),"user: COPY_COMPLETE failed");
    }
    else if (strcmp(cmd, "read") == 0) {
        if (argc != 7) { fprintf(stderr, "Bad args for read\n"); exit(1); }
        const char *dss = argv[4];
        const char *fname = argv[5];
        const char *uname = argv[6];

        /* Phase 1: READ_BEGIN -> expect READ_PLAN (size + layout) */
        msg_hdr_t h; make_hdr(&h, OP_READ_BEGIN, req_id);
        read_begin_t rb; memset(&rb,0,sizeof(rb));
        strncpy(rb.dss_name,dss,15);
        strncpy(rb.file_name,fname,MAX_FILENAME-1);
        strncpy(rb.user_name,uname,MAX_NAME_LEN-1);

        memcpy(out,&h,sizeof(h)); memcpy(out+sizeof(h),&rb,sizeof(rb));
        printf("user: tx READ_BEGIN req_id=%u dss=%s file=%s user=%s\n", req_id, dss, fname, uname);
        send_and_recv_print(sock,&mgr,out,sizeof(h)+sizeof(rb),"user: READ_BEGIN failed");

        /* Phase 2: READ_COMPLETE */
        msg_hdr_t hc; make_hdr(&hc, OP_READ_COMPLETE, req_id);
        read_complete_t rc; memset(&rc,0,sizeof(rc));
        strncpy(rc.dss_name,dss,15);
        strncpy(rc.file_name,fname,MAX_FILENAME-1);
        strncpy(rc.user_name,uname,MAX_NAME_LEN-1);

        memcpy(out,&hc,sizeof(hc)); memcpy(out+sizeof(hc),&rc,sizeof(rc));
        printf("user: tx READ_COMPLETE req_id=%u file=%s\n", req_id, fname);
        send_and_recv_print(sock,&mgr,out,sizeof(hc)+sizeof(rc),"user: READ_COMPLETE failed");
    }
    else if (strcmp(cmd, "disk-failure") == 0) {
        if (argc != 5) { fprintf(stderr, "Bad args for disk-failure\n"); exit(1); }
        const char *dss = argv[4];

        /* FAIL_BEGIN -> expect FAIL_PLAN; then RECOVERY_COMPLETE */
        msg_hdr_t h; make_hdr(&h, OP_FAIL_BEGIN, req_id);
        fail_begin_t fb; memset(&fb,0,sizeof(fb));
        strncpy(fb.dss_name,dss,15);

        memcpy(out,&h,sizeof(h)); memcpy(out+sizeof(h),&fb,sizeof(fb));
        printf("user: tx FAIL_BEGIN req_id=%u dss=%s\n", req_id, dss);
        send_and_recv_print(sock,&mgr,out,sizeof(h)+sizeof(fb),"user: FAIL_BEGIN failed");

        /* simulate recovery */
        msg_hdr_t hc; make_hdr(&hc, OP_RECOVERY_COMPLETE, req_id);
        recovery_complete_t rc; memset(&rc,0,sizeof(rc));
        strncpy(rc.dss_name,dss,15);

        memcpy(out,&hc,sizeof(hc)); memcpy(out+sizeof(hc),&rc,sizeof(rc));
        printf("user: tx RECOVERY_COMPLETE req_id=%u dss=%s\n", req_id, dss);
        send_and_recv_print(sock,&mgr,out,sizeof(hc)+sizeof(rc),"user: RECOVERY_COMPLETE failed");
    }
    else if (strcmp(cmd, "decommission-dss") == 0) {
        if (argc != 5) { fprintf(stderr, "Bad args for decommission-dss\n"); exit(1); }
        const char *dss = argv[4];

        msg_hdr_t h; make_hdr(&h, OP_DECOM_BEGIN, req_id);
        decom_begin_t db; memset(&db,0,sizeof(db));
        strncpy(db.dss_name,dss,15);

        memcpy(out,&h,sizeof(h)); memcpy(out+sizeof(h),&db,sizeof(db));
        printf("user: tx DECOM_BEGIN req_id=%u dss=%s\n", req_id, dss);
        send_and_recv_print(sock,&mgr,out,sizeof(h)+sizeof(db),"user: DECOM_BEGIN failed");

        /* simulate delete operations */
        msg_hdr_t hc; make_hdr(&hc, OP_DECOM_COMPLETE, req_id);
        decom_complete_t dc; memset(&dc,0,sizeof(dc));
        strncpy(dc.dss_name,dss,15);

        memcpy(out,&hc,sizeof(hc)); memcpy(out+sizeof(hc),&dc,sizeof(dc));
        printf("user: tx DECOM_COMPLETE req_id=%u dss=%s\n", req_id, dss);
        send_and_recv_print(sock,&mgr,out,sizeof(hc)+sizeof(dc),"user: DECOM_COMPLETE failed");
    }
    else if (strcmp(cmd, "deregister-user") == 0) {
        if (argc != 5) { fprintf(stderr, "Bad args for deregister-user\n"); exit(1); }
        msg_hdr_t h; make_hdr(&h, OP_DEREGISTER_USER, req_id);

        dereg_user_req_t p; memset(&p, 0, sizeof(p));
        strncpy(p.user_name, argv[4], MAX_NAME_LEN-1);

        memcpy(out, &h, sizeof(h)); memcpy(out + sizeof(h), &p, sizeof(p));
        printf("user: tx DEREGISTER_USER req_id=%u name=%s\n", req_id, argv[4]);
        send_and_recv_print(sock,&mgr,out,sizeof(h)+sizeof(p),"user: send DEREG failed");
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        exit(1);
    }

    close(sock);
    return 0;
}
