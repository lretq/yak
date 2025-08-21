#define pr_fmt(fmt) "tmpfs: " fmt

#include <yak/heap.h>
#include <yak/queue.h>
#include <yak/fs/vfs.h>
#include <yak/macro.h>
#include <yak/status.h>
#include <yak/log.h>
#include <string.h>
#include <stddef.h>

struct tmpfs_node {
	struct vnode vnode;
	char *name;
	size_t name_len;

	TAILQ_ENTRY(tmpfs_node) entry;
	TAILQ_HEAD(, tmpfs_node) children;
};

struct tmpfs {
	struct vfs vfs;

	struct tmpfs_node *root;
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

status_t tmpfs_create(struct vnode *vp, enum vtype type, char *name,
		      struct vnode **out)
{
	struct tmpfs_node *node = create_node(vp->vfs, type);
	node->name = strdup(name);
	node->name_len = strlen(name);
	TAILQ_INSERT_HEAD(&((struct tmpfs_node *)vp)->children, node, entry);
	*out = &node->vnode;
	return YAK_SUCCESS;
}

status_t tmpfs_lookup(struct vnode *vn, char *name,
		      [[maybe_unused]] struct user_credentials *c,
		      struct vnode **out)
{
	if (vn->type != VDIR) {
		return YAK_NODIR;
	}

	struct tmpfs_node *elm;
	TAILQ_FOREACH(elm, &((struct tmpfs_node *)vn)->children, entry)
	{
		if (strncmp(name, elm->name, elm->name_len) == 0) {
			*out = &elm->vnode;
			return YAK_SUCCESS;
		}
	}

	return YAK_NOENT;
}

struct vn_ops tmpfs_vn_op = {
	.vn_lookup = tmpfs_lookup,
	.vn_create = tmpfs_create,
};

status_t tmpfs_mount(struct vnode *vn);

struct vfs_ops tmpfs_op = {
	.vfs_mount = tmpfs_mount,
	.vfs_getroot = tmpfs_getroot,
};

struct vfs tmpfs = {
	.op = &tmpfs_op,
};

void tmpfs_init()
{
	vfs_register("tmpfs", &tmpfs_op);
}

status_t tmpfs_mount(struct vnode *vn)
{
	struct tmpfs *fs = kmalloc(sizeof(struct tmpfs));
	memset(fs, 0, sizeof(struct tmpfs));
	fs->vfs.op = &tmpfs_op;
	vn->mountedvfs = &fs->vfs;
	vn->vfs = &fs->vfs;
	fs->vfs.vnodecovered = vn;
	return YAK_SUCCESS;
}

static struct tmpfs_node *create_node(struct vfs *vfs, enum vtype type)
{
	struct tmpfs_node *node = kmalloc(sizeof(struct tmpfs_node));
	memset(node, 0, sizeof(struct tmpfs_node));

	if (type == VDIR) {
		TAILQ_INIT(&node->children);
	}

	node->name = NULL;
	node->name_len = 0;

	node->vnode.type = type;
	node->vnode.vfs = vfs;
	node->vnode.op = &tmpfs_vn_op;

	return node;
}
