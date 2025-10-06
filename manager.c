// manager.c — UDP manager

#include "common.h"
#include "state.h"

static void send_ack(int sock, const struct sockaddr_in *dst, uint32_t req_id,
                     uint8_t status, const char *text)
{
    msg_hdr_t hdr; make_hdr(&hdr, (status == ST_OK) ? OP_ACK : OP_ERR, req_id);
    resp_t r; memset(&r, 0, sizeof(r));
    r.status = status;
    if (text) { strncpy(r.msg, text, MAX_MSG_LEN-1); r.msg[MAX_MSG_LEN-1] = '\0'; }

    uint8_t buf[sizeof(hdr) + sizeof(r)];
    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), &r, sizeof(r));

    if (sendto(sock, buf, sizeof(buf), 0, (const struct sockaddr *)dst, sizeof(*dst)) != (ssize_t)sizeof(buf))
        DieWithError("manager: sendto failed");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <UDP MANAGER PORT>\n", argv[0]);
        exit(1);
    }
    unsigned short mport = (unsigned short)atoi(argv[1]);
    maybe_warn_port_range(mport, "manager");

    int sock;
    struct sockaddr_in maddr, src;
    socklen_t srclen = sizeof(src);

    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("manager: socket() failed");

    memset(&maddr, 0, sizeof(maddr));
    maddr.sin_family      = AF_INET;
    maddr.sin_addr.s_addr = htonl(INADDR_ANY);
    maddr.sin_port        = htons(mport);

    if (bind(sock, (struct sockaddr *)&maddr, sizeof(maddr)) < 0)
        DieWithError("manager: bind() failed");

    printf("manager: listening on UDP port %u\n", mport);

    manager_state_t st; memset(&st, 0, sizeof(st));

    for (;;) {
        uint8_t buf[1024];
        ssize_t r = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&src, &srclen);
        if (r < (ssize_t)sizeof(msg_hdr_t)) { perror("manager: short recv"); continue; }

        msg_hdr_t hdr; memcpy(&hdr, buf, sizeof(hdr));
        uint32_t req_id = ntohl(hdr.req_id);
        const uint8_t *payload = buf + sizeof(hdr);
        size_t paylen = (size_t)r - sizeof(hdr);

        switch (hdr.opcode) {
        case OP_REGISTER_USER: {
            if (paylen < sizeof(reg_user_req_t)) { send_ack(sock, &src, req_id, ST_BAD_PARAMS, "short REGISTER_USER"); break; }
            reg_user_req_t p; memcpy(&p, payload, sizeof(p));
            struct sockaddr_in uaddr = src; /* use src IP, advertised port */
            uaddr.sin_port = p.listen_port; /* already network order */
            int rc = add_user(&st, p.user_name, &uaddr);
            if (rc == -1) send_ack(sock, &src, req_id, ST_ALREADY_REGISTERED, "user exists");
            else if (rc < 0) send_ack(sock, &src, req_id, ST_INTERNAL, "user table full");
            else send_ack(sock, &src, req_id, ST_OK, "user registered");
            break;
        }
        case OP_REGISTER_DISK: {
            if (paylen < sizeof(reg_disk_req_t)) { send_ack(sock, &src, req_id, ST_BAD_PARAMS, "short REGISTER_DISK"); break; }
            reg_disk_req_t p; memcpy(&p, payload, sizeof(p));
            uint32_t cap = ntohl(p.capacity_blocks);
            struct sockaddr_in daddr = src;
            daddr.sin_port = p.listen_port;
            int rc = add_disk(&st, p.disk_name, cap, &daddr);
            if (rc == -1) send_ack(sock, &src, req_id, ST_ALREADY_REGISTERED, "disk exists");
            else if (rc < 0) send_ack(sock, &src, req_id, ST_INTERNAL, "disk table full");
            else send_ack(sock, &src, req_id, ST_OK, "disk registered");
            break;
        }
        case OP_CONFIGURE_DSS: {
            if (paylen < sizeof(cfg_dss_req_t)) { send_ack(sock, &src, req_id, ST_BAD_PARAMS, "short CONFIGURE_DSS"); break; }
            cfg_dss_req_t p; memcpy(&p, payload, sizeof(p));
            uint32_t n   = ntohl(p.n);
            uint32_t blk = ntohl(p.block_size);
            if (n == 0 || blk == 0) { send_ack(sock, &src, req_id, ST_BAD_PARAMS, "n>0 and block>0"); break; }

            int free_disks = count_free_disks(&st);
            if (free_disks < (int)n) { send_ack(sock, &src, req_id, ST_INSUFFICIENT_DISKS, "not enough free disks"); break; }

            /* pick first n free disks and mark in_dss=1 */
            int marked = 0;
            for (int i = 0; i < MAX_DISKS && marked < (int)n; i++) {
                if (st.disks[i].in_use && !st.disks[i].in_dss) {
                    st.disks[i].in_dss = 1;
                    marked++;
                }
            }
            st.dss_n = n;
            st.dss_block_size = blk;

            char msg[128];
            snprintf(msg, sizeof(msg), "configured DSS n=%u, block=%u", n, blk);
            send_ack(sock, &src, req_id, ST_OK, msg);
            break;
        }
        case OP_DEREGISTER_USER: {
            if (paylen < sizeof(dereg_user_req_t)) { send_ack(sock, &src, req_id, ST_BAD_PARAMS, "short DEREGISTER_USER"); break; }
            dereg_user_req_t p; memcpy(&p, payload, sizeof(p));
            int idx = find_user(&st, p.user_name);
            if (idx < 0) send_ack(sock, &src, req_id, ST_NOT_REGISTERED, "user not found");
            else { st.users[idx].in_use = 0; send_ack(sock, &src, req_id, ST_OK, "user deregistered"); }
            break;
        }
        case OP_DEREGISTER_DISK: {
            if (paylen < sizeof(dereg_disk_req_t)) { send_ack(sock, &src, req_id, ST_BAD_PARAMS, "short DEREGISTER_DISK"); break; }
            dereg_disk_req_t p; memcpy(&p, payload, sizeof(p));
            int idx = find_disk(&st, p.disk_name);
            if (idx < 0) { send_ack(sock, &src, req_id, ST_NOT_REGISTERED, "disk not found"); break; }
            if (st.disks[idx].in_dss) { /* cannot remove without decommission */
                send_ack(sock, &src, req_id, ST_BAD_PARAMS, "disk in active DSS");
                break;
            }
            st.disks[idx].in_use = 0;
            send_ack(sock, &src, req_id, ST_OK, "disk deregistered");
            break;
        }
        default:
            send_ack(sock, &src, req_id, ST_BAD_PARAMS, "unknown opcode");
            break;
        }
    }
    return 0;
}

