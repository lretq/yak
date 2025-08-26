#define pr_fmt(fmt) "initrd: " fmt

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <nanoprintf.h>
#include <yak/panic.h>
#include <yak/fs/vfs.h>
#include <yak/status.h>
#include <yak/macro.h>
#include <yak/log.h>

#include <yak/elf.h>

enum [[gnu::packed]] tar_file_type {
	TAR_REG = '0',
	TAR_HARD,
	TAR_SYM,
	TAR_CHR,
	TAR_BLK,
	TAR_DIR,
};

struct [[gnu::packed]] tar_header {
	char filename[100];
	// octal:
	char mode[8];
	char uid[8];
	char gid[8];
	char filesize[12];
	char mtime[12];
	// not octal:
	uint64_t chksum;
	enum tar_file_type filetype;
	char linkname[100];
	char magic[6]; /* ustar then \0 */
	uint16_t ustar_version;
	char user_name[32];
	char group_name[32];
	uint64_t device_major;
	uint64_t device_minor;
	char filename_prefix[155];

	char _padding[12];
};

_Static_assert(sizeof(struct tar_header) == 512,
	       "Tar header size is incorrect");

static uint64_t decode_octal(char *data, size_t size)
{
	uint8_t *currentPtr = (uint8_t *)data + size;
	uint64_t sum = 0;
	uint64_t currentMultiplier = 1;

	/* find last \0 or space */
	uint8_t *checkPtr = currentPtr;
	for (; checkPtr >= (uint8_t *)data; checkPtr--) {
		if (*checkPtr == 0 || *checkPtr == ' ')
			currentPtr = checkPtr - 1;
	}
	/* decode the octal number */
	for (; currentPtr >= (uint8_t *)data; currentPtr--) {
		sum += (uint64_t)((*currentPtr) - 48) * currentMultiplier;
		currentMultiplier *= 8;
	}
	return sum;
}

void initrd_unpack_tar(const char *path, const char *data, size_t len)
{
	struct vnode *vn;
	size_t zero_filled = 0;

	size_t pos = 0;

	char pathbuf[4096];

	while (pos <= len) {
		if (zero_filled >= 2)
			break;

		struct tar_header *hdr = (struct tar_header *)(data + pos);
		pos += sizeof(struct tar_header);

		if (memcmp(hdr->magic, "ustar", 6) == 0) {
			panic("bad tar magic\n");
		}

		if (hdr->filetype == 0) {
			zero_filled++;
			continue;
		}

		npf_snprintf(pathbuf, sizeof(pathbuf), "%s/%s%s", path,
			     hdr->filename_prefix, hdr->filename);

		zero_filled = 0;

		switch (hdr->filetype) {
		case TAR_REG:
			EXPECT(vfs_create(pathbuf, VREG, &vn));

			size_t size = decode_octal(hdr->filesize,
						   sizeof(hdr->filesize));

			EXPECT(VOP_WRITE(vn, 0, (data + pos), &size));

			pos += ALIGN_UP(size, 512);
			break;

		case TAR_DIR:
			EXPECT(vfs_create(pathbuf, VDIR, &vn));

			break;

		default:
			break;
		}
	}

	pr_debug("unpack complete\n");
}
