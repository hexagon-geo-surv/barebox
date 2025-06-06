// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2018, Tuomas Tynkkynen <tuomas.tynkkynen@iki.fi>
 * Copyright (C) 2018, Bin Meng <bmeng.cn@gmail.com>
 */

#include <common.h>
#include <driver.h>
#include <block.h>
#include <disks.h>
#include <linux/virtio_types.h>
#include <linux/virtio.h>
#include <linux/virtio_ring.h>
#include <uapi/linux/virtio_blk.h>

struct virtio_blk_priv {
	struct virtqueue *vq;
	struct virtio_device *vdev;
	struct block_device blk;
};

static int virtio_blk_do_req(struct virtio_blk_priv *priv, void *buffer,
			     sector_t sector, blkcnt_t blkcnt, u32 type)
{
	unsigned int num_out = 0, num_in = 0;
	struct scatterlist hdr_sg, data_sg, status_sg, *sgs[3];
	u8 status = VIRTIO_BLK_S_IOERR;
	int ret;

	struct virtio_blk_outhdr out_hdr = {
		.type = cpu_to_virtio32(priv->vdev, type),
		.sector = cpu_to_virtio64(priv->vdev, sector),
	};

	sg_init_one(&hdr_sg, &out_hdr, sizeof(out_hdr));
	sgs[num_out++] = &hdr_sg;

	sg_init_one(&data_sg, buffer, blkcnt * 512);

	switch(type) {
	case VIRTIO_BLK_T_OUT:
		sgs[num_out++] = &data_sg;
		break;
	case VIRTIO_BLK_T_IN:
		sgs[num_out + num_in++] = &data_sg;
		break;
	}

	sg_init_one(&status_sg, &status, sizeof(status));
	sgs[num_out + num_in++] = &status_sg;

	ret = virtqueue_add_sgs(priv->vq, sgs, num_out, num_in, &out_hdr);
	if (ret)
		return ret;

	virtqueue_kick(priv->vq);

	if (!virtqueue_get_buf_timeout(priv->vq, NULL, NSEC_PER_SEC))
		return -ETIMEDOUT;

	return status == VIRTIO_BLK_S_OK ? 0 : -EIO;
}

static int virtio_blk_read(struct block_device *blk, void *buffer,
			   sector_t start, blkcnt_t blkcnt)
{
	struct virtio_blk_priv *priv = container_of(blk, struct virtio_blk_priv, blk);
	return virtio_blk_do_req(priv, buffer, start, blkcnt,
				 VIRTIO_BLK_T_IN);
}

static int virtio_blk_write(struct block_device *blk, const void *buffer,
			    sector_t start, blkcnt_t blkcnt)
{
	struct virtio_blk_priv *priv = container_of(blk, struct virtio_blk_priv, blk);
	return virtio_blk_do_req(priv, (void *)buffer, start, blkcnt,
				 VIRTIO_BLK_T_OUT);
}

static struct block_device_ops virtio_blk_ops = {
	.read	= virtio_blk_read,
	.write	= virtio_blk_write,
};

static int virtio_blk_probe(struct virtio_device *vdev)
{
	struct virtio_blk_priv *priv;
	u64 cap;
	int devnum;
	int ret;

	priv = xzalloc(sizeof(*priv));

	ret = virtio_find_vqs(vdev, 1, &priv->vq);
	if (ret)
		return ret;

	priv->vdev = vdev;
	vdev->priv = priv;

	devnum = cdev_find_free_index("virtioblk");
	priv->blk.cdev.name = xasprintf("virtioblk%d", devnum);
	cdev_set_of_node(&priv->blk.cdev, vdev->dev.device_node);
	priv->blk.dev = &vdev->dev;
	priv->blk.blockbits = SECTOR_SHIFT;
	virtio_cread(vdev, struct virtio_blk_config, capacity, &cap);
	priv->blk.num_blocks = cap;
	priv->blk.ops = &virtio_blk_ops;
	priv->blk.type = BLK_TYPE_VIRTUAL;

	return blockdevice_register(&priv->blk);
}

static void virtio_blk_remove(struct virtio_device *vdev)
{
	struct virtio_blk_priv *priv = vdev->priv;

	vdev->config->reset(vdev);
	blockdevice_unregister(&priv->blk);
	vdev->config->del_vqs(vdev);

	free(priv);
}

static const struct virtio_device_id id_table[] = {
        { VIRTIO_ID_BLOCK, VIRTIO_DEV_ANY_ID },
        { 0 },
};

static struct virtio_driver virtio_blk = {
        .driver.name	= "virtio_blk",
        .id_table	= id_table,
        .probe		= virtio_blk_probe,
	.remove		= virtio_blk_remove,
};
device_virtio_driver(virtio_blk);
