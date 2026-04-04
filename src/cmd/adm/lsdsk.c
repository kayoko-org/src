/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#define _NETBSD_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/statvfs.h>
#include <prop/proplib.h>
#include <util.h>

typedef struct {
    char name[64];
    char fstype[16];
    char label[64];
    char mount[MAXPATHLEN];
    uint64_t size;
    bool is_child;
    bool is_last;
} disk_info_t;

int max_name = 4, max_type = 6, max_label = 5, max_mnt = 10;

const char* get_mountpoint(const char *devname) {
    struct statvfs *mntbuf;
    int i, mntsize = getmntinfo(&mntbuf, ST_NOWAIT);
    for (i = 0; i < mntsize; i++) {
        if (strstr(mntbuf[i].f_mntfromname, devname))
            return mntbuf[i].f_mntonname;
    }
    return "";
}

void update_max(disk_info_t *d) {
    int nl = strlen(d->name), tl = strlen(d->fstype);
    int ll = strlen(d->label), ml = strlen(d->mount);
    if (nl > max_name) max_name = nl;
    if (tl > max_type) max_type = tl;
    if (ll > max_label) max_label = ll;
    if (ml > max_mnt) max_mnt = ml;
}

void output_xml(disk_info_t *list, int count) {
    prop_array_t array = prop_array_create();
    for (int i = 0; i < count; i++) {
        prop_dictionary_t dict = prop_dictionary_create();
        prop_dictionary_set_string(dict, "name", list[i].name);
        prop_dictionary_set_string(dict, "fstype", list[i].fstype);
        prop_dictionary_set_string(dict, "label", list[i].label);
        prop_dictionary_set_string(dict, "mountpoint", list[i].mount);
        prop_dictionary_set_uint64(dict, "size_bytes", list[i].size);
        prop_array_add(array, dict);
        prop_object_release(dict);
    }

    char *xml_out = prop_array_externalize(array);
    if (xml_out) {
        printf("%s\n", xml_out);
        free(xml_out);
    }
    prop_object_release(array);
}

void scan_and_print(bool xml_mode) {
    char *disks, *dptr;
    size_t len;
    int mib[2] = { CTL_HW, HW_DISKNAMES };
    disk_info_t list[256];
    int count = 0;

    if (sysctl(mib, 2, NULL, &len, NULL, 0) == -1) return;
    disks = malloc(len);
    sysctl(mib, 2, disks, &len, NULL, 0);

    char *token = strtok_r(disks, " ", &dptr);
    while (token != NULL && count < 256) {
        if (strncmp(token, "dk", 2) != 0) {
            disk_info_t *p = &list[count++];
            memset(p, 0, sizeof(*p));
            strncpy(p->name, token, 63);
            strcpy(p->fstype, "disk");
            
            char path[MAXPATHLEN];
            struct disklabel lp;
            snprintf(path, sizeof(path), "/dev/r%s", token);
            int fd = open(path, O_RDONLY);
            if (fd != -1) {
                if (ioctl(fd, DIOCGDINFO, &lp) == 0) {
                    p->size = (uint64_t)lp.d_secperunit * lp.d_secsize;
                    strncpy(p->label, lp.d_packname, 63);
                }
                close(fd);
            }
            strncpy(p->mount, get_mountpoint(token), MAXPATHLEN-1);
            update_max(p);

            char *inner, *iptr;
            size_t ilen;
            sysctl(mib, 2, NULL, &ilen, NULL, 0);
            inner = malloc(ilen);
            sysctl(mib, 2, inner, &ilen, NULL, 0);
            
            int c_total = 0, c_curr = 0;
            char *itmp = strdup(inner);
            char *sub = strtok_r(itmp, " ", &iptr);
            while(sub) {
                if (strncmp(sub, "dk", 2) == 0) {
                    struct dkwedge_info dkw;
                    char spath[MAXPATHLEN];
                    snprintf(spath, sizeof(spath), "/dev/r%s", sub);
                    int sfd = open(spath, O_RDONLY);
                    if (sfd != -1) {
                        if (ioctl(sfd, DIOCGWEDGEINFO, &dkw) == 0 && strcmp((char*)dkw.dkw_parent, token) == 0) c_total++;
                        close(sfd);
                    }
                }
                sub = strtok_r(NULL, " ", &iptr);
            }
            free(itmp);

            sub = strtok_r(inner, " ", &iptr);
            while(sub && count < 256) {
                if (strncmp(sub, "dk", 2) == 0) {
                    struct dkwedge_info dkw;
                    char spath[MAXPATHLEN];
                    snprintf(spath, sizeof(spath), "/dev/r%s", sub);
                    int sfd = open(spath, O_RDONLY);
                    if (sfd != -1) {
                        if (ioctl(sfd, DIOCGWEDGEINFO, &dkw) == 0 && strcmp((char*)dkw.dkw_parent, token) == 0) {
                            c_curr++;
                            disk_info_t *c = &list[count++];
                            memset(c, 0, sizeof(*c));
                            c->is_child = true;
                            c->is_last = (c_curr == c_total);
                            
                            if (xml_mode) {
                                strncpy(c->name, sub, 63);
                            } else {
                                const char *conn = c->is_last ? "`-- " : "|-- ";
                                snprintf(c->name, 63, "%s%s", conn, sub);
                            }

                            strncpy(c->fstype, (char*)dkw.dkw_ptype, 15);
                            strncpy(c->label, (char*)dkw.dkw_wname, 63);
                            strncpy(c->mount, get_mountpoint(sub), MAXPATHLEN-1);
                            c->size = (uint64_t)dkw.dkw_size * 512;
                            update_max(c);
                        }
                        close(sfd);
                    }
                }
                sub = strtok_r(NULL, " ", &iptr);
            }
            free(inner);
        }
        token = strtok_r(NULL, " ", &dptr);
    }

    if (xml_mode) {
        output_xml(list, count);
    } else {
        printf("%-*s   %-*s   %-*s   %-*s   %8s\n\n", 
               max_name, "NAME", max_type, "FSTYPE", max_label, "LABEL", max_mnt, "MOUNTPOINT", "SIZE");

        for (int i = 0; i < count; i++) {
            char sz[12];
            humanize_number(sz, sizeof(sz), list[i].size, "", HN_AUTOSCALE, HN_DECIMAL);
            printf("%-*s   %-*s   %-*s   %-*s   %8s\n", 
                   max_name, list[i].name, max_type, list[i].fstype, 
                   max_label, list[i].label, max_mnt, list[i].mount, sz);
        }
    }
    free(disks);
}

int main(int argc, char **argv) {
    bool xml = false;
    int ch;
    while ((ch = getopt(argc, argv, "x")) != -1) {
        if (ch == 'x') xml = true;
    }
    scan_and_print(xml);
    return 0;
}
