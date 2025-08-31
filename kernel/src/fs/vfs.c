#define pr_fmt(fmt) "vfs: " fmt

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <yak/macro.h>
#include <yak/types.h>
#include <yak/hashtable.h>
#include <yak/heap.h>
#include <yak/log.h>
#include <yak/queue.h>
#include <yak/status.h>
#include <yak/fs/vfs.h>
#include <yak/vm.h>
#include <yak/vm/map.h>
#include <yak/vm/object.h>

#define MAX_FS_NAME 16

static struct hashtable filesystems;

static struct vnode *root_node;

status_t root_lock([[maybe_unused]] struct vnode *vn)
{
	return YAK_SUCCESS;
}

status_t root_unlock([[maybe_unused]] struct vnode *vn)
{
	return YAK_SUCCESS;
}

status_t root_inactive([[maybe_unused]] struct vnode *vn)
{
	pr_warn("root_node is inactive??\n");
	return YAK_SUCCESS;
}

static struct vn_ops root_ops = {
	.vn_lock = root_lock,
	.vn_unlock = root_unlock,
	.vn_inactive = root_inactive,
};

void vfs_init()
{
	hashtable_init(&filesystems);

	root_node = kmalloc(sizeof(struct vnode));
	VOP_INIT(root_node, NULL, &root_ops, VDIR);
}

status_t vfs_register(const char *name, struct vfs_ops *ops)
{
	assert(strlen(name) < MAX_FS_NAME);
	return hashtable_set(&filesystems, name, ops, 0);
}

static struct vfs_ops *lookup_fs(const char *name)
{
	return hashtable_get(&filesystems, name);
}

#define VFS_LOOKUP_PARENT (1 << 0)

status_t vfs_lookup_path(const char *path, struct vnode *cwd, int flags,
			 struct vnode **out, char **last_comp);

status_t vfs_mount(const char *path, char *fsname)
{
	struct vfs_ops *ops = lookup_fs(fsname);
	if (!ops)
		return YAK_UNKNOWN_FS;

	struct vnode *vn;
	status_t res = vfs_lookup_path(path, NULL, 0, &vn, NULL);
	IF_ERR(res)
	{
		goto exit;
	}

	res = ops->vfs_mount(vn);
	IF_ERR(res)
	{
		goto exit;
	}

	pr_info("mounted %s on %s\n", fsname, path);

exit:
	if (vn) {
		VOP_UNLOCK(vn);
		vnode_deref(vn);
	}

	return res;
}

status_t vfs_create(char *path, enum vtype type, struct vnode **out)
{
	struct vnode *parent, *vn;
	char *last_comp = NULL;
	status_t res = vfs_lookup_path(path, NULL, VFS_LOOKUP_PARENT, &parent,
				       &last_comp);
	guard(autofree)(last_comp, last_comp ? strlen(last_comp) + 1 : 0);
	IF_ERR(res)
	{
		return res;
	}

	res = VOP_CREATE(parent, type, last_comp, &vn);

	VOP_UNLOCK(parent);
	vnode_deref(parent);

	if (out && IS_OK(res)) {
		*out = vn;
	}

	return res;
}

static struct vnode *resolve(struct vnode *vn)
{
	assert(vn != NULL);
	if (vn->mountedvfs) {
		return resolve(VFS_GETROOT(vn->mountedvfs));
	}

	// TODO: handle links

	return vn;
}

static size_t split_path(char *path)
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

status_t vfs_write(struct vnode *vp, voff_t offset, const void *buf,
		   size_t *count)
{
	struct vm_object *obj = vp->vobj;

	if (count == NULL || vp->type == VDIR)
		return YAK_INVALID_ARGS;

	if (*count == 0)
		return YAK_SUCCESS;

	size_t length = *count;

	size_t written = 0;

	size_t start_off = offset;
	size_t end_off = offset + length;

	VOP_LOCK(vp);
	if (end_off > vp->filesize) {
		vp->filesize = end_off;
	}
	VOP_UNLOCK(vp);

	const char *src = buf;

	while (start_off < end_off) {
		voff_t pageoff = ALIGN_DOWN(start_off, PAGE_SIZE);
		size_t page_offset = start_off - pageoff;
		size_t chunk =
			MIN(PAGE_SIZE - page_offset, end_off - start_off);

		struct page *pg;
		// Lookup or allocate the page
		status_t res = vm_lookuppage(obj, pageoff, 0, &pg);
		IF_ERR(res)
		{
			return res;
		}

		memcpy((char *)page_to_mapped_addr(pg) + page_offset, src,
		       chunk);

		// TODO: mark page as dirt?
		// later, pager shall write it back

		src += chunk;
		start_off += chunk;
		written += chunk;
	}

	*count = written;
	return YAK_SUCCESS;
}

status_t vfs_read(struct vnode *vn, voff_t offset, void *buf, size_t *count)
{
	struct vm_object *obj = vn->vobj;

	if (count == NULL || vn->type == VDIR)
		return YAK_INVALID_ARGS;

	if (*count == 0)
		return YAK_SUCCESS;

	size_t length = *count;
	if (offset >= vn->filesize) {
		*count = 0;
		return YAK_EOF;
	}

	if (offset + length > vn->filesize)
		length = vn->filesize - offset;

	size_t read = 0;
	char *dst = buf;

	voff_t start_off = offset;
	voff_t end_off = offset + length;

	while (start_off < end_off) {
		voff_t pageoff = ALIGN_DOWN(start_off, PAGE_SIZE);
		size_t page_offset = start_off - pageoff;
		size_t chunk =
			MIN(PAGE_SIZE - page_offset, end_off - start_off);

		struct page *pg;
		// Lookup or retrieve the page
		status_t res = vm_lookuppage(obj, pageoff, 0, &pg);
		IF_ERR(res)
		{
			return res;
		}

		memcpy(dst, (char *)page_to_mapped_addr(pg) + page_offset,
		       chunk);

		dst += chunk;
		start_off += chunk;
		read += chunk;
	}

	*count = read;
	return YAK_SUCCESS;
}

status_t vfs_open(char *path, struct vnode **out)
{
	struct vnode *vn;
	status_t res = vfs_lookup_path(path, NULL, 0, &vn, NULL);
	IF_ERR(res)
	{
		return res;
	}

	res = VOP_OPEN(vn);
	IF_ERR(res)
	{
		VOP_UNLOCK(vn);
		vnode_deref(vn);
		return res;
	}

	VOP_UNLOCK(vn);

	// keep refcnt
	*out = vn;

	return YAK_SUCCESS;
}

status_t vfs_lookup_path(const char *path_, struct vnode *cwd, int flags,
			 struct vnode **out, char **last_comp)
{
	size_t pathlen = strlen(path_);

	int want_dir = (path_[pathlen - 1] == '/');

	// don't resolve cwd, because we don't want
	// to access a newly mounted filesystem
	struct vnode *current = (path_[0] == '/') ? resolve(root_node) : cwd;
	struct vnode *next = NULL;

	// we can't stack allocate, symlinks can recurse
	char *path = kmalloc(pathlen + 1);
	guard(autofree)(path, pathlen + 1);
	memcpy(path, path_, pathlen + 1);

	// turns '/' to '\0' in-place
	size_t n_comps = split_path(path);

	*out = NULL;
	if (last_comp)
		*last_comp = NULL;

	vnode_ref(current);
	VOP_LOCK(current);

	if (pathlen == 1 && path_[0] == '/') {
		*out = current;
		assert(current->type == VDIR);
		return YAK_SUCCESS;
	}

	char *comp = path;

	for (size_t i = 0; i < n_comps; i++) {
		if (current->type != VDIR) {
			VOP_UNLOCK(current);
			vnode_deref(current);
			return YAK_NODIR;
		}

		int is_last = (i + 1 == n_comps);

		if (is_last && (flags & VFS_LOOKUP_PARENT)) {
			*out = current;
			if (last_comp)
				*last_comp = strdup(comp);
			return YAK_SUCCESS;
		}

		status_t res = VOP_LOOKUP(current, comp, &next);
		IF_ERR(res)
		{
			VOP_UNLOCK(current);
			vnode_deref(current);
			return res;
		}

		vnode_ref(next);
		VOP_LOCK(next);

		VOP_UNLOCK(current);
		vnode_deref(current);

		// follow mountpoints, symlinks ...
		current = resolve(next);
		if (next != current) {
			vnode_ref(current);
			VOP_LOCK(current);

			VOP_UNLOCK(next);
			vnode_deref(next);
		}

		if (is_last) {
			if (want_dir && current->type != VDIR) {
				VOP_UNLOCK(current);
				vnode_deref(current);
				return YAK_NODIR;
			}

			*out = current;

			if (last_comp)
				*last_comp = strdup(comp);

			return YAK_SUCCESS;
		}

		comp += strlen(comp) + 1;
	}

	VOP_UNLOCK(current);
	vnode_deref(current);
	return YAK_NOENT;
}
