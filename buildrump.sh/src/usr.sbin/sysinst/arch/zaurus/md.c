/*	$NetBSD: md.c,v 1.11 2021/08/09 19:24:33 andvar Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Based on code written by Philip A. Nelson for Piermont Information
 * Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* md.c -- zaurus machine specific routines */

#include <sys/param.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include <util.h>

#include "defs.h"
#include "md.h"
#include "msg_defs.h"
#include "menu_defs.h"

void
md_init(void)
{
}

void
md_init_set_status(int flags)
{
	static const int mib[2] = {CTL_KERN, KERN_VERSION};
	size_t len;
	char *version;

	/* check INSTALL kernel name to select an appropriate kernel set */
	/* XXX: hw.cpu_model has a processor name on arm ports */
	sysctl(mib, 2, NULL, &len, NULL, 0);
	version = malloc(len);
	if (version == NULL)
		return;
	sysctl(mib, 2, version, &len, NULL, 0);
	if (strstr(version, "C700") != NULL)
		set_kernel_set(SET_KERNEL_C700);
	free(version);
}

bool
md_get_info(struct install_partition_desc *install)
{
	int res;

	if (pm->no_mbr || pm->no_part)
		return true;

again:
	if (pm->parts == NULL) {

		const struct disk_partitioning_scheme *ps =
		    select_part_scheme(pm, NULL, true, NULL);

		if (!ps)
			return false;

		struct disk_partitions *parts =
		   (*ps->create_new_for_disk)(pm->diskdev,
		   0, pm->dlsize, true, NULL);
		if (!parts)
			return false;

		pm->parts = parts;
		if (ps->size_limit > 0 && pm->dlsize > ps->size_limit)
			pm->dlsize = ps->size_limit;
	}

	res = set_bios_geom_with_mbr_guess(pm->parts);
	if (res == 0)
		return false;
	else if (res == 1)
		return true;

	pm->parts->pscheme->destroy_part_scheme(pm->parts);
	pm->parts = NULL;
	goto again;
}

int
md_make_bsd_partitions(struct install_partition_desc *install)
{
	return make_bsd_partitions(install);
}

/*
 * any additional partition validation
 */
bool
md_check_partitions(struct install_partition_desc *install)
{
	return true;
}

/*
 * hook called before writing new disklabel.
 */
bool
md_pre_disklabel(struct install_partition_desc *install,
    struct disk_partitions *parts)
{

	if (parts->parent == NULL)
		return true;	/* no outer partitions */

	parts = parts->parent;

	msg_display_subst(MSG_dofdisk, 3, parts->disk,
	    msg_string(parts->pscheme->name),
	    msg_string(parts->pscheme->short_name));

	/* write edited "MBR" onto disk. */
	if (!parts->pscheme->write_to_disk(parts)) {
		msg_display(MSG_wmbrfail);
		process_menu(MENU_ok, NULL);
		return false;
	}
	return true;
}

/*
 * hook called after writing disklabel to new target disk.
 */
bool
md_post_disklabel(struct install_partition_desc *install,
    struct disk_partitions *parts)
{
	return true;
}

/*
 * hook called after upgrade() or install() has finished setting
 * up the target disk but immediately before the user is given the
 * ``disks are now set up'' message.
 */
int
md_post_newfs(struct install_partition_desc *install)
{
	struct mbr_sector pbr;
	char adevname[STRSIZE];
	ssize_t sz;
	int fd = -1;

	snprintf(adevname, sizeof(adevname), "/dev/r%sa", pm->diskdev);
	fd = open(adevname, O_RDWR);
	if (fd < 0)
		goto out;

	/* Read partition boot record */
	sz = pread(fd, &pbr, sizeof(pbr), 0);
	if (sz != sizeof(pbr))
		goto out;

	/* Check magic number */
	if (pbr.mbr_magic != le16toh(MBR_MAGIC))
		goto out;

#ifdef NETBSD_MAJOR
#define	OSNAME	"NetBSD" NETBSD_MAJOR
#else
#define	OSNAME	"NetBSD60"
#endif
	/* Update oemname */
	memcpy(&pbr.mbr_oemname, OSNAME, sizeof(OSNAME) - 1);

	/* Clear BPB */
	memset(&pbr.mbr_bpb, 0, sizeof(pbr.mbr_bpb));

	/* write-backed new partition boot record */
	(void)pwrite(fd, &pbr, sizeof(pbr), 0);

out:
	if (fd >= 0)
		close(fd);
	return 0;
}

int
md_post_extract(struct install_partition_desc *install)
{
	return 0;
}

void
md_cleanup_install(struct install_partition_desc *install)
{
#ifndef DEBUG
	enable_rc_conf();
#endif
}

int
md_pre_update(struct install_partition_desc *install)
{
	return 1;
}

/* Upgrade support */
int
md_update(struct install_partition_desc *install)
{
	md_post_newfs(install);
	return 1;
}

int
md_check_mbr(struct disk_partitions *parts, mbr_info_t *mbri, bool quiet)
{
	return 2;
}

bool
md_parts_use_wholedisk(struct disk_partitions *parts)
{

	return parts_use_wholedisk(parts, 0, NULL);
}


int
md_pre_mount(struct install_partition_desc *install, size_t ndx)
{
	return 0;
}

bool
md_mbr_update_check(struct disk_partitions *parts, mbr_info_t *mbri)
{
	return false;	/* no change, no need to write back */
}

#ifdef HAVE_GPT
bool
md_gpt_post_write(struct disk_partitions *parts, part_id root_id,
    bool root_is_new, part_id efi_id, bool efi_is_new)
{
	/* no GPT boot support, nothing needs to be done here */
	return true;
}
#endif

