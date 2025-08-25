#pragma once

#include <stddef.h>
#include <yak/status.h>
#include <yak/mutex.h>

struct vnode;

struct vfs {
	struct vfs_ops *ops;
	struct vnode *vnodecovered;
};

struct vfs_ops {
	status_t (*vfs_mount)(struct vnode *vroot);
	status_t (*vfs_unmount)(struct vfs *vfsp);
	struct vnode *(*vfs_getroot)(struct vfs *vfsp);
};

#define VFS_GETROOT(vfs) (vfs)->ops->vfs_getroot(vfs)

enum vtype {
	VREG = 1,
	VBLK,
	VDIR,
	VCHR,
	VFIFO,
	VLNK,
	VSOCK,
};

#define DT_REG 1
#define DT_DIR 3

struct vnode {
	struct vn_ops *ops;
	enum vtype type;

	size_t refcnt;
	struct kmutex lock;

	struct vfs *vfs;
	struct vfs *mountedvfs;
};

typedef int64_t ino_t;
typedef int64_t off_t;

struct dirent {
	ino_t d_ino;
	off_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[];
};

struct vn_ops {
	status_t (*vn_lookup)(struct vnode *vp, char *name, struct vnode **out);

	status_t (*vn_create)(struct vnode *vp, enum vtype type, char *name,
			      struct vnode **out);

	status_t (*vn_lock)(struct vnode *vp);
	status_t (*vn_unlock)(struct vnode *vp);

	status_t (*vn_inactive)(struct vnode *vp);

	status_t (*vn_getdents)(struct vnode *vp, struct dirent *buf,
				size_t bufsize, size_t *bytes_read);

	status_t (*vn_write)(struct vnode *vp, size_t offset, const char *buf,
			     size_t *count);
	status_t (*vn_read)(struct vnode *vp, size_t offset, char *buf,
			    size_t *count);
};

#define VOP_INIT(vn, vfs_, ops_, type_)    \
	(vn)->ops = ops_;                  \
	(vn)->type = type_;                \
	(vn)->refcnt = 1;                  \
	kmutex_init(&(vn)->lock, "vnode"); \
	(vn)->vfs = vfs_;                  \
	(vn)->mountedvfs = NULL;

#define VOP_LOOKUP(vp, name, out) vp->ops->vn_lookup(vp, name, out)
#define VOP_CREATE(vp, type, name, out) vp->ops->vn_create(vp, type, name, out)
#define VOP_GETDENTS(vp, buf, bufsize, bytes_read) \
	vp->ops->vn_getdents(vp, buf, bufsize, bytes_read)

#define VOP_WRITE(vp, offset, buf, count) \
	vp->ops->vn_write(vp, offset, buf, count)

#define VOP_READ(vp, offset, buf, count) \
	vp->ops->vn_read(vp, offset, buf, count)

#define VOP_LOCK(vp) vp->ops->vn_lock(vp)
#define VOP_UNLOCK(vp) vp->ops->vn_unlock(vp)

#define VOP_RETAIN(vp) __atomic_fetch_add(&(vp)->refcnt, 1, __ATOMIC_ACQUIRE);
#define VOP_RELEASE(vp)                                                    \
	if (0 == __atomic_sub_fetch(&(vp)->refcnt, 1, __ATOMIC_ACQ_REL)) { \
		vp->ops->vn_inactive(vp);                                  \
	}

void vfs_init();

status_t vfs_register(const char *name, struct vfs_ops *ops);

status_t vfs_mount(const char *path, char *fsname);

status_t vfs_getdents(struct vnode *vn, struct dirent *buf, size_t bufsize,
		      size_t *bytes_read);

status_t vfs_write(struct vnode *vn, size_t offset, const char *buf,
		   size_t *count);

status_t vfs_read(struct vnode *vn, size_t offset, char *buf, size_t *count);

status_t vfs_create(char *path, enum vtype type, struct vnode **out);
