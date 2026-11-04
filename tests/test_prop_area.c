#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

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
    hdr->bytes_used = 20; /* root_bt occupies bytes 0..19 of data area */
    hdr->serial = 0;
    hdr->magic = PROP_AREA_MAGIC;
    hdr->version = PROP_AREA_VERSION;
    /* root prop_bt at area + 128 is already all zeroes */
}

static uint32_t prop_area_alloc(void *area, uint32_t size) {
    struct prop_area_hdr *hdr = (struct prop_area_hdr *)area;
    uint32_t aligned = (size + 3) & ~3;
    if (hdr->bytes_used + aligned > PA_SIZE - 128) {
        return 0; /* Out of space */
    }
    uint32_t off = hdr->bytes_used;
    hdr->bytes_used += aligned;
    return off;
}

static int prop_area_set(void *area, const char *key, const char *val) {
    struct prop_area_hdr *hdr = (struct prop_area_hdr *)area;
    char *data = (char *)area + 128;
    uint32_t current_bt_off = 0; /* root_bt */

    const char *p = key;
    while (*p) {
        const char *dot = strchr(p, '.');
        uint32_t comp_len = dot ? (uint32_t)(dot - p) : (uint32_t)strlen(p);
        if (comp_len == 0) return -1;

        struct prop_bt *curr = (struct prop_bt *)(data + current_bt_off);
        uint32_t *target_link = NULL;

        if (curr->children == 0) {
            /* No children yet; allocate first child */
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
            /* Walk BST at curr->children */
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
                    /* Match found */
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

    /* current_bt_off is the leaf node */
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
        info->serial |= 1; /* lock */
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
    uint32_t current_bt_off = 0; /* root_bt */

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

int main(void) {
    void *area = malloc(PA_SIZE);
    prop_area_init(area);

    assert(prop_area_get(area, "ro.build.version.sdk") == NULL);

    assert(prop_area_set(area, "ro.build.version.sdk", "28") == 0);
    assert(prop_area_set(area, "hwservicemanager.ready", "false") == 0);
    assert(prop_area_set(area, "ro.hardware.egl", "android") == 0);
    assert(prop_area_set(area, "ro.hardware.gralloc", "default") == 0);
    assert(prop_area_set(area, "service.adb.tcp.port", "5555") == 0);

    const char *v1 = prop_area_get(area, "ro.build.version.sdk");
    assert(v1 != NULL && strcmp(v1, "28") == 0);

    const char *v2 = prop_area_get(area, "hwservicemanager.ready");
    assert(v2 != NULL && strcmp(v2, "false") == 0);

    /* Update existing */
    assert(prop_area_set(area, "hwservicemanager.ready", "true") == 0);
    const char *v3 = prop_area_get(area, "hwservicemanager.ready");
    assert(v3 != NULL && strcmp(v3, "true") == 0);

    printf("[PASS] All prop_area unit tests passed successfully!\n");
    free(area);
    return 0;
}
