// manager.c — UDP manager

#include "common.h"
#include "state.h"
#include "storage.h"

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

static void send_blob(int sock, const struct sockaddr_in *dst, uint32_t req_id,
                      uint8_t opcode, const void *payload, size_t paylen)
{
    msg_hdr_t hdr; make_hdr(&hdr, opcode, req_id);
    uint8_t buf[sizeof(hdr)+1024];
    memcpy(buf, &hdr, sizeof(hdr));
    if (paylen > 1024) paylen = 1024;
    memcpy(buf+sizeof(hdr), payload, paylen);
    if (sendto(sock, buf, sizeof(hdr)+paylen, 0, (const struct sockaddr *)dst, sizeof(*dst)) < 0)
        DieWithError("manager: send_blob sendto failed");
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
    storage_state_t ss; storage_init(&ss);

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

            /* infer owner by matching src socket to registered user */
            int owner_idx = -1;
            for (int i=0; i<MAX_USERS; i++){
                if (st.users[i].in_use &&
                    st.users[i].addr.sin_addr.s_addr == src.sin_addr.s_addr &&
                    st.users[i].addr.sin_port == src.sin_port) { owner_idx = i; break; }
            }
            if (owner_idx<0) { send_ack(sock, &src, req_id, ST_NOT_REGISTERED, "unknown user"); break; }

            int rc = storage_configure_dss(&st, &ss, st.users[owner_idx].name, n, blk);
            if (rc == -2) send_ack(sock, &src, req_id, ST_INSUFFICIENT_DISKS, "not enough free disks");
            else if (rc<0) send_ack(sock, &src, req_id, ST_INTERNAL, "configure failed");
            else {
                char msg[128]; snprintf(msg, sizeof(msg), "configured DSS n=%u, block=%u", n, blk);
                send_ack(sock, &src, req_id, ST_OK, msg);
            }
            break;
        }
        
        case OP_PUT_OPEN: {
            if (paylen < sizeof(put_open_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short PUT_OPEN"); break; }
            put_open_t p; memcpy(&p, payload, sizeof(p));
            /* infer owner from sender */
            int owner_idx = -1;
            for (int i=0; i<MAX_USERS; i++){
                if (st.users[i].in_use &&
                    st.users[i].addr.sin_addr.s_addr == src.sin_addr.s_addr &&
                    st.users[i].addr.sin_port == src.sin_port) { owner_idx = i; break; }
            }
            if (owner_idx<0) { send_ack(sock, &src, req_id, ST_NOT_REGISTERED, "unknown user"); break; }
            int rc = storage_put_open(&ss, st.users[owner_idx].name, p.filename, p.total_size);
            if (rc<0) send_ack(sock,&src,req_id,ST_DSS_NOT_READY,"no DSS");
            else send_ack(sock,&src,req_id,ST_OK,"put open ok");
            break;
        }

        case OP_PUT_CHUNK: {
            if (paylen < sizeof(put_chunk_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short PUT_CHUNK"); break; }
            put_chunk_t pc; memcpy(&pc, payload, sizeof(pc));
            int owner_idx = -1;
            for (int i=0; i<MAX_USERS; i++){
                if (st.users[i].in_use &&
                    st.users[i].addr.sin_addr.s_addr == src.sin_addr.s_addr &&
                    st.users[i].addr.sin_port == src.sin_port) { owner_idx = i; break; }
            }
            if (owner_idx<0) { send_ack(sock, &src, req_id, ST_NOT_REGISTERED, "unknown user"); break; }
            int rc = storage_put_chunk(&ss, st.users[owner_idx].name, pc.filename, pc.seq, pc.data, pc.nbytes);
            if (rc<0) send_ack(sock,&src,req_id,ST_INTERNAL,"put chunk fail");
            else send_ack(sock,&src,req_id,ST_OK,"put chunk ok");
            break;
        }

        case OP_PUT_CLOSE: {
            if (paylen < sizeof(put_close_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short PUT_CLOSE"); break; }
            put_close_t p; memcpy(&p, payload, sizeof(p));
            int owner_idx = -1;
            for (int i=0; i<MAX_USERS; i++){
                if (st.users[i].in_use &&
                    st.users[i].addr.sin_addr.s_addr == src.sin_addr.s_addr &&
                    st.users[i].addr.sin_port == src.sin_port) { owner_idx = i; break; }
            }
            if (owner_idx<0) { send_ack(sock, &src, req_id, ST_NOT_REGISTERED, "unknown user"); break; }
            int rc = storage_put_close(&ss, st.users[owner_idx].name, p.filename);
            if (rc<0) send_ack(sock,&src,req_id,ST_INTERNAL,"put close fail");
            else send_ack(sock,&src,req_id,ST_OK,"put close ok");
            break;
        }

        case OP_LS: {
            if (paylen < sizeof(ls_req_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short LS"); break; }
            ls_req_t rq; memcpy(&rq, payload, sizeof(rq));
            ls_resp_t out; memset(&out, 0, sizeof(out));
            uint32_t nbytes=0;
            int rc = storage_ls(&ss, rq.owner, out.data, &nbytes);
            if (rc<0) { send_ack(sock,&src,req_id,ST_DSS_NOT_READY,"no DSS"); break; }
            out.nbytes = nbytes;
            send_blob(sock,&src,req_id,OP_LS,&out,sizeof(out));
            break;
        }

        case OP_READ_OPEN: {
            if (paylen < sizeof(read_open_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short READ_OPEN"); break; }
            read_open_t rq; memcpy(&rq, payload, sizeof(rq));
            uint32_t total_chunks=0;
            int rc = storage_read_open(&ss, rq.owner, rq.filename, &total_chunks);
            if (rc==-2) send_ack(sock,&src,req_id,ST_INTERNAL,"all replicas failed");
            else if (rc<0) send_ack(sock,&src,req_id,ST_NOT_FOUND,"file not found");
            else { char msg[64]; snprintf(msg,sizeof(msg),"chunks=%u", total_chunks); send_ack(sock,&src,req_id,ST_OK,msg); }
            break;
        }

        case OP_READ_CHUNK: {
            if (paylen < sizeof(read_chunk_req_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short READ_CHUNK"); break; }
            read_chunk_req_t rq; memcpy(&rq, payload, sizeof(rq));
            /* infer owner from sender */
            int owner_idx=-1;
            for (int i=0; i<MAX_USERS; i++){
                if (st.users[i].in_use &&
                    st.users[i].addr.sin_addr.s_addr == src.sin_addr.s_addr &&
                    st.users[i].addr.sin_port == src.sin_port) { owner_idx = i; break; }
            }
            if (owner_idx<0) { send_ack(sock, &src, req_id, ST_NOT_REGISTERED, "unknown user"); break; }
            uint8_t bufout[MAX_CHUNK]; uint32_t nb=0;
            int rc = storage_read_chunk(&ss, st.users[owner_idx].name, rq.filename, rq.seq, bufout, &nb);
            if (rc<0) send_ack(sock,&src,req_id,ST_NOT_FOUND,"read chunk err");
            else {
                /* reuse put_chunk_t as generic data carrier */
                put_chunk_t resp; memset(&resp,0,sizeof(resp));
                strncpy(resp.filename, rq.filename, MAX_FILENAME-1);
                resp.seq = rq.seq;
                resp.nbytes = nb;
                memcpy(resp.data, bufout, nb);
                send_blob(sock,&src,req_id,OP_READ_CHUNK,&resp,sizeof(resp));
            }
            break;
        }

        case OP_READ_CLOSE: {
            if (paylen < sizeof(read_close_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short READ_CLOSE"); break; }
            read_close_t rq; memcpy(&rq, payload, sizeof(rq));
            int owner_idx=-1;
            for (int i=0; i<MAX_USERS; i++){
                if (st.users[i].in_use &&
                    st.users[i].addr.sin_addr.s_addr == src.sin_addr.s_addr &&
                    st.users[i].addr.sin_port == src.sin_port) { owner_idx = i; break; }
            }
            if (owner_idx<0) { send_ack(sock, &src, req_id, ST_NOT_REGISTERED, "unknown user"); break; }
            int rc = storage_read_close(&ss, st.users[owner_idx].name, rq.filename);
            if (rc<0) send_ack(sock,&src,req_id,ST_INTERNAL,"read close fail");
            else send_ack(sock,&src,req_id,ST_OK,"read close ok");
            break;
        }

        case OP_FAIL_DISK: {
            if (paylen < sizeof(dereg_disk_req_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short FAIL_DISK"); break; }
            dereg_disk_req_t rq; memcpy(&rq, payload, sizeof(rq));
            int owner_idx = -1;
            for (int i=0; i<MAX_USERS; i++){
                if (st.users[i].in_use &&
                    st.users[i].addr.sin_addr.s_addr == src.sin_addr.s_addr &&
                    st.users[i].addr.sin_port == src.sin_port) { owner_idx = i; break; }
            }
            if (owner_idx<0) { send_ack(sock, &src, req_id, ST_NOT_REGISTERED, "unknown user"); break; }
            int rc = storage_fail_disk(&st, &ss, st.users[owner_idx].name, rq.disk_name);
            if (rc<0) send_ack(sock,&src,req_id,ST_BAD_PARAMS,"disk not in DSS");
            else send_ack(sock,&src,req_id,ST_OK,"disk failed");
            break;
        }

        case OP_DECOMMISSION_DSS: {
            int owner_idx = -1;
            for (int i=0; i<MAX_USERS; i++){
                if (st.users[i].in_use &&
                    st.users[i].addr.sin_addr.s_addr == src.sin_addr.s_addr &&
                    st.users[i].addr.sin_port == src.sin_port) { owner_idx = i; break; }
            }
            if (owner_idx<0) { send_ack(sock, &src, req_id, ST_NOT_REGISTERED, "unknown user"); break; }
            int rc = storage_decommission_dss(&st, &ss, st.users[owner_idx].name);
            if (rc<0) send_ack(sock,&src,req_id,ST_INTERNAL,"no DSS");
            else send_ack(sock,&src,req_id,ST_OK,"dss decommissioned");
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


