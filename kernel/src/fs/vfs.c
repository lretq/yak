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

	IF_ERR(res)
	{
		assert(last_comp == NULL);
		return res;
	}

	size_t comp_size = strlen(last_comp) + 1;

	assert(parent);
	res = VOP_CREATE(parent, type, last_comp, &vn);

	VOP_UNLOCK(parent);
	vnode_deref(parent);

	if (out && IS_OK(res)) {
		*out = vn;
	}

	kfree(last_comp, comp_size);

	return res;
}

status_t vfs_symlink(char *link_path, char *dest_path, struct vnode **out)
{
	char *last_comp;
	struct vnode *parent, *vn;
	status_t rv = vfs_lookup_path(link_path, NULL, VFS_LOOKUP_PARENT,
				      &parent, &last_comp);
	IF_ERR(rv)
	{
		return rv;
	}

	size_t comp_size = strlen(last_comp) + 1;

	assert(parent);
	rv = VOP_SYMLINK(parent, last_comp, dest_path, &vn);

	VOP_UNLOCK(parent);
	vnode_deref(parent);

	if (out && IS_OK(rv)) {
		*out = vn;
	}

	kfree(last_comp, comp_size);

	return rv;
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
		   size_t length, size_t *writtenp)
{
	if (writtenp == NULL || vp->type == VDIR)
		return YAK_INVALID_ARGS;

	if (vp->ops->vn_write) {
		return vp->ops->vn_write(vp, offset, buf, length, writtenp);
	}

	struct vm_object *obj = vp->vobj;
	assert(obj);

	if (length == 0)
		return YAK_SUCCESS;

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

	*writtenp = written;
	return YAK_SUCCESS;
}

status_t vfs_read(struct vnode *vn, voff_t offset, void *buf, size_t length,
		  size_t *readp)
{
	if (readp == NULL || vn->type == VDIR)
		return YAK_INVALID_ARGS;

	if (vn->ops->vn_read) {
		return vn->ops->vn_read(vn, offset, buf, length, readp);
	}

	struct vm_object *obj = vn->vobj;
	assert(obj);

	if (length == 0)
		return YAK_SUCCESS;

	if (offset >= vn->filesize) {
		*readp = 0;
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

	*readp = read;
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

	res = VOP_OPEN(&vn);
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
	if (!path_ || *path_ == '\0')
		return YAK_INVALID_ARGS;

	size_t pathlen = strlen(path_);

	int want_dir = (path_[pathlen - 1] == '/');

	// don't resolve cwd, because we don't want
	// to access a newly mounted filesystem
	struct vnode *current = (path_[0] == '/') ? resolve(root_node) : cwd;
	assert(current);

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

		bool is_last = (i + 1 == n_comps);

		if (is_last && (flags & VFS_LOOKUP_PARENT)) {
			*out = current;
			if (last_comp)
				*last_comp = strndup(comp, strlen(comp) + 1);
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

		// follow mountpoints, symlinks ...
		struct vnode *resolved;
		resolved = resolve(next);
		if (next != resolved) {
			vnode_ref(resolved);
			VOP_LOCK(resolved);

			VOP_UNLOCK(next);
			vnode_deref(next);
		}

		next = resolved;

		if (next->type == VLNK) {
			pr_debug("lookup link\n");

			char *dest;
			status_t rl_rv = VOP_READLINK(next, &dest);
			VOP_UNLOCK(next);
			IF_ERR(rl_rv)
			{
				vnode_deref(next);
				return rl_rv;
			}

			struct vnode *destvn, *resolve_cwd;

			if (*dest == '/') {
				resolve_cwd = root_node;
			} else {
				resolve_cwd = current;
			}

			rl_rv = vfs_lookup_path(dest, resolve_cwd, 0, &destvn,
						NULL);

			kfree(dest, 0);

			vnode_deref(next);

			IF_ERR(rl_rv)
			{
				return rl_rv;
			}

			next = destvn;
		}

		vnode_deref(current);
		current = next;

		if (is_last) {
			if (want_dir && next->type != VDIR) {
				VOP_UNLOCK(next);
				vnode_deref(next);
				return YAK_NODIR;
			}

			*out = next;

			if (last_comp)
				*last_comp = strndup(comp, strlen(comp) + 1);

			return YAK_SUCCESS;
		}

		comp += strlen(comp) + 1;
	}

	VOP_UNLOCK(current);
	vnode_deref(current);
	return YAK_NOENT;
}

status_t vfs_ioctl(struct vnode *vn, unsigned long com, void *data)
{
	if (!vn->ops->vn_ioctl)
		return YAK_NOT_SUPPORTED;

	return VOP_IOCTL(vn, com, data);
}

#define MAX_PATH 1024
#define BUF_SIZE 8192

#include <nanoprintf.h>

void vfs_dump_rec(struct vnode *vn, const char *prefix)
{
	char buf[BUF_SIZE];
	size_t bytes_read;

	if (!vn || !vn->ops->vn_getdents) {
		pr_error("vnode does not have getdents\n");
		return;
	}

	status_t res = VOP_GETDENTS(vn, (struct dirent *)buf, sizeof(buf),
				    &bytes_read);
	if (res != YAK_SUCCESS) {
		pr_error("%s<failed to read dir>\n", prefix);
		return;
	}

	size_t offset = 0;
	while (offset < bytes_read) {
		struct dirent *d = (struct dirent *)(buf + offset);
		offset += d->d_reclen;

		// Skip "." and ".."
		if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
			continue;

		// Print entry
		printk(0, "%s  %s%s\n", prefix, d->d_name,
		       (d->d_type == DT_DIR) ? "/" : "");

		// Recurse if directory
		if (d->d_type == DT_DIR) {
			char new_prefix[128];
			npf_snprintf(new_prefix, sizeof(new_prefix), "%s  ",
				     prefix);
			struct vnode *child;
			VOP_LOOKUP(vn, d->d_name, &child);
			vfs_dump_rec(child, new_prefix);
		}
	}
}

void vfs_dump(const char *path)
{
	printk(0, "%s\n", path);
	vfs_dump_rec(VFS_GETROOT(root_node->mountedvfs), "");
}
