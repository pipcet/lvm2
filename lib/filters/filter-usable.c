/*
 * Copyright (C) 2014 Red Hat, Inc. All rights reserved.
 *
 * This file is part of LVM2.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "base/memory/zalloc.h"
#include "lib/misc/lib.h"
#include "lib/filters/filter.h"
#include "lib/activate/activate.h"

struct filter_data {
	filter_mode_t mode;
	int skip_lvs;
};

static const char *_too_small_to_hold_pv_msg = "Too small to hold a PV";

static int _check_pv_min_size(struct device *dev)
{
	uint64_t size;
	int ret = 0;

	/* Check it's not too small */
	if (!dev_get_size(dev, &size)) {
		log_debug_devs("%s: Skipping: dev_get_size failed", dev_name(dev));
		goto out;
	}

	if (size < pv_min_size()) {
		log_debug_devs("%s: Skipping: %s", dev_name(dev),
				_too_small_to_hold_pv_msg);
		goto out;
	}

	ret = 1;
out:
	return ret;
}

static int _passes_usable_filter(struct cmd_context *cmd, struct dev_filter *f, struct device *dev, const char *use_filter_name)
{
	struct filter_data *data = f->private;
	filter_mode_t mode = data->mode;
	int skip_lvs = data->skip_lvs;
	struct dev_usable_check_params ucp = {0};
	int is_lv = 0;
	int r = 1;

	dev->filtered_flags &= ~DEV_FILTERED_MINSIZE;
	dev->filtered_flags &= ~DEV_FILTERED_UNUSABLE;

	/* further checks are done on dm devices only */
	if (dm_is_dm_major(MAJOR(dev->dev))) {
		switch (mode) {
		case FILTER_MODE_NO_LVMETAD:
			ucp.check_empty = 1;
			ucp.check_blocked = 1;
			ucp.check_suspended = ignore_suspended_devices();
			ucp.check_error_target = 1;
			ucp.check_reserved = 1;
			ucp.check_lv = skip_lvs;
			break;
		case FILTER_MODE_PRE_LVMETAD:
			ucp.check_empty = 1;
			ucp.check_blocked = 1;
			ucp.check_suspended = 0;
			ucp.check_error_target = 1;
			ucp.check_reserved = 1;
			ucp.check_lv = skip_lvs;
			break;
		case FILTER_MODE_POST_LVMETAD:
			ucp.check_empty = 0;
			ucp.check_blocked = 1;
			ucp.check_suspended = ignore_suspended_devices();
			ucp.check_error_target = 0;
			ucp.check_reserved = 0;
			ucp.check_lv = skip_lvs;
			break;
		}

		if (!(r = device_is_usable(cmd, dev, ucp, &is_lv))) {
			if (is_lv)
				dev->filtered_flags |= DEV_FILTERED_IS_LV;
			else
				dev->filtered_flags |= DEV_FILTERED_UNUSABLE;
			log_debug_devs("%s: Skipping unusable device.", dev_name(dev));
		}
	}

	if (r) {
		r = _check_pv_min_size(dev);
		if (!r)
			dev->filtered_flags |= DEV_FILTERED_MINSIZE;
	}

	return r;
}

static void _usable_filter_destroy(struct dev_filter *f)
{
	if (f->use_count)
		log_error(INTERNAL_ERROR "Destroying usable device filter while in use %u times.", f->use_count);

	free(f->private);
	free(f);
}

struct dev_filter *usable_filter_create(struct cmd_context *cmd, struct dev_types *dt __attribute__((unused)), filter_mode_t mode)
{
	struct filter_data *data;
	struct dev_filter *f;

	if (!(f = zalloc(sizeof(struct dev_filter)))) {
		log_error("Usable device filter allocation failed");
		return NULL;
	}

	f->passes_filter = _passes_usable_filter;
	f->destroy = _usable_filter_destroy;
	f->use_count = 0;
	f->name = "usable";

	if (!(data = zalloc(sizeof(struct filter_data)))) {
		log_error("Usable device filter mode allocation failed");
		free(f);
		return NULL;
	}

	data->mode = mode;

	data->skip_lvs = !find_config_tree_bool(cmd, devices_scan_lvs_CFG, NULL);

	f->private = data;

	log_debug_devs("Usable device filter initialised (scan_lvs %d).", !data->skip_lvs);

	return f;
}
