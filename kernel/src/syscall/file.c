#include "yak/file.h"
#include "yak/mutex.h"
#include <yak/heap.h>
#include <yak/cpudata.h>
#include <yak/syscall.h>
#include <yak/log.h>
#include <yak/fs/vfs.h>
#include <yak/status.h>
#include <yak-abi/errno.h>
#include <yak-abi/fcntl.h>

static int next_fd(struct kprocess *proc)
{
	for (int i = 0; i < proc->fd_cap; i++) {
		if (proc->fds[i] == NULL)
			return i;
	}
	return -1;
}

static void grow_fdtable(struct kprocess *proc)
{
	int new_cap = proc->fd_cap + 1;

	struct fd **old = proc->fds;
	int old_cap = proc->fd_cap;

	struct fd **table = kcalloc(new_cap, sizeof(struct fd *));
	for (int i = 0; i < old_cap; i++) {
		table[i] = old[i];
	}

	proc->fds = table;
	proc->fd_cap = new_cap;

	kfree(old, old_cap);
}

static void file_init(struct file *file)
{
	kmutex_init(&file->lock, "file");
	file->offset = 0;
	file->refcnt = 1;

	file->vnode = NULL;
	file->flags = 0;
}

static int alloc_fd(struct kprocess *proc)
{
	guard(mutex)(&proc->fd_mutex);

	int fd;

retry:
	fd = next_fd(proc);
	if (fd == -1) {
		grow_fdtable(proc);
		goto retry;
	}

	proc->fds[fd] = kmalloc(sizeof(struct fd));
	struct fd *fdp = proc->fds[fd];
	assert(fdp);

	struct file *f = kmalloc(sizeof(struct file));
	fdp->file = f;

	pr_debug("Alloc'd fd %d\n", fd);

	return fd;
}

DEFINE_SYSCALL(SYS_OPEN, open, char *filename, int flags, int mode)
{
	pr_debug("sys_open: %s %d %d\n", filename, flags, mode);
	struct kprocess *proc = curproc();

	struct vnode *vn;
	status_t status = vfs_open(filename, &vn);

	if (status == YAK_NOENT) {
		if (flags & O_CREAT) {
			status = vfs_create(filename, VREG, &vn);
			IF_ERR(status)
			{
				return -EINVAL;
			}
		} else {
			return -ENOENT;
		}
	} else if (IS_ERR(status)) {
		return -EINVAL;
	}

	int fd = alloc_fd(proc);
	struct file *file = proc->fds[fd]->file;
	file_init(file);

	file->vnode = vn;

	return fd;
}

DEFINE_SYSCALL(SYS_CLOSE, close, int fd)
{
	struct kprocess *proc = curproc();

	kmutex_acquire(&proc->fd_mutex, TIMEOUT_INFINITE);
	struct fd *desc = proc->fds[fd];
	proc->fds[fd] = NULL;
	kmutex_release(&proc->fd_mutex);

	struct file *file = desc->file;

	kfree(proc->fds[fd], sizeof(struct fd));

	file_deref(file);

	return 0;
}

DEFINE_SYSCALL(SYS_WRITE, write, int fd, const char *buf, size_t count)
{
	pr_debug("sys_write: %d %p %ld\n", fd, buf, count);

	struct kprocess *proc = curproc();
	kmutex_acquire(&proc->fd_mutex, TIMEOUT_INFINITE);
	struct file *file = proc->fds[fd]->file;
	file->refcnt++;
	kmutex_release(&proc->fd_mutex);

	struct vnode *vn = file->vnode;

	off_t offset = file->offset;

	status_t res = vfs_write(vn, offset, buf, &count);
	if (IS_ERR(res)) {
		return -EIO;
	}

	file->offset = offset + count;

	// TODO: proper retain/release
	file->refcnt--;

	return count;
}

DEFINE_SYSCALL(SYS_READ, read, int fd, char *buf, size_t count)
{
	pr_debug("sys_read: %d %p %ld\n", fd, buf, count);

	struct kprocess *proc = curproc();
	kmutex_acquire(&proc->fd_mutex, TIMEOUT_INFINITE);
	struct file *file = proc->fds[fd]->file;
	file->refcnt++;
	kmutex_release(&proc->fd_mutex);

	struct vnode *vn = file->vnode;

	off_t offset = file->offset;

	status_t res = vfs_read(vn, file->offset, buf, &count);
	if (IS_ERR(res)) {
		return status_errno(res);
	}

	file->offset = offset + count;

	// TODO: proper retain/release
	file->refcnt--;

	return count;
}
