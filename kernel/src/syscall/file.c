#include <yak/file.h>
#include <yak/mutex.h>
#include <yak/types.h>
#include <yak/heap.h>
#include <yak/cpudata.h>
#include <yak/syscall.h>
#include <yak/log.h>
#include <yak/fs/vfs.h>
#include <yak/status.h>
#include <yak-abi/errno.h>
#include <yak-abi/seek-whence.h>
#include <yak-abi/fcntl.h>

#define FD_LIMIT 65535
#define FD_GROW_BY 8

static int next_fd(struct kprocess *proc)
{
	for (int i = 0; i < proc->fd_cap; i++) {
		if (proc->fds[i] == NULL)
			return i;
	}
	return -1;
}

static status_t grow_fdtable(struct kprocess *proc)
{
	int old_cap = proc->fd_cap;
	int new_cap = old_cap + FD_GROW_BY;

	struct fd **old_fds = proc->fds;

	if (new_cap > FD_LIMIT) {
		return YAK_MFILE;
	}

	struct fd **table = kcalloc(new_cap, sizeof(struct fd *));
	if (!table) {
		return YAK_OOM;
	}

	for (int i = 0; i < old_cap; i++) {
		table[i] = old_fds[i];
	}

	proc->fds = table;
	proc->fd_cap = new_cap;

	kfree(old_fds, old_cap * sizeof(struct fd *));

	return YAK_SUCCESS;
}

static void file_init(struct file *file)
{
	kmutex_init(&file->lock, "file");
	file->offset = 0;
	file->refcnt = 1;

	file->vnode = NULL;
	file->flags = 0;
}

static status_t alloc_fd(struct kprocess *proc, int *fd)
{
	guard(mutex)(&proc->fd_mutex);

retry:
	*fd = next_fd(proc);
	if (*fd == -1) {
		status_t res = grow_fdtable(proc);
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
	fdp->file = f;

	pr_debug("Alloc'd fd %d\n", *fd);

	return YAK_SUCCESS;
}

DEFINE_SYSCALL(SYS_OPEN, open, char *filename, int flags, int mode)
{
	pr_debug("sys_open: %s %o %d\n", filename, flags, mode);
	struct kprocess *proc = curproc();

	struct vnode *vn;
	status_t res = vfs_open(filename, &vn);

	if (res == YAK_NOENT) {
		if (flags & O_CREAT) {
			RET_ERRNO_ON_ERR(vfs_create(filename, VREG, &vn));
		} else {
			// file does not exist
			return -ENOENT;
		}
	} else if (IS_ERR(res)) {
		return status_errno(res);
	}

	int fd;
	RET_ERRNO_ON_ERR(alloc_fd(proc, &fd));

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

	kfree(desc, sizeof(struct fd));

	file_deref(file);

	return 0;
}

DEFINE_SYSCALL(SYS_WRITE, write, int fd, const char *buf, size_t count)
{
	pr_debug("sys_write: %d %p %ld\n", fd, buf, count);

	struct kprocess *proc = curproc();
	kmutex_acquire(&proc->fd_mutex, TIMEOUT_INFINITE);
	struct fd *desc = proc->fds[fd];
	if (!desc) {
		kmutex_release(&proc->fd_mutex);
		return -EBADF;
	}
	struct file *file = desc->file;
	file_ref(file);
	kmutex_release(&proc->fd_mutex);

	struct vnode *vn = file->vnode;

	off_t offset = file->offset;

	size_t written = -1;
	status_t res = vfs_write(vn, offset, buf, count, &written);
	if (IS_ERR(res)) {
		return -EIO;
	}

	__atomic_fetch_add(&file->offset, count, __ATOMIC_SEQ_CST);

	file_deref(file);

	return written;
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

	size_t read = -1;
	status_t res = vfs_read(vn, offset, buf, count, &read);
	if (IS_ERR(res)) {
		return status_errno(res);
	}

	__atomic_fetch_add(&file->offset, count, __ATOMIC_SEQ_CST);

	// TODO: proper retain/release
	file->refcnt--;

	return read;
}

DEFINE_SYSCALL(SYS_SEEK, seek, int fd, off_t offset, int whence)
{
	struct kprocess *proc = curproc();
	guard(mutex)(&proc->fd_mutex);

	struct fd *desc = proc->fds[fd];
	if (!desc) {
		return -EBADF;
	}

	struct file *file = desc->file;

	switch (whence) {
	case SEEK_SET:
		file->offset = offset;
		break;
	case SEEK_CUR:
		if (file->offset + offset < 0)
			return -EINVAL;
		file->offset += offset;
		break;
	case SEEK_END:
		VOP_LOCK(file->vnode);
		file->offset = file->vnode->filesize + offset;
		VOP_UNLOCK(file->vnode);
		break;
	default:
		pr_warn("sys_seek(): unknown whence %d\n", whence);
		return -EINVAL;
	}

	return file->offset;
}
