// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016 Pengutronix, Steffen Trumtrar <kernel@pengutronix.de>
 *
 * derived from Linux kernel drivers/char/hw_random/core.c
 */

#include <common.h>
#include <linux/hw_random.h>
#include <malloc.h>

static LIST_HEAD(hwrngs);

#define RNG_BUFFER_SIZE		32

int hwrng_get_data(struct hwrng *rng, void *buffer, size_t size, int wait)
{
	return rng->read(rng, buffer, size, wait);
}

static int hwrng_init(struct hwrng *rng)
{
	int ret = 0;

	if (rng->init)
		ret = rng->init(rng);

	if (!ret)
		list_add_tail(&rng->list, &hwrngs);

	return ret;
}

static ssize_t rng_dev_read(struct cdev *cdev, void *buf, size_t size,
			    loff_t offset, unsigned long flags)
{
	struct hwrng *rng = container_of(cdev, struct hwrng, cdev);
	size_t count = size;
	ssize_t cur = 0;
	int len;

	memset(buf, 0, size);

	while (count) {
		int max = min(count, (size_t)RNG_BUFFER_SIZE);
		len = hwrng_get_data(rng, rng->buf, max, true);
		if (len < 0)
			return len;
		if (!len && ctrlc())
			return cur;

		memcpy(buf + cur, rng->buf, len);

		count -= len;
		cur += len;
	}

	return cur;
}

static struct cdev_operations rng_chrdev_ops = {
	.read  = rng_dev_read,
};

static int hwrng_register_cdev(struct hwrng *rng)
{
	struct device *dev = rng->dev;
	const char *alias;
	char *devname;
	int err;

	alias = of_alias_get(dev->of_node);
	if (alias) {
		devname = xstrdup(alias);
	} else {
		err = cdev_find_free_index("hwrng");
		if (err < 0) {
			dev_err(dev, "no index found to name device\n");
			return err;
		}
		devname = xasprintf("hwrng%d", err);
	}

	rng->cdev.name = devname;
	rng->cdev.flags = DEVFS_IS_CHARACTER_DEV;
	rng->cdev.ops = &rng_chrdev_ops;
	rng->cdev.dev = rng->dev;

	return devfs_create(&rng->cdev);
}

static void hwrng_unregister_cdev(struct hwrng *rng)
{
	devfs_remove(&rng->cdev);
	free(rng->cdev.name);
}

struct hwrng *hwrng_get_first(void)
{
	if (list_empty(&hwrngs))
		return ERR_PTR(-ENODEV);
	else
		return list_first_entry(&hwrngs, struct hwrng, list);
}

int hwrng_register(struct device *dev, struct hwrng *rng)
{
	int err;

	if (rng->name == NULL || rng->read == NULL)
		return -EINVAL;

	rng->buf = xzalloc(RNG_BUFFER_SIZE);
	rng->dev = dev;

	err = hwrng_init(rng);
	if (err) {
		free(rng->buf);
		return err;
	}

	err = hwrng_register_cdev(rng);
	if (err)
		free(rng->buf);

	return err;
}

void hwrng_unregister(struct hwrng *rng)
{
	hwrng_unregister_cdev(rng);
	free(rng->buf);
}
