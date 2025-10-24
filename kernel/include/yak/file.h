#pragma once

#include <yak/refcount.h>
#include <yak/mutex.h>

struct vnode;
struct kprocess;

struct file {
	struct vnode *vnode;
	struct kmutex lock;
	unsigned long refcnt;
	off_t offset;
	unsigned int flags;
};

struct fd {
	struct file *file;
	unsigned int flags;
};

void file_init(struct file *file);
DECLARE_REFMAINT(file);

status_t proc_alloc_fd(struct kprocess *proc, int *fd);
status_t proc_alloc_fd_at(struct kprocess *proc, int fd);
status_t proc_grow_fd_table(struct kprocess *proc, int new_cap);
int proc_get_next_fd(struct kprocess *proc);
