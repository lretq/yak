#define pr_fmt(fmt) "tmpfs: " fmt

#include <yak/heap.h>
#include <yak/queue.h>
#include <yak/hashtable.h>
#include <yak/fs/vfs.h>
#include <yak/macro.h>
#include <yak/status.h>
#include <yak/vm/map.h>
#include <yak/vm/page.h>
#include <yak/vm/pmm.h>
#include <yak/vm/aobj.h>
#include <yak/types.h>
#include <yak/log.h>
#include <string.h>
#include <stddef.h>

struct tmpfs_node {
	struct vnode vnode;
	char *name;
	size_t name_len;

	char *link_path;

	size_t inode;

	struct hashtable children;
};

struct tmpfs {
	struct vfs vfs;

	struct tmpfs_node *root;
	size_t seq_ino;
};

static struct tmpfs_node *create_node(struct vfs *vfs, enum vtype type);

struct vnode *tmpfs_getroot(struct vfs *vfs)
{
	struct tmpfs *fs = (struct tmpfs *)vfs;
	if (!fs->root) {
		fs->root = create_node(vfs, VDIR);
	}
	return &fs->root->vnode;
}

status_t tmpfs_inactive(struct vnode *vn)
{
	// TODO:free node
	return YAK_SUCCESS;
}

status_t tmpfs_create(struct vnode *parent, enum vtype type, char *name,
		      struct vnode **out)
{
	struct tmpfs_node *parent_node = (struct tmpfs_node *)parent;

	struct tmpfs_node *n;

	if ((n = hashtable_get(&parent_node->children, name)) != NULL) {
		pr_debug("exists already: %s (parent: %s, n: %s)\n", name,
			 parent_node->name, n->name);
		return YAK_EXISTS;
	}

	struct tmpfs_node *node = create_node(parent->vfs, type);
	if (!node) {
		return YAK_OOM;
	}

	node->name_len = strlen(name);
	node->name = strndup(name, node->name_len);

	status_t ret;
	IF_ERR((ret = hashtable_set(&parent_node->children, name, node, 0)))
	{
		return ret;
	};

	vnode_ref(parent);

	*out = &node->vnode;
	return YAK_SUCCESS;
}

status_t tmpfs_symlink(struct vnode *parent, char *name, char *path,
		       struct vnode **out)
{
	char *path_copy = strdup(path);

	struct vnode *linkvn;
	status_t rv = tmpfs_create(parent, VLNK, name, &linkvn);
	IF_ERR(rv)
	{
		kfree(path_copy, 0);
		return rv;
	}

	struct tmpfs_node *tmpfs_node = (struct tmpfs_node *)linkvn;
	tmpfs_node->link_path = path_copy;

	*out = linkvn;

	return YAK_SUCCESS;
}

status_t tmpfs_readlink(struct vnode *vn, char **path)
{
	if (vn->type != VLNK || path == NULL)
		return YAK_INVALID_ARGS;

	*path = strdup(((struct tmpfs_node *)vn)->link_path);

	return YAK_SUCCESS;
}

status_t tmpfs_getdents(struct vnode *vn, struct dirent *buf, size_t bufsize,
			size_t *bytes_read)
{
	if (vn->type != VDIR) {
		return YAK_NODIR;
	}

	struct tmpfs_node *tvn = (struct tmpfs_node *)vn;

	char *outp = (char *)buf;
	size_t remaining = bufsize;
	size_t written = 0;

	struct hashtable_entry *elm;
	HASHTABLE_FOR_EACH(&tvn->children, elm)
	{
		struct tmpfs_node *child = elm->value;
		const char *name = elm->key;
		size_t namelen = elm->key_len + 1;
		size_t reclen = offsetof(struct dirent, d_name) + namelen;
		reclen = ALIGN_UP(reclen, sizeof(long));

		if (reclen > remaining) {
			break;
		}

		struct dirent *d = (struct dirent *)outp;
		d->d_ino = child->inode;
		d->d_off = 0; // ?
		d->d_reclen = (unsigned short)reclen;
		d->d_type = child->vnode.type; // DT_* = V*
		memcpy(d->d_name, name, namelen);

		outp += reclen;
		remaining -= reclen;
		written += reclen;
	}

	*bytes_read = written;

	return YAK_SUCCESS;
}

status_t tmpfs_lookup(struct vnode *vn, char *name, struct vnode **out)
{
	if (vn->type != VDIR) {
		return YAK_NODIR;
	}

	struct tmpfs_node *elm =
		hashtable_get(&((struct tmpfs_node *)vn)->children, name);
	if (!elm)
		return YAK_NOENT;

	kmutex_acquire(&elm->vnode.lock, 0);
	kmutex_release(&elm->vnode.lock);
	*out = &elm->vnode;
	return YAK_SUCCESS;
}

status_t tmpfs_lock(struct vnode *vn)
{
	kmutex_acquire(&vn->lock, TIMEOUT_INFINITE);
	return YAK_SUCCESS;
}

status_t tmpfs_unlock(struct vnode *vn)
{
	kmutex_release(&vn->lock);
	return YAK_SUCCESS;
}

struct vn_ops tmpfs_vn_op = {
	.vn_lookup = tmpfs_lookup,
	.vn_create = tmpfs_create,
	.vn_lock = tmpfs_lock,
	.vn_unlock = tmpfs_unlock,
	.vn_inactive = tmpfs_inactive,
	.vn_getdents = tmpfs_getdents,
	.vn_symlink = tmpfs_symlink,
	.vn_readlink = tmpfs_readlink,
};

status_t tmpfs_mount(struct vnode *vn);

struct vfs_ops tmpfs_op = {
	.vfs_mount = tmpfs_mount,
	.vfs_getroot = tmpfs_getroot,
};

struct vfs tmpfs = {
	.ops = &tmpfs_op,
};

void tmpfs_init()
{
	EXPECT(vfs_register("tmpfs", &tmpfs_op));
}

status_t tmpfs_mount(struct vnode *vn)
{
	struct tmpfs *fs = kmalloc(sizeof(struct tmpfs));
	fs->root = NULL;
	fs->seq_ino = 1;

	vnode_ref(vn);
	fs->vfs.vnodecovered = vn;

	fs->vfs.ops = &tmpfs_op;

	vn->mountedvfs = &fs->vfs;
	return YAK_SUCCESS;
}

static struct tmpfs_node *create_node(struct vfs *vfs, enum vtype type)
{
	struct tmpfs_node *node = kmalloc(sizeof(struct tmpfs_node));
	if (!node)
		return NULL;

	memset(node, 0, sizeof(struct tmpfs_node));

	if (type == VDIR) {
		hashtable_init(&node->children);
	} else {
		node->vnode.filesize = 0;
		node->vnode.vobj = vm_aobj_create();
	}

	node->name = NULL;
	node->name_len = 0;
	node->inode = __atomic_fetch_add(&((struct tmpfs *)vfs)->seq_ino, 1,
					 __ATOMIC_RELAXED);

	VOP_INIT(&node->vnode, vfs, &tmpfs_vn_op, type);

	return node;
}
