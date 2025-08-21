#define pr_fmt(fmt) "vfs: " fmt

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <yak/fs/vfs.h>
#include <yak/queue.h>
#include <yak/heap.h>
#include <yak/log.h>
#include <yak/status.h>
#include <string.h>

#define MAX_FS_NAME 12
struct factory {
	SLIST_ENTRY(factory) entry;
	char fs_name[MAX_FS_NAME + 1];
	size_t name_len;
	struct vfs_ops *ops;
};

static SLIST_HEAD(, factory) factories_registered;

static struct vnode *root_node;

void vfs_init()
{
	SLIST_INIT(&factories_registered);

	root_node = kmalloc(sizeof(struct vnode));
	root_node->type = VDIR;
	root_node->op = NULL;
	root_node->mountedvfs = NULL;
}

void vfs_register(const char *name, struct vfs_ops *ops)
{
	assert(strlen(name) < MAX_FS_NAME);

	struct factory *f = kmalloc(sizeof(struct factory));
	assert(f);

	strncpy(f->fs_name, name, sizeof(f->fs_name));
	f->name_len = strlen(f->fs_name);
	f->ops = ops;

	SLIST_INSERT_HEAD(&factories_registered, f, entry);
}

static struct vfs_ops *lookup_fs(const char *name)
{
	struct factory *elm;
	SLIST_FOREACH(elm, &factories_registered, entry)
	{
		if (strncmp(elm->fs_name, name, elm->name_len) == 0) {
			return elm->ops;
		}
	}

	return NULL;
}

#define VFS_LOOKUP_PARENT (1 << 0)

status_t vfs_lookup_path(const char *path, struct vnode *cwd, int flags,
			 struct vnode **out, char **last_comp);

status_t vfs_mount(char *path, char *fsname)
{
	struct vfs_ops *ops = lookup_fs(fsname);
	if (!ops)
		return YAK_UNKNOWN_FS;

	struct vnode *vn;
	status_t res = vfs_lookup_path(path, NULL, 0, &vn, NULL);
	IF_ERR(res)
	{
		return res;
	}

	res = ops->vfs_mount(vn);
	IF_ERR(res)
	{
		return res;
	}

	pr_info("mounted %s on %s\n", fsname, path);

	return YAK_SUCCESS;
}

status_t vfs_create(char *path, enum vtype type)
{
	struct vnode *vn;
	char *last_comp;
	status_t res =
		vfs_lookup_path(path, NULL, VFS_LOOKUP_PARENT, &vn, &last_comp);
	pr_info("last_comp: %s\n", last_comp);
	if (res == YAK_NOENT) {
		if (!vn)
			return res;
	} else {
		return IS_OK(res) ? YAK_EXISTS : res;
	}

	res = vn->op->vn_create(vn, type, last_comp, &vn);
	IF_ERR(res)
	{
		return res;
	}

	return YAK_SUCCESS;
}

static struct vnode *resolve(struct vnode *vn)
{
	assert(vn != NULL);
	if (vn->mountedvfs) {
		return resolve(vn->mountedvfs->op->vfs_getroot(vn->mountedvfs));
	}

	// TODO: handle links

	return vn;
}

size_t split_path(char *path)
{
	size_t count = 0;
	int in_comp = 0;

	char *dst = path;
	char *src = path;
	while (*src) {
		if (*src == '/') {
			while (*src == '/')
				src++;

			if (in_comp) {
				*dst++ = '\0';
				in_comp = 0;
			}
		} else {
			if (!in_comp) {
				count++;
				in_comp = 1;
			}

			*dst++ = *src++;
		}
	}

	*dst = '\0';
	return count;
}

status_t vfs_lookup_path(const char *path_, struct vnode *cwd, int flags,
			 struct vnode **out, char **last_comp)
{
	size_t pathlen = strlen(path_);

	int want_dir = (path_[pathlen - 1] == '/');

	struct vnode *current;

	if (path_[0] == '/') {
		// absolute path
		if (pathlen == 1) {
			*out = resolve(root_node);
			return YAK_SUCCESS;
		}

		current = resolve(root_node);
	} else {
		// relative path
		current = cwd;
	}

	char *path;
	aguard_arr(char, pathlen + 1, path);
	strcpy(path, path_);
	size_t n_comps = split_path(path);

	pr_info("components in path: %ld\n", n_comps);

	status_t res;
	char *comp = path;

	*out = NULL;

	for (size_t i = 0; i < n_comps; i++) {
		if (current->type != VDIR) {
			return YAK_NODIR;
		}

		int is_last = (i + 1 == n_comps);
		pr_info("[%ld] %s%s\n", i, comp, is_last ? " (last)" : "");

		if (is_last && (flags & VFS_LOOKUP_PARENT)) {
			*out = current;
			if (last_comp)
				*last_comp = strdup(comp);
		}

		res = current->op->vn_lookup(current, comp, NULL, &current);
		pr_debug("lookup %s\n", comp);
		IF_ERR(res) return res;

		if (is_last && !(flags & VFS_LOOKUP_PARENT)) {
			*out = current;
		}

		if (is_last) {
			if (want_dir && current->type != VDIR)
				return YAK_NODIR;

			if (last_comp)
				*last_comp = strdup(comp);

			return YAK_SUCCESS;
		}

		comp += strlen(comp) + 1;
	}

	return YAK_NOENT;
}
