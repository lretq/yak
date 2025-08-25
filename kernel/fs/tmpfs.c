#define pr_fmt(fmt) "tmpfs: " fmt

#include <yak/heap.h>
#include <yak/queue.h>
#include <yak/hashtable.h>
#include <yak/fs/vfs.h>
#include <yak/macro.h>
#include <yak/status.h>
#include <yak/vm/page.h>
#include <yak/vm/pmm.h>
#include <yak/log.h>
#include <string.h>
#include <stddef.h>

struct tmpfs_node {
	struct vnode vnode;
	char *name;
	size_t name_len;

	size_t inode;

	struct hashtable children;

	struct page **pages;
	size_t pages_cap;
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

	if (hashtable_get(&parent_node->children, name) != NULL) {
		return YAK_EXISTS;
	}

	struct tmpfs_node *node = create_node(parent->vfs, type);
	if (!node) {
		return YAK_OOM;
	}
	node->name = strdup(name);
	node->name_len = strlen(name);

	status_t ret;
	IF_ERR((ret = hashtable_set(&parent_node->children, name, node, 0)))
	{
		return ret;
	};

	VOP_RETAIN(parent);

	*out = &node->vnode;
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

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

status_t tmpfs_write(struct vnode *vp, size_t offset, const char *buf,
		     size_t *count)
{
	struct tmpfs_node *node = (struct tmpfs_node *)vp;

	if (count == NULL || vp->type == VDIR)
		return YAK_INVALID_ARGS;

	if (*count == 0)
		return YAK_SUCCESS;

	size_t length = *count;

	size_t start_page = ALIGN_DOWN(offset, PAGE_SIZE) / PAGE_SIZE;
	size_t end_page = ALIGN_UP(offset + length, PAGE_SIZE) / PAGE_SIZE;

	VOP_LOCK(vp);

	if (node->pages_cap < end_page) {
		struct page **new_pages =
			kmalloc(sizeof(struct page *) * end_page);
		if (!new_pages)
			return YAK_OOM;

		size_t i;
		for (i = 0; i < node->pages_cap; i++) {
			new_pages[i] = node->pages[i];
		}

		for (; i < end_page; i++) {
			new_pages[i] = NULL;
		}

		struct page **old_pages = node->pages;
		size_t old_size = node->pages_cap * sizeof(struct page *);

		__atomic_store_n(&node->pages, new_pages, __ATOMIC_SEQ_CST);
		__atomic_store_n(&node->pages_cap, end_page, __ATOMIC_SEQ_CST);

		kfree(old_pages, old_size);
	}

	for (size_t i = start_page; i < end_page; i++) {
		if (!node->pages[i]) {
			struct page *pg = pmm_alloc_order(0);
			if (!pg) {
				return YAK_OOM;
			}
			page_zero(pg, 0);
			__atomic_store_n(&node->pages[i], pg, __ATOMIC_SEQ_CST);
		}
	}

	VOP_UNLOCK(vp);

	size_t page_index = offset >> PAGE_SHIFT;
	size_t page_offset = offset & (PAGE_SIZE - 1);

	const char *src = buf;
	size_t remaining = length;

	while (remaining > 0) {
		size_t copy_len = PAGE_SIZE - page_offset;
		if (copy_len > remaining)
			copy_len = remaining;

		struct page *pg = __atomic_load_n(&node->pages[page_index],
						  __ATOMIC_SEQ_CST);
		if (unlikely(!pg)) {
			panic("page should be available");
		}

		memcpy((char *)(page_to_mapped_addr(pg) + page_offset), src,
		       copy_len);

		remaining -= copy_len;
		src += copy_len;
		page_index++;
		page_offset = 0;
	}

	return YAK_SUCCESS;
}

status_t tmpfs_read(struct vnode *vp, size_t offset, char *buf, size_t *count)
{
	struct tmpfs_node *node = (struct tmpfs_node *)vp;

	if (count == NULL || vp->type == VDIR)
		return YAK_INVALID_ARGS;

	if (*count == 0)
		return YAK_SUCCESS;

	size_t length = *count;

	VOP_LOCK(vp);
	size_t filesize = __atomic_load_n(&node->pages_cap, __ATOMIC_SEQ_CST)
			  << PAGE_SHIFT;
	if (offset >= filesize) {
		// cannot read anything. File too short!
		length = 0;
	} else if (length > filesize - offset) {
		length = filesize - offset;
	}
	pr_debug("length: %ld to %ld\n", *count, length);
	VOP_UNLOCK(vp);

	size_t page_index = offset >> PAGE_SHIFT;
	size_t page_offset = offset & (PAGE_SIZE - 1);

	char *dst = buf;
	size_t remaining = length;

	while (remaining > 0) {
		size_t copy_len = PAGE_SIZE - page_offset;
		if (copy_len > remaining)
			copy_len = remaining;

		struct page *pg = __atomic_load_n(&node->pages[page_index],
						  __ATOMIC_SEQ_CST);
		if (pg) {
			memcpy(dst,
			       (char *)(page_to_mapped_addr(pg) + page_offset),
			       copy_len);
		} else {
			memset(dst, 0, copy_len);
		}

		remaining -= copy_len;
		dst += copy_len;
		page_index++;
		page_offset = 0;
	}

	*count = length;

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
	.vn_write = tmpfs_write,
	.vn_read = tmpfs_read,
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

	VOP_RETAIN(vn);
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
		node->pages = NULL;
		node->pages_cap = 0;
	}

	node->name = NULL;
	node->name_len = 0;
	node->inode = __atomic_fetch_add(&((struct tmpfs *)vfs)->seq_ino, 1,
					 __ATOMIC_RELAXED);

	VOP_INIT(&node->vnode, vfs, &tmpfs_vn_op, type);

	return node;
}
