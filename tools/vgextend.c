/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2009 Red Hat, Inc. All rights reserved.
 *
 * This file is part of LVM2.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tools.h"

int vgextend(struct cmd_context *cmd, int argc, char **argv)
{
	char *vg_name;
	struct volume_group *vg = NULL;
	int r = ECMD_FAILED;

	if (!argc) {
		log_error("Please enter volume group name and "
			  "physical volume(s)");
		return EINVALID_CMD_LINE;
	}

	if (argc == 1) {
		log_error("Please enter physical volume(s)");
		return EINVALID_CMD_LINE;
	}

	vg_name = skip_dev_dir(cmd, argv[0], NULL);
	argc--;
	argv++;

	log_verbose("Checking for volume group \"%s\"", vg_name);
	vg = vg_read_for_update(cmd, vg_name, NULL, 0);
	if (vg_read_error(vg)) {
		vg_release(vg);
		return ECMD_FAILED;
	}
/********** FIXME
	log_print("maximum logical volume size is %s",
		  (dummy = lvm_show_size(LVM_LV_SIZE_MAX(vg) / 2, LONG)));
	dm_free(dummy);
	dummy = NULL;
**********/

	if (!archive(vg))
		goto error;

	/* extend vg */
	if (!vg_extend(vg, argc, argv))
		goto error;

	/* ret > 0 */
	log_verbose("Volume group \"%s\" will be extended by %d new "
		    "physical volumes", vg_name, argc);

	/* store vg on disk(s) */
	if (!vg_write(vg) || !vg_commit(vg))
		goto error;

	backup(vg);
	log_print("Volume group \"%s\" successfully extended", vg_name);
	r = ECMD_PROCESSED;

error:
	unlock_and_release_vg(cmd, vg, vg_name);
	return r;
}
