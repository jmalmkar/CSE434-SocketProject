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
    uint8_t buf[sizeof(hdr)+2048];
    memcpy(buf, &hdr, sizeof(hdr));
    if (paylen > 2048) paylen = 2048;
    memcpy(buf+sizeof(hdr), payload, paylen);
    if (sendto(sock, buf, sizeof(hdr)+paylen, 0, (const struct sockaddr *)dst, sizeof(*dst)) < 0)
        DieWithError("manager: send_blob sendto failed");
}

static int find_user_by_src(manager_state_t *st, const struct sockaddr_in *src){
    for (int i=0;i<MAX_USERS;i++){
        if (!st->users[i].in_use) continue;
        if (st->users[i].addr.sin_addr.s_addr == src->sin_addr.s_addr &&
            st->users[i].addr.sin_port == src->sin_port) return i;
    }
    return -1;
}

static int any_dss_configured(manager_state_t *st){
    for (int i=0;i<MAX_DSS;i++) if (st->dsses[i].in_use) return 1;
    return 0;
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
    storage_seed_random();

    for (;;) {
        uint8_t buf[4096];
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
            struct sockaddr_in uaddr = src;
            if (p.m_port) uaddr.sin_port = htons(p.m_port);
            uint32_t ip_be = p.ipv4_be ? p.ipv4_be : src.sin_addr.s_addr;
            uint16_t m_port = p.m_port ? p.m_port : ntohs(src.sin_port);
            uint16_t c_port = p.c_port ? p.c_port : (p.listen_port ? p.listen_port : ntohs(src.sin_port));

            int rc = add_user(&st, p.user_name, &uaddr, ip_be, m_port, c_port);
            if (rc == -1) send_ack(sock, &src, req_id, ST_ALREADY_REGISTERED, "user exists");
            else if (rc == -2) send_ack(sock, &src, req_id, ST_BAD_PARAMS, "port pair in use");
            else if (rc < 0) send_ack(sock, &src, req_id, ST_INTERNAL, "user table full");
            else send_ack(sock, &src, req_id, ST_OK, "user registered");
            break;
        }

        case OP_REGISTER_DISK: {
            if (paylen < sizeof(reg_disk_req_t)) { send_ack(sock, &src, req_id, ST_BAD_PARAMS, "short REGISTER_DISK"); break; }
            reg_disk_req_t p; memcpy(&p, payload, sizeof(p));
            struct sockaddr_in daddr = src;
            if (p.m_port) daddr.sin_port = htons(p.m_port);
            uint32_t ip_be = p.ipv4_be ? p.ipv4_be : src.sin_addr.s_addr;
            uint16_t m_port = p.m_port ? p.m_port : ntohs(src.sin_port);
            uint16_t c_port = p.c_port ? p.c_port : (p.listen_port ? p.listen_port : ntohs(src.sin_port));
            uint32_t cap = p.capacity_blocks;

            int rc = add_disk(&st, p.disk_name, cap, &daddr, ip_be, m_port, c_port);
            if (rc == -1) send_ack(sock, &src, req_id, ST_ALREADY_REGISTERED, "disk exists");
            else if (rc == -2) send_ack(sock, &src, req_id, ST_BAD_PARAMS, "port pair in use");
            else if (rc < 0) send_ack(sock, &src, req_id, ST_INTERNAL, "disk table full");
            else send_ack(sock, &src, req_id, ST_OK, "disk registered");
            break;
        }

        case OP_CONFIGURE_DSS: {
            if (paylen < sizeof(cfg_dss_req_t)) { send_ack(sock, &src, req_id, ST_BAD_PARAMS, "short CONFIGURE_DSS"); break; }
            cfg_dss_req_t p; memcpy(&p, payload, sizeof(p));
            uint32_t n   = p.n;
            uint32_t su  = p.striping_unit;
            if (n < 3) { send_ack(sock, &src, req_id, ST_BAD_PARAMS, "n>=3"); break; }
            if (!(is_power_of_two(su) && su>=128 && su<=1024*1024)) {
                send_ack(sock, &src, req_id, ST_BAD_PARAMS, "striping unit pow2 [128..1MB]");
                break;
            }
            if (find_dss(&st, p.dss_name) >= 0) { send_ack(sock, &src, req_id, ST_BAD_PARAMS, "dss exists"); break; }
            if (count_free_disks(&st) < (int)n) { send_ack(sock, &src, req_id, ST_INSUFFICIENT_DISKS, "not enough free disks"); break; }

            int di = add_dss(&st, p.dss_name, n, su);
            if (di < 0) { send_ack(sock, &src, req_id, ST_INTERNAL, "dss table full"); break; }

            int chosen[64]; if (storage_pick_random_free_disks(&st, n, chosen) < 0) {
                send_ack(sock,&src,req_id,ST_INSUFFICIENT_DISKS,"not enough free"); st.dsses[di].in_use=0; break;
            }
            for (uint32_t k=0;k<n;k++){
                st.dsses[di].disk_idx[k]=chosen[k];
                st.disks[chosen[k]].state=DISK_IN_DSS;
            }
            char msg[128];
            snprintf(msg, sizeof(msg), "configured DSS %s n=%u striping=%u", p.dss_name, n, su);
            send_ack(sock, &src, req_id, ST_OK, msg);
            break;
        }

        case OP_LS: {
            if (!any_dss_configured(&st)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"no DSS"); break; }
            ls_resp_t out; memset(&out,0,sizeof(out));
            int w=0;
            /* compose summary */
            for (int d=0; d<MAX_DSS; d++) if (st.dsses[d].in_use){
                int n = snprintf((char*)out.data+w, sizeof(out.data)-w,
                    "%s: Disk array with n=%u (", st.dsses[d].name, st.dsses[d].n);
                if (n<0) break; w+=n;
                for (uint32_t k=0;k<st.dsses[d].n;k++){
                    int di = st.dsses[d].disk_idx[k];
                    n = snprintf((char*)out.data+w, sizeof(out.data)-w, "%s%s",
                        st.disks[di].name, (k+1<st.dsses[d].n?", ":""));
                    if (n<0) break; w+=n;
                }
                n = snprintf((char*)out.data+w, sizeof(out.data)-w, ") with striping-unit %u B.\n", st.dsses[d].striping_unit);
                if (n<0) break; w+=n;

                /* files in this DSS */
                for (int f=0; f<MAX_FILES; f++) if (st.files[f].in_use &&
                    strncmp(st.files[f].dss_name, st.dsses[d].name,16)==0){
                    n = snprintf((char*)out.data+w, sizeof(out.data)-w, "%s %u B %s\n",
                        st.files[f].file_name, st.files[f].file_size, st.files[f].owner);
                    if (n<0) break; w+=n;
                }
            }
            out.nbytes = (uint32_t)w;
            send_blob(sock,&src,req_id,OP_LS,&out,sizeof(out));
            break;
        }

        /* COPY */

        case OP_COPY_BEGIN: {
            if (paylen < sizeof(copy_begin_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short COPY_BEGIN"); break; }
            copy_begin_t cb; memcpy(&cb, payload, sizeof(cb));
            if (!any_dss_configured(&st)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"no DSS"); break; }

            /* pick DSS: if provided dss_name exists use it; else the first DSS */
            int dss_idx = (cb.dss_name[0]? find_dss(&st,cb.dss_name) : -1);
            if (dss_idx<0){
                for (int i=0;i<MAX_DSS;i++) if (st.dsses[i].in_use){ dss_idx=i; break; }
            }
            if (dss_idx<0) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"no DSS"); break; }
            if (st.dsses[dss_idx].critical) { send_ack(sock,&src,req_id,ST_BUSY,"DSS busy"); break; }

            plan_dss_t plan; storage_build_plan(&st, dss_idx, &plan);
            send_blob(sock,&src,req_id,OP_COPY_PLAN,&plan,sizeof(plan));
            /* Manager waits for COPY_COMPLETE before recording file, enforced by client flow. */
            break;
        }

        case OP_COPY_COMPLETE: {
            if (paylen < sizeof(copy_complete_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short COPY_COMPLETE"); break; }
            copy_complete_t cc; memcpy(&cc, payload, sizeof(cc));
            int dss_idx = find_dss(&st, cc.dss_name);
            if (dss_idx<0) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"unknown DSS"); break; }
            if (st.dsses[dss_idx].critical) { send_ack(sock,&src,req_id,ST_BUSY,"DSS busy"); break; }

            /* Enforce owner tag present, insert directory entry */
            if (!add_file(&st, cc.dss_name, cc.file_name, cc.file_size, cc.owner)) {
                send_ack(sock,&src,req_id,ST_INTERNAL,"file table full"); break;
            }
            send_ack(sock,&src,req_id,ST_OK,"copy complete");
            break;
        }

        /* READ */

        case OP_READ_BEGIN: {
            if (paylen < sizeof(read_begin_t)) {
                send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short READ_BEGIN");
                break;
            }
            read_begin_t rb;
            memcpy(&rb, payload, sizeof(rb));
            int dss_idx = find_dss(&st, rb.dss_name);
            if (dss_idx<0) {
                send_ack(sock,&src,req_id,ST_BAD_PARAMS,"unknown DSS");
                break;
            }
            if (st.dsses[dss_idx].critical) {
                send_ack(sock,&src,req_id,ST_BUSY,"DSS busy");
                break;
            }
        
            file_rec_t *fr = find_file_in_dss(&st, rb.dss_name, rb.file_name);
            if (!fr) {
                send_ack(sock,&src,req_id,ST_NOT_FOUND,"file not found");
                break;
            }
        
            // Ownership Validation
            if (strncmp(fr->owner, rb.user_name, MAX_NAME_LEN) != 0) {
                send_ack(sock,&src,req_id,ST_NOT_OWNER,"not owner");
                break;
            }
        
            // If authorized, continue building read plan
            read_plan_t rp;
            memset(&rp,0,sizeof(rp));
            snprintf(rp.dss_name,16,"%s",rb.dss_name);
            snprintf(rp.file_name,MAX_FILENAME,"%s",rb.file_name);
            rp.file_size = fr->file_size;
            storage_build_plan(&st,dss_idx,&rp.plan);
            send_blob(sock,&src,req_id,OP_READ_PLAN,&rp,sizeof(rp));
        
            if (!add_read_session(&st, rb.user_name, rb.dss_name, rb.file_name)) {
                // error handling if needed
            }
            break;
        }

        case OP_READ_COMPLETE: {
            if (paylen < sizeof(read_complete_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short READ_COMPLETE"); break; }
            read_complete_t rc; memcpy(&rc, payload, sizeof(rc));
            del_read_session(&st, rc.user_name, rc.dss_name, rc.file_name);
            send_ack(sock,&src,req_id,ST_OK,"read complete");
            break;
        }

        /* FAIL */

        case OP_FAIL_BEGIN: {
            if (paylen < sizeof(fail_begin_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short FAIL_BEGIN"); break; }
            fail_begin_t fb; memcpy(&fb, payload, sizeof(fb));
            int dss_idx = find_dss(&st, fb.dss_name);
            if (dss_idx<0) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"unknown DSS"); break; }
            if (reads_in_progress_for_dss(&st, fb.dss_name)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"reads in progress"); break; }
            if (st.dsses[dss_idx].critical) { send_ack(sock,&src,req_id,ST_BUSY,"DSS busy"); break; }
            st.dsses[dss_idx].critical = 1; /* enter critical section */

            plan_dss_t plan; storage_build_plan(&st,dss_idx,&plan);
            send_blob(sock,&src,req_id,OP_FAIL_PLAN,&plan,sizeof(plan));
            break;
        }

        case OP_RECOVERY_COMPLETE: {
            if (paylen < sizeof(recovery_complete_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short RECOVERY_COMPLETE"); break; }
            recovery_complete_t rc; memcpy(&rc, payload, sizeof(rc));
            int dss_idx = find_dss(&st, rc.dss_name);
            if (dss_idx<0) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"unknown DSS"); break; }
            st.dsses[dss_idx].critical = 0; /* leave critical */
            send_ack(sock,&src,req_id,ST_OK,"recovery complete");
            break;
        }

        /* DECOMMISSION */

        case OP_DECOM_BEGIN: {
            if (paylen < sizeof(decom_begin_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short DECOM_BEGIN"); break; }
            decom_begin_t db; memcpy(&db, payload, sizeof(db));
            int dss_idx = find_dss(&st, db.dss_name);
            if (dss_idx<0) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"unknown DSS"); break; }
            if (st.dsses[dss_idx].critical) { send_ack(sock,&src,req_id,ST_BUSY,"DSS busy"); break; }
            st.dsses[dss_idx].critical = 1;

            plan_dss_t plan; storage_build_plan(&st,dss_idx,&plan);
            send_blob(sock,&src,req_id,OP_DECOM_PLAN,&plan,sizeof(plan));
            break;
        }

        case OP_DECOM_COMPLETE: {
            if (paylen < sizeof(decom_complete_t)) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"short DECOM_COMPLETE"); break; }
            decom_complete_t dc; memcpy(&dc, payload, sizeof(dc));
            int dss_idx = find_dss(&st, dc.dss_name);
            if (dss_idx<0) { send_ack(sock,&src,req_id,ST_BAD_PARAMS,"unknown DSS"); break; }

            /* delete files on that DSS */
            for (int i=0;i<MAX_FILES;i++) if (st.files[i].in_use &&
                strncmp(st.files[i].dss_name, st.dsses[dss_idx].name,16)==0){
                st.files[i].in_use=0;
            }
            /* free disks */
            for (uint32_t k=0;k<st.dsses[dss_idx].n;k++){
                int di = st.dsses[dss_idx].disk_idx[k];
                st.disks[di].state = DISK_FREE;
            }
            st.dsses[dss_idx].in_use=0;
            st.dsses[dss_idx].critical=0;

            send_ack(sock,&src,req_id,ST_OK,"decommission complete");
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
            if (st.disks[idx].state==DISK_IN_DSS) { send_ack(sock, &src, req_id, ST_BAD_PARAMS, "disk in active DSS"); break; }
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

