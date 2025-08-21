#pragma once

#include <yak/status.h>

struct vnode;

struct user_credentials {};

struct vfs {
	struct vfs_ops *op;
	struct vnode *vnodecovered;
};

struct vfs_ops {
	status_t (*vfs_mount)(struct vnode *vroot);
	status_t (*vfs_unmount)(struct vfs *vfsp);
	struct vnode *(*vfs_getroot)(struct vfs *vfsp);
};

enum vtype {
	VREG,
	VBLK,
	VDIR,
	VCHR,
	VFIFO,
	VLNK,
	VSOCK,
};

struct vnode {
	struct vn_ops *op;
	struct vfs *vfs;
	struct vfs *mountedvfs;
	enum vtype type;
};

struct vn_ops {
	status_t (*vn_open)(struct vnode **vpp, unsigned int flags,
			    struct user_credentials *c);

	status_t (*vn_close)(struct vnode *vp, unsigned int flags,
			     struct user_credentials *c);

	status_t (*vn_lookup)(struct vnode *vp, char *name,
			      struct user_credentials *c, struct vnode **out);

	status_t (*vn_create)(struct vnode *vp, enum vtype type, char *name,
			      struct vnode **out);
};

void vfs_init();

void vfs_register(const char *name, struct vfs_ops *ops);

status_t vfs_mount(char *path, char *fsname);
