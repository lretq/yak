#pragma once

#include <stddef.h>
#include <yak/status.h>
#include <yak/mutex.h>
#include <yak/refcount.h>
#include <yak/vmflags.h>

struct vm_map;

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

	refcount_t refcnt;
	struct kmutex lock;

	struct vfs *vfs;
	struct vfs *mountedvfs;

	size_t filesize;

	struct vm_object *vobj;
};

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

	status_t (*vn_symlink)(struct vnode *parent, char *name, char *path,
			       struct vnode **out);

	status_t (*vn_readlink)(struct vnode *vn, char **path);

	status_t (*vn_read)(struct vnode *vn, voff_t offset, void *buf,
			    size_t length, size_t *read_bytes);

	status_t (*vn_write)(struct vnode *vp, voff_t offset, const void *buf,
			     size_t length, size_t *written_bytes);

	status_t (*vn_open)(struct vnode **vp);

	status_t (*vn_ioctl)(struct vnode *vp, unsigned long com, void *data);

	status_t (*vn_mmap)(struct vnode *vp, struct vm_map *map, size_t length,
			    voff_t offset, vm_prot_t prot,
			    vm_inheritance_t inheritance, vaddr_t hint,
			    int flags, vaddr_t *out);

	status_t (*vn_fallocate)(struct vnode *vp, int mode, off_t offset,
				 off_t size);

	bool (*vn_isatty)(struct vnode *vp);
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

#define VOP_OPEN(vp) (*(vp))->ops->vn_open(vp)

#define VOP_SYMLINK(vp, name, dest, out) \
	vp->ops->vn_symlink(vp, name, dest, out)

#define VOP_READLINK(vp, out) vp->ops->vn_readlink(vp, out)

#define VOP_IOCTL(vp, com, data) vp->ops->vn_ioctl(vp, com, data)

#define VOP_MMAP(vp, m, l, o, p, i, h, f, out) \
	vp->ops->vn_mmap(vp, m, l, o, p, i, h, f, out)

#define VOP_FALLOCATE(vp, mode, offset, size) \
	vp->ops->vn_fallocate(vp, mode, offset, size)

#define VOP_ISATTY(vp) vp->ops->vn_isatty(vp)

GENERATE_REFMAINT_INLINE(vnode, refcnt, p->ops->vn_inactive)

void vfs_init();

status_t vfs_register(const char *name, struct vfs_ops *ops);

status_t vfs_mount(const char *path, char *fsname);

status_t vfs_getdents(struct vnode *vn, struct dirent *buf, size_t bufsize,
		      size_t *bytes_read);

status_t vfs_write(struct vnode *vn, size_t offset, const void *buf,
		   size_t count, size_t *writtenp);

status_t vfs_read(struct vnode *vn, size_t offset, void *buf, size_t count,
		  size_t *readp);

status_t vfs_create(char *path, enum vtype type, struct vnode **out);

status_t vfs_open(char *path, struct vnode **out);

status_t vfs_symlink(char *link_path, char *dest_path, struct vnode **out);

status_t vfs_ioctl(struct vnode *vn, unsigned long com, void *data);

status_t vfs_mmap(struct vnode *vn, struct vm_map *map, size_t length,
		  voff_t offset, vm_prot_t prot, vm_inheritance_t inheritance,
		  vaddr_t hint, int flags, vaddr_t *out);
