#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <fcntl.h>

#define PROP_AREA_MAGIC   0x504f5250
#define PROP_AREA_VERSION 0xfc6ed0ab
#define PA_SIZE           131072

struct prop_area_hdr {
    uint32_t bytes_used;
    uint32_t serial;
    uint32_t magic;
    uint32_t version;
    uint32_t reserved[28];
};

struct prop_bt {
    uint32_t namelen;
    uint32_t prop;
    uint32_t left;
    uint32_t right;
    uint32_t children;
    char name[0];
};

struct prop_info {
    uint32_t serial;
    char value[92];
    char name[0];
};

static void prop_area_init(void *area) {
    memset(area, 0, PA_SIZE);
    struct prop_area_hdr *hdr = (struct prop_area_hdr *)area;
    hdr->bytes_used = 20;
    hdr->serial = 0;
    hdr->magic = PROP_AREA_MAGIC;
    hdr->version = PROP_AREA_VERSION;
}

static uint32_t prop_area_alloc(void *area, uint32_t size) {
    struct prop_area_hdr *hdr = (struct prop_area_hdr *)area;
    uint32_t aligned = (size + 3) & ~3;
    if (hdr->bytes_used + aligned > PA_SIZE - 128) {
        return 0;
    }
    uint32_t off = hdr->bytes_used;
    hdr->bytes_used += aligned;
    return off;
}

static int prop_area_set(void *area, const char *key, const char *val) {
    struct prop_area_hdr *hdr = (struct prop_area_hdr *)area;
    char *data = (char *)area + 128;
    uint32_t current_bt_off = 0;

    const char *p = key;
    while (*p) {
        const char *dot = strchr(p, '.');
        uint32_t comp_len = dot ? (uint32_t)(dot - p) : (uint32_t)strlen(p);
        if (comp_len == 0) return -1;

        struct prop_bt *curr = (struct prop_bt *)(data + current_bt_off);
        uint32_t *target_link = NULL;

        if (curr->children == 0) {
            uint32_t node_sz = sizeof(struct prop_bt) + comp_len + 1;
            uint32_t new_off = prop_area_alloc(area, node_sz);
            if (!new_off) return -1;
            struct prop_bt *new_node = (struct prop_bt *)(data + new_off);
            memset(new_node, 0, (node_sz + 3) & ~3);
            new_node->namelen = comp_len;
            memcpy(new_node->name, p, comp_len);
            new_node->name[comp_len] = '\0';
            curr->children = new_off;
            current_bt_off = new_off;
        } else {
            uint32_t child_off = curr->children;
            while (1) {
                struct prop_bt *child = (struct prop_bt *)(data + child_off);
                int cmp = 0;
                if (comp_len < child->namelen) {
                    cmp = -1;
                } else if (comp_len > child->namelen) {
                    cmp = 1;
                } else {
                    cmp = strncmp(p, child->name, comp_len);
                }

                if (cmp < 0) {
                    if (child->left == 0) {
                        target_link = &child->left;
                        break;
                    }
                    child_off = child->left;
                } else if (cmp > 0) {
                    if (child->right == 0) {
                        target_link = &child->right;
                        break;
                    }
                    child_off = child->right;
                } else {
                    current_bt_off = child_off;
                    target_link = NULL;
                    break;
                }
            }

            if (target_link) {
                uint32_t node_sz = sizeof(struct prop_bt) + comp_len + 1;
                uint32_t new_off = prop_area_alloc(area, node_sz);
                if (!new_off) return -1;
                struct prop_bt *new_node = (struct prop_bt *)(data + new_off);
                memset(new_node, 0, (node_sz + 3) & ~3);
                new_node->namelen = comp_len;
                memcpy(new_node->name, p, comp_len);
                new_node->name[comp_len] = '\0';
                *target_link = new_off;
                current_bt_off = new_off;
            }
        }

        if (dot) {
            p = dot + 1;
        } else {
            break;
        }
    }

    struct prop_bt *leaf = (struct prop_bt *)(data + current_bt_off);
    uint32_t vallen = (uint32_t)strlen(val);
    if (vallen >= 92) vallen = 91;

    if (leaf->prop == 0) {
        uint32_t keylen = (uint32_t)strlen(key);
        uint32_t info_sz = 96 + keylen + 1;
        uint32_t info_off = prop_area_alloc(area, info_sz);
        if (!info_off) return -1;
        struct prop_info *info = (struct prop_info *)(data + info_off);
        memset(info, 0, (info_sz + 3) & ~3);
        info->serial = (vallen << 24);
        memcpy(info->value, val, vallen);
        info->value[vallen] = '\0';
        memcpy(info->name, key, keylen);
        info->name[keylen] = '\0';
        leaf->prop = info_off;
    } else {
        struct prop_info *info = (struct prop_info *)(data + leaf->prop);
        uint32_t old_serial = info->serial;
        info->serial |= 1;
        __sync_synchronize();
        memcpy(info->value, val, vallen);
        info->value[vallen] = '\0';
        __sync_synchronize();
        info->serial = (vallen << 24) | ((old_serial + 2) & 0x00ffffff & ~1);
    }

    hdr->serial++;
    return 0;
}

static const char *prop_area_get(void *area, const char *key) {
    char *data = (char *)area + 128;
    uint32_t current_bt_off = 0;

    const char *p = key;
    while (*p) {
        const char *dot = strchr(p, '.');
        uint32_t comp_len = dot ? (uint32_t)(dot - p) : (uint32_t)strlen(p);
        if (comp_len == 0) return NULL;

        struct prop_bt *curr = (struct prop_bt *)(data + current_bt_off);
        if (curr->children == 0) return NULL;

        uint32_t child_off = curr->children;
        int found = 0;
        while (child_off != 0) {
            struct prop_bt *child = (struct prop_bt *)(data + child_off);
            int cmp = 0;
            if (comp_len < child->namelen) {
                cmp = -1;
            } else if (comp_len > child->namelen) {
                cmp = 1;
            } else {
                cmp = strncmp(p, child->name, comp_len);
            }

            if (cmp < 0) {
                child_off = child->left;
            } else if (cmp > 0) {
                child_off = child->right;
            } else {
                current_bt_off = child_off;
                found = 1;
                break;
            }
        }

        if (!found) return NULL;

        if (dot) {
            p = dot + 1;
        } else {
            break;
        }
    }

    struct prop_bt *leaf = (struct prop_bt *)(data + current_bt_off);
    if (leaf->prop == 0) return NULL;
    struct prop_info *info = (struct prop_info *)(data + leaf->prop);
    return info->value;
}

static ssize_t my_read_all(int fd, void *buf, size_t count) {
    uint8_t *p = (uint8_t *)buf;
    size_t total = 0;
    while (total < count) {
        ssize_t r = read(fd, p + total, count - total);
        if (r < 0) return r;
        if (r == 0) break;
        total += r;
    }
    return total;
}

int main(void) {
    void *area = mmap(NULL, PA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    assert(area != MAP_FAILED);
    prop_area_init(area);

    const char *sock_path = "/tmp/test_prop_service.sock";
    unlink(sock_path);

    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    assert(sfd >= 0);

    struct sockaddr_un sun;
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path, sock_path, sizeof(sun.sun_path) - 1);

    int brc = bind(sfd, (struct sockaddr *)&sun, sizeof(sun));
    assert(brc == 0);

    int lrc = listen(sfd, 8);
    assert(lrc == 0);

    pid_t pid = fork();
    assert(pid >= 0);

    if (pid == 0) {
        /* Server process */
        while (1) {
            int cfd = accept(sfd, NULL, NULL);
            if (cfd < 0) continue;

            uint32_t cmd = 0;
            if (my_read_all(cfd, &cmd, 4) == 4) {
                if (cmd == 0x00020001) { /* PROP_MSG_SETPROP2 (writev, version 2) */
                    uint32_t namelen = 0;
                    if (my_read_all(cfd, &namelen, 4) == 4 && namelen > 0 && namelen < 512) {
                        char name[512];
                        if (my_read_all(cfd, name, namelen) == (ssize_t)namelen) {
                            name[namelen] = '\0';
                            uint32_t vallen = 0;
                            char val[512];
                            val[0] = '\0';
                            if (my_read_all(cfd, &vallen, 4) == 4 && vallen < 512) {
                                if (vallen > 0) {
                                    my_read_all(cfd, val, vallen);
                                }
                                val[vallen] = '\0';
                                int set_rc = prop_area_set(area, name, val);
                                uint32_t resp = (set_rc == 0) ? 0 : 1;
                                write(cfd, &resp, 4);
                            }
                        }
                    }
                } else if (cmd == 1) { /* PROP_MSG_SETPROP */
                    char buf[124];
                    if (my_read_all(cfd, buf, 124) == 124) {
                        char name[33];
                        char val[93];
                        memcpy(name, buf, 32);
                        name[32] = '\0';
                        memcpy(val, buf + 32, 92);
                        val[92] = '\0';
                        int set_rc = prop_area_set(area, name, val);
                        uint32_t resp = (set_rc == 0) ? 0 : 1;
                        write(cfd, &resp, 4);
                    }
                }
            }
            close(cfd);
        }
        exit(0);
    }

    close(sfd);
    usleep(50000); /* Wait for server loop */

    /* Client Test 1: PROP_MSG_SETPROP2 via writev */
    {
        int cfd = socket(AF_UNIX, SOCK_STREAM, 0);
        assert(cfd >= 0);
        assert(connect(cfd, (struct sockaddr *)&sun, sizeof(sun)) == 0);

        uint32_t cmd = 0x00020001;
        const char *name = "ro.property_service.version";
        const char *val = "2";
        uint32_t namelen = strlen(name);
        uint32_t vallen = strlen(val);

        struct iovec iov[5];
        iov[0].iov_base = &cmd;     iov[0].iov_len = 4;
        iov[1].iov_base = &namelen; iov[1].iov_len = 4;
        iov[2].iov_base = (void*)name; iov[2].iov_len = namelen;
        iov[3].iov_base = &vallen;  iov[3].iov_len = 4;
        iov[4].iov_base = (void*)val;  iov[4].iov_len = vallen;

        ssize_t w = writev(cfd, iov, 5);
        assert(w == (ssize_t)(4 + 4 + namelen + 4 + vallen));

        uint32_t resp = 0xFFFFFFFF;
        ssize_t r = read(cfd, &resp, 4);
        assert(r == 4 && resp == 0);
        close(cfd);
    }

    /* Client Test 2: Dynamic property set via writev */
    {
        int cfd = socket(AF_UNIX, SOCK_STREAM, 0);
        assert(cfd >= 0);
        assert(connect(cfd, (struct sockaddr *)&sun, sizeof(sun)) == 0);

        uint32_t cmd = 0x00020001;
        const char *name = "hwservicemanager.ready";
        const char *val = "true";
        uint32_t namelen = strlen(name);
        uint32_t vallen = strlen(val);

        struct iovec iov[5];
        iov[0].iov_base = &cmd;     iov[0].iov_len = 4;
        iov[1].iov_base = &namelen; iov[1].iov_len = 4;
        iov[2].iov_base = (void*)name; iov[2].iov_len = namelen;
        iov[3].iov_base = &vallen;  iov[3].iov_len = 4;
        iov[4].iov_base = (void*)val;  iov[4].iov_len = vallen;

        writev(cfd, iov, 5);

        uint32_t resp = 0xFFFFFFFF;
        read(cfd, &resp, 4);
        assert(resp == 0);
        close(cfd);
    }

    /* Kill server */
    kill(pid, SIGTERM);
    unlink(sock_path);

    /* Verify area memory directly */
    const char *v1 = prop_area_get(area, "ro.property_service.version");
    assert(v1 != NULL && strcmp(v1, "2") == 0);

    const char *v2 = prop_area_get(area, "hwservicemanager.ready");
    assert(v2 != NULL && strcmp(v2, "true") == 0);

    printf("[PASS] Property service writev (version 2) & legacy socket tests passed!\n");
    munmap(area, PA_SIZE);
    return 0;
}
