#include <yak/file.h>
#include <yak/mutex.h>
#include <yak/types.h>
#include <yak/heap.h>
#include <yak/refcount.h>
#include <yak/cpudata.h>
#include <yak/syscall.h>
#include <yak/log.h>
#include <yak/fs/vfs.h>
#include <yak/status.h>
#include <yak-abi/errno.h>
#include <yak-abi/seek-whence.h>
#include <yak-abi/fcntl.h>

DEFINE_SYSCALL(SYS_OPEN, open, char *filename, int flags, int mode)
{
	pr_debug("sys_open: %s %d %d\n", filename, flags, mode);
	struct kprocess *proc = curproc();

	unsigned int file_flags = 0;
	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		file_flags |= FILE_READ;
		break;
	case O_WRONLY:
		file_flags |= FILE_WRITE;
		break;
	case O_RDWR:
		file_flags |= FILE_READ | FILE_WRITE;
		break;
	default:
		return SYS_ERR(EINVAL);
	}

	struct vnode *vn;
	status_t res = vfs_open(filename, &vn);

	if (res == YAK_NOENT) {
		if (flags & O_CREAT) {
			RET_ERRNO_ON_ERR(vfs_create(filename, VREG, &vn));
		} else {
			// file does not exist
			return SYS_ERR(ENOENT);
		}
	} else {
		RET_ERRNO_ON_ERR(res);
	}

	guard(mutex)(&proc->fd_mutex);

	int fd;
	RET_ERRNO_ON_ERR(fd_alloc(proc, &fd));

	struct fd *desc = proc->fds[fd];
	// TODO: e.g. cloexec, ...

	struct file *file = desc->file;
	file->vnode = vn;
	file->offset = 0;
	file->flags = file_flags;

	return SYS_OK(fd);
}

DEFINE_SYSCALL(SYS_CLOSE, close, int fd)
{
	pr_debug("sys_close(%d)\n", fd);
	struct kprocess *proc = curproc();

	kmutex_acquire(&proc->fd_mutex, TIMEOUT_INFINITE);
	struct fd *desc = fd_safe_get(proc, fd);
	if (!desc) {
		kmutex_release(&proc->fd_mutex);
		return SYS_ERR(EBADF);
	}
	proc->fds[fd] = NULL;
	kmutex_release(&proc->fd_mutex);

	struct file *file = desc->file;

	kfree(desc, sizeof(struct fd));

	file_deref(file);

	return SYS_OK(0);
}

DEFINE_SYSCALL(SYS_DUP2, dup2, int oldfd, int newfd)
{
	pr_debug("sys_dup(%d %d)\n", oldfd, newfd);
	struct kprocess *proc = curproc();

	guard(mutex)(&proc->fd_mutex);

	struct fd *src_fd = fd_safe_get(proc, oldfd);

	if (src_fd == NULL) {
		return SYS_ERR(EBADF);
	} else if (oldfd == newfd) {
		return SYS_OK(0);
	}

	struct fd *dest_fd = NULL;

	if (newfd == -1) {
		RET_ERRNO_ON_ERR(fd_alloc(proc, &newfd));
	} else {
		if (newfd >= proc->fd_cap) {
			fd_grow(proc, newfd + 1);
		} else {
			dest_fd = fd_safe_get(proc, newfd);
			file_deref(dest_fd->file);
			dest_fd->file = NULL;
		}
	}

	dest_fd = proc->fds[newfd];
	assert(dest_fd != NULL);

	dest_fd->flags = src_fd->flags;
	dest_fd->file = src_fd->file;
	file_ref(src_fd->file);

	return SYS_OK(newfd);
}

DEFINE_SYSCALL(SYS_WRITE, write, int fd, const char *buf, size_t count)
{
	pr_extra_debug("sys_write: %d %p %ld\n", fd, buf, count);

	struct kprocess *proc = curproc();
	struct file *file;

	{
		guard(mutex)(&proc->fd_mutex);
		struct fd *desc = fd_safe_get(proc, fd);
		if (!desc) {
			return SYS_ERR(EBADF);
		}
		file = desc->file;
		file_ref(file);
	}

	guard_ref_adopt(file, file);

	if (!(file->flags & FILE_WRITE)) {
		return SYS_ERR(EBADF);
	}

	off_t offset = __atomic_load_n(&file->offset, __ATOMIC_SEQ_CST);
	size_t written = 0;
	status_t res = vfs_write(file->vnode, offset, buf, count, &written);
	RET_ERRNO_ON_ERR(res);
	__atomic_fetch_add(&file->offset, written, __ATOMIC_SEQ_CST);

	return SYS_OK(written);
}

DEFINE_SYSCALL(SYS_READ, read, int fd, char *buf, size_t count)
{
	pr_extra_debug("sys_read: %d %p %ld\n", fd, buf, count);

	struct kprocess *proc = curproc();
	struct file *file;

	{
		guard(mutex)(&proc->fd_mutex);
		struct fd *desc = fd_safe_get(proc, fd);
		if (!desc) {
			return SYS_ERR(EBADF);
		}
		file = desc->file;
		file_ref(file);
	}

	guard_ref_adopt(file, file);

	if (!(file->flags & FILE_READ)) {
		return SYS_ERR(EBADF);
	}

	off_t offset = __atomic_load_n(&file->offset, __ATOMIC_SEQ_CST);
	size_t delta = 0;
	status_t res = vfs_read(file->vnode, offset, buf, count, &delta);
	RET_ERRNO_ON_ERR(res);
	__atomic_fetch_add(&file->offset, delta, __ATOMIC_SEQ_CST);

	return SYS_OK(delta);
}

DEFINE_SYSCALL(SYS_SEEK, seek, int fd, off_t offset, int whence)
{
	struct kprocess *proc = curproc();
	guard(mutex)(&proc->fd_mutex);

	struct fd *desc = proc->fds[fd];
	if (!desc) {
		return SYS_ERR(EBADF);
	}

	struct file *file = desc->file;

	switch (whence) {
	case SEEK_SET:
		file->offset = offset;
		break;
	case SEEK_CUR:
		if (file->offset + offset < 0)
			return SYS_ERR(EINVAL);
		file->offset += offset;
		break;
	case SEEK_END:
		VOP_LOCK(file->vnode);
		file->offset = file->vnode->filesize + offset;
		VOP_UNLOCK(file->vnode);
		break;
	default:
		pr_warn("sys_seek(): unknown whence %d\n", whence);
		return SYS_ERR(EINVAL);
	}

	return SYS_OK(file->offset);
}

/* Implements fallocate like linux */
DEFINE_SYSCALL(SYS_FALLOCATE, fallocate, int fd, int mode, off_t offset,
	       off_t size)
{
	struct kprocess *proc = curproc();
	struct file *file;

	{
		guard(mutex)(&proc->fd_mutex);
		struct fd *desc = fd_safe_get(proc, fd);
		if (!desc) {
			return SYS_ERR(EBADF);
		}
		file = desc->file;
		file_ref(file);
	}

	guard_ref_adopt(file, file);

	if (!(file->flags & FILE_WRITE)) {
		return SYS_ERR(EBADF);
	}

	status_t rv = VOP_FALLOCATE(file->vnode, mode, offset, size);
	RET_ERRNO_ON_ERR(rv);
	return SYS_OK(0);
}
