#pragma once

#include <stddef.h>
#include <yak/types.h>
#include <yak/status.h>
#include <yak/fs/vfs.h>

struct device_ops {
	status_t (*dev_read)(int minor, voff_t offset, void *buf, size_t length,
			     size_t *read_bytes);

	status_t (*dev_write)(int minor, voff_t offset, const void *buf,
			      size_t length, size_t *written_bytes);

	status_t (*dev_open)(int minor, struct vnode **vp);
};

status_t devfs_register(char *name, int type, int major, int minor,
			struct device_ops *ops, struct vnode **out);
