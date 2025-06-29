// SPDX-License-Identifier: GPL-2.0-only
#include <common.h>
#include <malloc.h>
#include <fb.h>
#include <errno.h>
#include <command.h>
#include <getopt.h>
#include <fcntl.h>
#include <fs.h>
#include <init.h>

static int fb_ioctl(struct cdev* cdev, unsigned int req, void *data)
{
	struct fb_info *info = cdev->priv;
	struct fb_info **fb;
	int ret;

	switch (req) {
	case FBIOGET_SCREENINFO:
		fb = data;
		*fb = info;
		ret = 0;
		break;
	case FBIO_ENABLE:
		ret = fb_enable(info);
		break;
	case FBIO_DISABLE:
		ret = fb_disable(info);
		break;
	default:
		return -ENOSYS;
	}

	return ret;
}

static int fb_close(struct cdev *cdev)
{
	struct fb_info *info = cdev->priv;

	if (info->fbops->fb_flush)
		info->fbops->fb_flush(info);
	return 0;
}

void fb_damage(struct fb_info *info, struct fb_rect *rect)
{
	if (info->fbops->fb_damage)
		info->fbops->fb_damage(info, rect);
}

static int fb_op_flush(struct cdev *cdev)
{
	struct fb_info *info = cdev->priv;

	if (info->fbops->fb_flush)
		info->fbops->fb_flush(info);
	return 0;
}

void fb_flush(struct fb_info *info)
{
	if (info->fbops->fb_flush)
		info->fbops->fb_flush(info);
}

static void fb_release_shadowfb(struct fb_info *info)
{
	free(info->screen_base_shadow);
	info->screen_base_shadow = NULL;
}

static int fb_alloc_shadowfb(struct fb_info *info)
{
	if (info->screen_base_shadow && info->shadowfb)
		return 0;

	if (!info->screen_base_shadow && !info->shadowfb)
		return 0;

	if (info->shadowfb) {
		info->screen_base_shadow = memalign(PAGE_SIZE,
				info->line_length * info->yres);
		if (!info->screen_base_shadow)
			return -ENOMEM;
		memcpy(info->screen_base_shadow, info->screen_base,
				info->line_length * info->yres);
	} else {
		fb_release_shadowfb(info);
	}

	return 0;
}

int fb_enable(struct fb_info *info)
{
	int ret;

	if (info->enabled)
		return 0;

	ret = fb_alloc_shadowfb(info);
	if (ret)
		return ret;

	if (info->fbops->fb_enable)
		info->fbops->fb_enable(info);

	info->enabled = true;

	return 0;
}

int fb_disable(struct fb_info *info)
{
	if (!info->enabled)
		return 0;

	if (info->fbops->fb_disable)
		info->fbops->fb_disable(info);

	fb_release_shadowfb(info);

	info->enabled = false;

	return 0;
}

static int fb_enable_get(struct param_d *param, void *priv)
{
	struct fb_info *info = priv;

	info->p_enable = info->enabled;
	return 0;
}

static int fb_enable_set(struct param_d *param, void *priv)
{
	struct fb_info *info = priv;

	if (!info->mode && !info->base_plane)
		return -EINVAL;

	if (info->p_enable)
		return fb_enable(info);
	else
		return fb_disable(info);
}

static struct fb_videomode *fb_num_to_mode(struct fb_info *info, int num)
{
	int num_modes;

	num_modes = info->modes.num_modes + info->edid_modes.num_modes;

	if (num >= num_modes)
		return NULL;

	if (num >= info->modes.num_modes)
		return &info->edid_modes.modes[num - info->modes.num_modes];

	return &info->modes.modes[num];
}

static int fb_setup_mode(struct fb_info *info)
{
	struct device *dev = &info->dev;
	int ret;
	struct fb_videomode *mode;

	if (info->enabled != 0)
		return -EPERM;

	mode = fb_num_to_mode(info, info->current_mode);
	if (!mode)
		return -EINVAL;

	info->mode = mode;

	info->xres = info->mode->xres;
	info->yres = info->mode->yres;
	info->line_length = 0;

	if (info->fbops->fb_activate_var) {
		ret = info->fbops->fb_activate_var(info);
		if (ret) {
			info->cdev.size = 0;
			return ret;
		}
	}

	if (!info->line_length)
		info->line_length = info->xres * (info->bits_per_pixel >> 3);
	if (!info->screen_size)
		info->screen_size = info->line_length * info->yres;

	dev->resource[0].start = (resource_size_t)info->screen_base;
	info->cdev.size = info->line_length * info->yres;
	dev->resource[0].end = dev->resource[0].start + info->cdev.size - 1;

	return 0;
}

static int fb_of_reserve_fixup(struct device_node *root, void *context)
{
	struct fb_info *info = context;

	if (!info->enabled)
		return 0;

	of_add_reserve_entry((unsigned long)info->screen_base,
			(unsigned long)info->screen_base + info->screen_size);

	return 0;
}

void fb_of_reserve_add_fixup(struct fb_info *info)
{
	if (!IS_ENABLED(CONFIG_OFDEVICE))
		return;

	of_register_fixup(fb_of_reserve_fixup, info);
}

static int fb_set_modename(struct param_d *param, void *priv)
{
	struct fb_info *info = priv;
	int ret;

	ret = fb_setup_mode(info);
	if (ret)
		return ret;

	return 0;
}

static struct cdev_operations fb_ops = {
	.read	= mem_read,
	.write	= mem_write,
	.memmap	= generic_memmap_rw,
	.ioctl	= fb_ioctl,
	.close  = fb_close,
	.flush  = fb_op_flush,
};

static void fb_print_mode(struct fb_videomode *mode)
{
	printf("  %s: %dx%d@%d\n", mode->name,
			mode->xres, mode->yres, mode->refresh);
}

static void fb_print_modes(struct display_timings *modes)
{
	int i;

	for (i = 0; i < modes->num_modes; i++)
		fb_print_mode(&modes->modes[i]);
}

static void fb_info(struct device *dev)
{
	struct fb_info *info = dev->priv;

	printf("Available modes:\n");
	fb_print_modes(&info->modes);
	fb_print_modes(&info->edid_modes);
	if (info->base_plane) {
		printf("Type: overlay\n");
		printf("base plane: %s\n", dev_name(&info->base_plane->dev));
	} else {
		printf("Type: primary\n");
	}
	printf("xres: %u\n", info->xres);
	printf("yres: %u\n", info->yres);
	printf("line_length: %u\n", info->line_length);
}

void *fb_get_screen_base(struct fb_info *info)
{
	return info->screen_base_shadow ?
		info->screen_base_shadow : info->screen_base;
}

static int fb_set_shadowfb(struct param_d *p, void *priv)
{
	struct fb_info *info = priv;

	if (!info->enabled)
		return 0;

	return fb_alloc_shadowfb(info);
}

static const char *mode_name(struct fb_videomode *mode)
{
	return basprintf("%dx%d@%d", mode->xres, mode->yres, mode->refresh);
}

int register_framebuffer(struct fb_info *info)
{
	int id;
	struct device *dev;
	int ret, num_modes, i;
	const char **names;

	dev = &info->dev;

	/*
	 * If info->mode is set at this point it's the only mode
	 * the fb supports. move it over to the modes list.
	 */
	if (info->mode) {
		info->modes.modes = info->mode;
		info->modes.num_modes = 1;
	}

	if (!info->line_length)
		info->line_length = info->xres * (info->bits_per_pixel >> 3);

	info->cdev.ops = &fb_ops;
	if (info->base_plane) {
		int ovl_id = info->base_plane->n_overlays++;

		info->cdev.name = basprintf("%s_%d", dev_name(&info->base_plane->dev), ovl_id);
		dev->id = DEVICE_ID_SINGLE;
		dev_set_name(dev, "%s", info->cdev.name);
	} else {
		id = get_free_deviceid("fb");
		dev->id = id;
		dev_set_name(dev, "fb");
		info->cdev.name = basprintf("fb%d", id);
	}
	info->cdev.size = info->line_length * info->yres;
	info->cdev.dev = dev;
	info->cdev.priv = info;
	dev->resource = xzalloc(sizeof(struct resource));
	dev->resource[0].start = (resource_size_t)info->screen_base;
	dev->resource[0].end = dev->resource[0].start + info->cdev.size - 1;
	dev->resource[0].flags = IORESOURCE_MEM;
	dev->num_resources = 1;

	dev->priv = info;
	devinfo_add(dev, fb_info);

	ret = register_device(&info->dev);
	if (ret)
		goto err_free;

	dev_add_param_bool(dev, "enable", fb_enable_set, fb_enable_get,
			&info->p_enable, info);

	if (IS_ENABLED(CONFIG_DRIVER_VIDEO_EDID))
		fb_edid_add_modes(info);

	num_modes = info->modes.num_modes + info->edid_modes.num_modes;

	names = xzalloc(sizeof(char *) * num_modes);

	for (i = 0; i < info->modes.num_modes; i++) {
		if (!info->modes.modes[i].name)
			info->modes.modes[i].name = mode_name(&info->modes.modes[i]);
		names[i] = info->modes.modes[i].name;
	}

	for (i = 0; i < info->edid_modes.num_modes; i++) {
		if (!info->edid_modes.modes[i].name)
			info->modes.modes[i].name = mode_name(&info->edid_modes.modes[i]);
		names[i + info->modes.num_modes] = info->edid_modes.modes[i].name;
	}

	dev_add_param_enum(dev, "mode_name", fb_set_modename, NULL, &info->current_mode, names, num_modes, info);
	info->shadowfb = 1;
	dev_add_param_bool(dev, "shadowfb", fb_set_shadowfb, NULL, &info->shadowfb, info);

	info->mode = fb_num_to_mode(info, 0);

	fb_setup_mode(info);

	ret = devfs_create(&info->cdev);
	if (ret)
		goto err_unregister;

	if (IS_ENABLED(CONFIG_DRIVER_VIDEO_SIMPLEFB)) {
		ret = fb_register_simplefb(info);
		if (ret)
			dev_err(&info->dev, "failed to register simplefb: %pe\n",
					ERR_PTR(ret));
	}

	if (IS_ENABLED(CONFIG_FRAMEBUFFER_CONSOLE))
		register_fbconsole(info);

	return 0;

err_unregister:
	unregister_device(&info->dev);
err_free:
	free(dev->resource);

	return ret;
}
