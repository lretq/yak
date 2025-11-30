#include <assert.h>
#include <yak/heap.h>
#include <yak/log.h>
#include <yak/status.h>
#include <yak/file.h>
#include <yak/process.h>
#include <yak/fs/vfs.h>

#define FD_LIMIT 65535
#define FD_GROW_BY 4

static void file_cleanup(struct file *file);
GENERATE_REFMAINT(file, refcnt, file_cleanup);

static void file_cleanup(struct file *file)
{
	vnode_deref(file->vnode);
	kfree(file, sizeof(struct file));
}

int fd_getnext(struct kprocess *proc)
{
	for (int i = 0; i < proc->fd_cap; i++) {
		if (proc->fds[i] == NULL)
			return i;
	}
	return -1;
}

status_t fd_grow(struct kprocess *proc, int new_cap)
{
	int old_cap = proc->fd_cap;
	if (new_cap < old_cap)
		return YAK_SUCCESS;

	struct fd **old_fds = proc->fds;

	if (new_cap > FD_LIMIT) {
		return YAK_MFILE;
	}

	struct fd **table = kcalloc(new_cap, sizeof(struct fd *));
	if (!table)
		return YAK_OOM;

	for (int i = 0; i < old_cap; i++)
		table[i] = old_fds[i];

	proc->fds = table;
	proc->fd_cap = new_cap;

	kfree(old_fds, old_cap * sizeof(struct fd *));

	return YAK_SUCCESS;
}

void file_init(struct file *file)
{
	kmutex_init(&file->lock, "file");
	file->offset = 0;
	file->refcnt = 1;

	file->vnode = NULL;
	file->flags = 0;
}

status_t fd_alloc_at(struct kprocess *proc, int fd)
{
	status_t rv = fd_grow(proc, fd + 1);
	IF_ERR(rv) return rv;

	proc->fds[fd] = kmalloc(sizeof(struct fd));
	struct fd *fdp = proc->fds[fd];
	assert(fdp);

	struct file *f = kmalloc(sizeof(struct file));
	fdp->file = f;

	file_init(f);

	return YAK_SUCCESS;
}

status_t fd_alloc(struct kprocess *proc, int *fd)
{
retry:
	*fd = fd_getnext(proc);
	if (*fd == -1) {
		status_t res = fd_grow(proc, proc->fd_cap + FD_GROW_BY);
		IF_ERR(res)
		{
			return res;
		}
		goto retry;
	}

	proc->fds[*fd] = kmalloc(sizeof(struct fd));
	struct fd *fdp = proc->fds[*fd];
	assert(fdp);

	struct file *f = kmalloc(sizeof(struct file));
	assert(f);
	fdp->file = f;

	file_init(f);

	pr_debug("Alloc'd fd %d\n", *fd);

	return YAK_SUCCESS;
}

struct fd *fd_safe_get(struct kprocess *proc, int fd)
{
	if (fd > proc->fd_cap) {
		return NULL;
	}

	return proc->fds[fd];
}
