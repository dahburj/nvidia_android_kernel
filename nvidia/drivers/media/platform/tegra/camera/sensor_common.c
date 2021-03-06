/*
 * sensor_common.c - utilities for tegra sensor drivers
 *
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <media/sensor_common.h>
#include <linux/of_graph.h>
#include <linux/string.h>

static int read_property_u32(
	struct device_node *node, const char *name, u32 *value)
{
	const char *str;
	int err = 0;

	err = of_property_read_string(node, name, &str);
	if (err)
		return -ENODATA;

	err = kstrtou32(str, 10, value);
	if (err)
		return -EFAULT;

	return 0;
}

static int read_property_u64(
	struct device_node *node, const char *name, u64 *value)
{
	const char *str;
	int err = 0;

	err = of_property_read_string(node, name, &str);
	if (err)
		return -ENODATA;

	err = kstrtou64(str, 10, value);
	if (err)
		return -EFAULT;

	return 0;
}

static int sensor_common_parse_signal_props(
	struct device *dev, struct device_node *node,
	struct sensor_signal_properties *signal)
{
	const char *temp_str;
	int err = 0;
	u32 value = 0;
	u64 val64 = 0;

	/* Do not report error for these properties yet */
	err = read_property_u32(node, "readout_orientation", &value);
	if (err)
		signal->readout_orientation = 0;
	else
		signal->readout_orientation = value;

	err = read_property_u32(node, "num_lanes", &value);
	if (err)
		signal->num_lanes = 0;
	else
		signal->num_lanes = value;

	err = read_property_u32(node, "mclk_khz", &value);
	if (err)
		signal->mclk_freq = 0;
	else
		signal->mclk_freq = value;

	err = read_property_u64(node, "pix_clk_hz", &val64);
	if (err) {
		dev_err(dev, "%s:pix_clk_hz property missing\n", __func__);
		return err;
	}
	signal->pixel_clock.val = val64;

	err = read_property_u32(node, "cil_settletime", &value);
	if (err)
		signal->cil_settletime = 0;
	else
		signal->cil_settletime = value;

	/* initialize default if this prop not available */
	err = of_property_read_string(node, "discontinuous_clk", &temp_str);
	if (!err)
		signal->discontinuous_clk =
			!strncmp(temp_str, "yes", sizeof("yes"));
	else
		signal->discontinuous_clk = 1;

	/* initialize default if this prop not available */
	err = of_property_read_string(node, "dpcm_enable", &temp_str);
	if (!err)
		signal->dpcm_enable =
			!strncmp(temp_str, "true", sizeof("true"));
	else
		signal->dpcm_enable = 0;

	/* initialize default if this prop not available */
	err = of_property_read_string(node,
					"deskew_initial_enable", &temp_str);
	if (!err)
		signal->deskew_initial_enable =
				!strncmp(temp_str, "true", sizeof("true"));
	else
		signal->deskew_initial_enable = 0;
	err = of_property_read_string(node,
					"deskew_periodic_enable", &temp_str);
	if (!err)
		signal->deskew_periodic_enable =
				!strncmp(temp_str, "true", sizeof("true"));
	else
		signal->deskew_periodic_enable = 0;

	err = of_property_read_string(node, "tegra_sinterface", &temp_str);
	if (err) {
		dev_err(dev,
			"%s: tegra_sinterface property missing\n", __func__);
		return err;
	}

	if (strcmp(temp_str, "serial_a") == 0)
		signal->tegra_sinterface = 0;
	else if (strcmp(temp_str, "serial_b") == 0)
		signal->tegra_sinterface = 1;
	else if (strcmp(temp_str, "serial_c") == 0)
		signal->tegra_sinterface = 2;
	else if (strcmp(temp_str, "serial_d") == 0)
		signal->tegra_sinterface = 3;
	else if (strcmp(temp_str, "serial_e") == 0)
		signal->tegra_sinterface = 4;
	else if (strcmp(temp_str, "serial_f") == 0)
		signal->tegra_sinterface = 5;
	else if (strcmp(temp_str, "serial_g") == 0)
		signal->tegra_sinterface = 6;
	else if (strcmp(temp_str, "serial_h") == 0)
		signal->tegra_sinterface = 7;
	else if (strcmp(temp_str, "host") == 0)
		signal->tegra_sinterface = 0; /* for vivid driver */
	else {
		dev_err(dev,
			"%s: tegra_sinterface property out of range\n",
			__func__);
		return -EINVAL;
	}

	err = of_property_read_string(node, "phy_mode", &temp_str);
	if (err) {
		dev_dbg(dev, "%s: use default phy mode DPHY\n", __func__);
		signal->phy_mode = CSI_PHY_MODE_DPHY;
	} else {
		if (strcmp(temp_str, "CPHY") == 0)
			signal->phy_mode = CSI_PHY_MODE_CPHY;
		else if (strcmp(temp_str, "DPHY") == 0)
			signal->phy_mode = CSI_PHY_MODE_DPHY;
		else if (strcmp(temp_str, "SLVS") == 0)
			signal->phy_mode = SLVS_EC;
		else {
			dev_err(dev, "%s: Invalid Phy mode\n", __func__);
			return -EINVAL;
		}
	}

	return 0;
}

static int extract_pixel_format(
	const char *pixel_t, u32 *format)
{
	size_t size = strnlen(pixel_t, OF_MAX_STR_LEN);

	if (strncmp(pixel_t, "bayer_bggr10", size) == 0)
		*format = V4L2_PIX_FMT_SBGGR10;
	else if (strncmp(pixel_t, "bayer_rggb10", size) == 0)
		*format = V4L2_PIX_FMT_SRGGB10;
	else if (strncmp(pixel_t, "bayer_grbg10", size) == 0)
		*format = V4L2_PIX_FMT_SGRBG10;
	else if (strncmp(pixel_t, "bayer_bggr12", size) == 0)
		*format = V4L2_PIX_FMT_SBGGR12;
	else if (strncmp(pixel_t, "bayer_rggb12", size) == 0)
		*format = V4L2_PIX_FMT_SRGGB12;
	else if (strncmp(pixel_t, "bayer_wdr_pwl_rggb12", size) == 0)
		*format = V4L2_PIX_FMT_SRGGB12;
	else if (strncmp(pixel_t, "bayer_wdr_dol_rggb10", size) == 0)
		*format = V4L2_PIX_FMT_SRGGB10;
	else if (strncmp(pixel_t, "bayer_xbggr10p", size) == 0)
		*format = V4L2_PIX_FMT_XBGGR10P;
	else if (strncmp(pixel_t, "bayer_xrggb10p", size) == 0)
		*format = V4L2_PIX_FMT_XRGGB10P;
	else {
		pr_err("%s: Need to extend format%s\n", __func__, pixel_t);
		return -EINVAL;
	}

	return 0;
}

static int sensor_common_parse_image_props(
	struct device *dev, struct device_node *node,
	struct sensor_image_properties *image)
{
	const char *temp_str;
	int err = 0;
	const char *phase_str, *mode_str;
	int depth;
	char pix_format[24];
	u32 value = 0;

	err = read_property_u32(node, "active_w",
		&image->width);
	if (err) {
		dev_err(dev, "%s:active_w property missing\n", __func__);
		goto fail;
	}

	err = read_property_u32(node, "active_h",
		&image->height);
	if (err) {
		dev_err(dev, "%s:active_h property missing\n", __func__);
		goto fail;
	}

	err = read_property_u32(node, "line_length",
		&image->line_length);
	if (err) {
		dev_err(dev, "%s:Line length property missing\n", __func__);
		goto fail;
	}

	/* embedded_metadata_height is optional */
	err = read_property_u32(node, "embedded_metadata_height", &value);
	if (err)
		image->embedded_metadata_height = 0;
	else
		image->embedded_metadata_height = value;

	err = of_property_read_string(node, "pixel_t", &temp_str);
	if (err) {
		/* pixel_t missing is only an error if alternate not provided */

		/* check for alternative format string */
		err = of_property_read_string(node, "pixel_phase", &phase_str);
		if (err) {
			dev_err(dev,
				"%s:pixel_phase property missing.\n",
				__func__);
			dev_err(dev,
				"%s:Either pixel_t or alternate must be present.\n",
				__func__);
			goto fail;
		}
		err = of_property_read_string(node, "mode_type", &mode_str);
		if (err) {
			dev_err(dev,
				"%s:mode_type property missing.\n",
				__func__);
			dev_err(dev,
				"%s:Either pixel_t or alternate must be present.\n",
				__func__);
			goto fail;
		}
		err = read_property_u32(node, "csi_pixel_bit_depth", &depth);
		if (err) {
			dev_err(dev,
				"%s:csi_pixel_bit_depth property missing.\n",
				__func__);
			dev_err(dev,
				"%s:Either pixel_t or alternate must be present.\n",
				__func__);
			goto fail;
		}
		sprintf(pix_format, "%s_%s%d", mode_str, phase_str, depth);
		temp_str = pix_format;
	}

	err = extract_pixel_format(temp_str, &image->pixel_format);
	if (err) {
		dev_err(dev, "Unsupported pixel format\n");
		goto fail;
	}

fail:
	return err;
}

static int sensor_common_parse_dv_timings(
	struct device *dev, struct device_node *node,
	struct sensor_dv_timings *timings)
{
	int err = 0;
	u32 value = 0;

	/* Do not report error for these properties yet */
	err = read_property_u32(node, "horz_front_porch", &value);
	if (err)
		timings->hfrontporch = 0;
	else
		timings->hfrontporch = value;

	err = read_property_u32(node, "horz_sync", &value);
	if (err)
		timings->hsync = 0;
	else
		timings->hsync = value;

	err = read_property_u32(node, "horz_back_porch", &value);
	if (err)
		timings->hbackporch = 0;
	else
		timings->hbackporch = value;

	err = read_property_u32(node, "vert_front_porch", &value);
	if (err)
		timings->vfrontporch = 0;
	else
		timings->vfrontporch = value;

	err = read_property_u32(node, "vert_sync", &value);
	if (err)
		timings->vsync = 0;
	else
		timings->vsync = value;

	err = read_property_u32(node, "vert_back_porch", &value);
	if (err)
		timings->vbackporch = 0;
	else
		timings->vbackporch = value;

	return 0;
}

static int sensor_common_parse_control_props(
	struct device *dev, struct device_node *node,
	struct sensor_control_properties *control)
{
	int err = 0;
	u32 value = 0;
	u64 val64 = 0;

	err = read_property_u32(node, "gain_factor", &value);
	if (err) {
		dev_dbg(dev, "%s:%s:property missing\n",
			__func__, "gain_factor");
		control->gain_factor = 1;
		return 0;
	} else
		control->gain_factor = value;

	err = read_property_u32(node, "framerate_factor", &value);
	if (err) {
		dev_err(dev, "%s:%s:property missing\n",
			__func__, "framerate_factor");
		control->framerate_factor = 1;
	} else
		control->framerate_factor = value;

	err = read_property_u32(node, "exposure_factor", &value);
	if (err) {
		dev_err(dev, "%s:%s:property missing\n",
			__func__, "framerate_factor");
		control->exposure_factor = 1;
	} else
		control->exposure_factor = value;

	/* ignore err for this prop */
	err = read_property_u32(node, "inherent_gain", &value);
	if (err)
		control->inherent_gain = 0;
	else
		control->inherent_gain = value;

	err = read_property_u32(node, "min_gain_val", &value);
	if (err) {
		dev_err(dev, "%s:%s:property missing\n",
			__func__, "min_gain_val");
		control->min_gain_val = 0;
	} else
		control->min_gain_val = value;

	err = read_property_u32(node, "max_gain_val", &value);
	if (err) {
		dev_err(dev, "%s:%s:property missing\n",
			__func__, "max_gain_val");
		control->max_gain_val = 0;
	} else
		control->max_gain_val = value;

	err = read_property_u32(node, "step_gain_val", &value);
	if (err) {
		dev_err(dev, "%s:%s:property missing\n",
			__func__, "step_gain_val");
		control->step_gain_val = 0;
	} else
		control->step_gain_val = value;

	/* ignore err for this prop */
	err = read_property_u32(node, "min_hdr_ratio", &value);
	if (err)
		control->min_hdr_ratio = 0;
	else
		control->min_hdr_ratio = value;

	err = read_property_u32(node, "max_hdr_ratio", &value);
	if (err)
		control->max_hdr_ratio = 0;
	else
		control->max_hdr_ratio = value;

	err = read_property_u32(node, "min_framerate", &value);
	if (err) {
		dev_err(dev, "%s:%s:property missing\n",
			__func__, "min_framerate");
		control->min_framerate = 0;
	} else
		control->min_framerate = value;

	err = read_property_u32(node, "max_framerate", &value);
	if (err) {
		dev_err(dev, "%s:%s:property missing\n",
			__func__, "max_framerate");
		control->max_framerate = 0;
	} else
		control->max_framerate = value;

	err = read_property_u32(node, "step_framerate", &value);
	if (err) {
		dev_err(dev, "%s:%s:property missing\n",
			__func__, "step_framerate");
		control->step_framerate = 0;
	} else
		control->step_framerate = value;

	err = read_property_u64(node, "min_exp_time", &val64);
	if (err) {
		dev_err(dev, "%s:%s:property missing\n",
			__func__, "min_exp_time");
		control->min_exp_time.val = 0;
	}
		control->min_exp_time.val = val64;

	err = read_property_u64(node, "max_exp_time", &val64);
	if (err) {
		dev_err(dev, "%s:%s:property missing\n",
			__func__, "max_exp_time");
		control->max_exp_time.val = 0;
	} else
		control->max_exp_time.val = val64;

	err = read_property_u64(node, "step_exp_time", &val64);
	if (err) {
		dev_err(dev, "%s:%s:property missing\n",
			__func__, "step_exp_time");
		control->step_exp_time.val = 0;
	} else
		control->step_exp_time.val = val64;

	return 0;
}

int sensor_common_parse_num_modes(const struct device *dev)
{
	struct device_node *np;
	struct device_node *node = NULL;
	char temp_str[OF_MAX_STR_LEN];
	int num_modes = 0;
	int i;

	if (!dev || !dev->of_node)
		return 0;

	np = dev->of_node;

	for (i = 0; num_modes < MAX_NUM_SENSOR_MODES; i++) {
		snprintf(temp_str, sizeof(temp_str), "%s%d",
			OF_SENSORMODE_PREFIX, i);
		of_node_get(np);
		node = of_get_child_by_name(np, temp_str);
		of_node_put(node);
		if (node == NULL)
			break;
		num_modes++;
	}

	return num_modes;
}
EXPORT_SYMBOL(sensor_common_parse_num_modes);

int sensor_common_init_sensor_properties(
	struct device *dev, struct device_node *np,
	struct sensor_properties *sensor)
{
	char temp_str[OF_MAX_STR_LEN];
	struct device_node *node = NULL;
	int num_modes = 0;
	int err, i;

	if (sensor == NULL)
		return -EINVAL;

	/* get number of modes */
	for (i = 0; num_modes < MAX_NUM_SENSOR_MODES; i++) {
		snprintf(temp_str, sizeof(temp_str), "%s%d",
			OF_SENSORMODE_PREFIX, i);
		of_node_get(np);
		node = of_get_child_by_name(np, temp_str);
		of_node_put(node);
		if (node == NULL)
			break;
		num_modes++;
	}
	sensor->num_modes = num_modes;

	sensor->sensor_modes = devm_kzalloc(dev,
		num_modes * sizeof(struct sensor_mode_properties),
		GFP_KERNEL);
	if (!sensor->sensor_modes) {
		dev_err(dev, "Failed to allocate memory for sensor modes\n");
		err = -ENOMEM;
		goto alloc_fail;
	}

	for (i = 0; i < num_modes; i++) {
		snprintf(temp_str, sizeof(temp_str), "%s%d",
			OF_SENSORMODE_PREFIX, i);
		of_node_get(np);
		node = of_get_child_by_name(np, temp_str);
		if (node == NULL) {
			dev_err(dev, "Failed to find %s\n", temp_str);
			err = -ENODATA;
			goto fail;
		};

		dev_dbg(dev, "parsing for %s props\n", temp_str);

		err = sensor_common_parse_signal_props(dev, node,
			&sensor->sensor_modes[i].signal_properties);
		if (err) {
			dev_err(dev, "Failed to read %s signal props\n",
				temp_str);
			goto fail;
		}

		err = sensor_common_parse_image_props(dev, node,
			&sensor->sensor_modes[i].image_properties);
		if (err) {
			dev_err(dev, "Failed to read %s image props\n",
				temp_str);
			goto fail;
		}

		err = sensor_common_parse_dv_timings(dev, node,
			&sensor->sensor_modes[i].dv_timings);
		if (err) {
			dev_err(dev, "Failed to read %s DV timings\n",
				temp_str);
			goto fail;
		}

		err = sensor_common_parse_control_props(dev, node,
			&sensor->sensor_modes[i].control_properties);
		if (err) {
			dev_err(dev, "Failed to read %s control props\n",
				temp_str);
			goto fail;
		}
		of_node_put(node);
	}

	return 0;

fail:
	devm_kfree(dev, sensor->sensor_modes);
alloc_fail:
	of_node_put(node);
	return err;
}
EXPORT_SYMBOL(sensor_common_init_sensor_properties);
