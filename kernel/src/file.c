#include <yak/heap.h>
#include <yak/log.h>
#include <yak/file.h>
#include <yak/process.h>
#include <yak/fs/vfs.h>

static void file_cleanup(struct file *file);
GENERATE_REFMAINT(file, refcnt, file_cleanup);

static void file_cleanup(struct file *file)
{
	pr_debug("cleanup file\n");
	vnode_deref(file->vnode);
	kfree(file, sizeof(struct file));
}
