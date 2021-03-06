#define FUSE_USE_VERSION 26
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/mman.h>

#include <endian.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct memFS_super {
	uint8_t		res1[3];
	char		oemname[8];
	uint16_t	bytes_per_sector;
	uint8_t		sectors_per_cluster;
	uint16_t	reserved_sectors;
	uint8_t		fat_count;
	uint16_t	root_max_entries;
	uint16_t	total_sectors_small;
	uint8_t		media_info;
	uint16_t	sectors_per_fat_small;
	uint16_t	sectors_per_track;
	uint16_t	head_count;
	uint32_t	fs_offset;
	uint32_t	total_sectors;
	uint32_t	sectors_per_fat;
	uint16_t	fat_flags;
	uint16_t	version;
	uint32_t	root_cluster;
	uint16_t	fsinfo_sector;
	uint16_t	backup_sector;
	uint8_t		res2[12];
	uint8_t		drive_number;
	uint8_t		res3;
	uint8_t		ext_sig;
	uint32_t	serial;
	char		label[11];
	char		type[8];
	char		res4[420];
	uint16_t	sig;
} __attribute__ ((__packed__));

struct memFS_direntry {
	union {
		struct {
			char		name[8];
			char		ext[3];
		};
		char			nameext[11];
	};
	uint8_t		attr;
	uint8_t		res;
	uint8_t		ctime_ms;
	uint16_t	ctime_time;
	uint16_t	ctime_date;
	uint16_t	atime_date;
	uint16_t	cluster_hi;
	uint16_t	mtime_time;
	uint16_t	mtime_date;
	uint16_t	cluster_lo;
	uint32_t	size;
} __attribute__ ((__packed__));

#define memFS_ATTR_DIR	0x10
#define memFS_ATTR_LFN	0xf
#define memFS_ATTR_INVAL	(0x80|0x40|0x08)

struct memFS_direntry_lfn {
	uint8_t		seq;
	uint16_t	name1[5];
	uint8_t		attr;
	uint8_t		res1;
	uint8_t		csum;
	uint16_t	name2[6];
	uint16_t	res2;
	uint16_t	name3[2];
} __attribute__ ((__packed__));

#define memFS_LFN_SEQ_START	0x40
#define memFS_LFN_SEQ_DELETED	0x80
#define memFS_LFN_SEQ_MASK	0x3f

struct memFS {
	const char	*dev;
	int		fs;
	/* XXX add your code here */
};

struct memFS_search_data {
	const char	*name;
	int		found;
	struct stat	*st;
};

struct memFS memFS_info, *f = &memFS_info;
iconv_t iconv_utf16;

uid_t mount_uid;
gid_t mount_gid;
time_t mount_time;
size_t pagesize;



static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";
static const char *folder_path = "/arpit";

static void
memFS_init(const char *dev)
{
	iconv_utf16 = iconv_open("utf-8", "utf-16");
	mount_uid = getuid();
	mount_gid = getgid();
	mount_time = time(NULL);

	f->fs = open(dev, O_RDONLY);
	if (f->fs < 0)
		err(1, "open(%s)", dev);

	
}

static int
memFS_search_entry(void *data, const char *name, const struct stat *st, off_t offs)
{
	printf("memFS_search_entry : \n");
	struct memFS_search_data *sd = data;

	if (strcmp(sd->name, name) != 0)
		return (0);

	sd->found = 1;
	*sd->st = *st;

	return (1);
}

/*
 *	ENOENT : Directory does not exist 
 * 	Return file attributes. The data in the stat structure passed is filled in here
 */
static int
memFS_fuse_getattr(const char *path, struct stat *st) {
	
	printf("memFS_fuse_getattr : \n");
	
	int return_val = 0;
	
	memset(st , 0 , sizeof( struct stat ));
	if ( strcmp(path , "/") == 0 ) {
		st->st_mode	 = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else if ( strcmp(path , folder_path) == 0 ) {
		st->st_mode	 = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else if (strcmp(path , hello_path) == 0) {
		st->st_mode = S_IFREG | 0444;
		st->st_nlink = 1;
		st->st_size = strlen(hello_str);
	}
	else {
		return_val = -ENOENT;
	}
	
	return return_val;
	
}

/*
 *	ENOENT : Directory does not exist 
 * 	filler : The purpose of this function is to insert directory entries into 
 * 	the directory structure, which is also passed to your callback as buf.
 *
 * 	filler prototype : 
 * 		int fuse_fill_dir_t(void *buf, const char *name, const struct stat *stbuf, off_t off);
 * 	Arguments:
 *  	- buf
 * 		- the null-terminated filename
 * 		- the address of struct stat (or NULL if have none)
 * 		- the offset of the next directory entry.
 */
static int
memFS_fuse_readdir(const char *path, void *buf,
		  fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
			  
	printf("memFS_read_dir : \n");
	if( strcmp(path , "/") != 0 ) {
		return -ENOENT;
	}
	
	filler(buf , "." , NULL , 0);
	filler(buf , ".." , NULL , 0);
	filler(buf , hello_path + 1 , NULL , 0);
	filler(buf , folder_path + 1 , NULL , 0);
	
	return 0;
}

/*
 * 	This function checks whatever user is permitted to open the file
 * 	with flags given in the fuse_file_info structure.
 * 	Return values :
 * 		- EACCESS : requested permission isn't available, or 0 for success.
 * 		-  ENOENT : file/directory does not exists
 * 		-		0 : success and user can open the file
 */
static int
memFS_fuse_open(const char *path, struct fuse_file_info *fi) {
	
	printf("memFS_fuse_open : \n");
	if ( strcmp(path , hello_path) != 0 ) {
		return -ENOENT;
	}

	/*
	 * 	O_RDONLY : value is 0
	 * 	111 & 011 -> 011 -> TRUE so no access
	 * 	100 & 011 -> 000 -> FALSE 
	 */
	if( (fi->flags & 3) != O_RDONLY ) {
      return -EACCES;
	}
	
	return 0;
}

/*
 * 	This function reads sizebytes from the given file into the buffer buf
 * 	beginning offset bytes into the file.
 * 	Return value:
 * 		- the number of bytes transferred
 * 		- 0 if offset was at or beyond the end of the file.
 */
static int
memFS_fuse_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi) {

	printf("memFS_fuse_read : \n");
	size_t length = 0;
	
	if( strcmp(path, hello_path) != 0) {
		return -ENOENT;
	}
	
	length = strlen(hello_str);
	
	/*
	 * 	if offset = 50 but file has 40
	 * 	so condition is false so return 0 as no bytes read in buffer;
	 * 	otherwise
	 * 	eg 1: 10 + 70 > 40
	 * 		size = 40 - 10 = 30
	 * 	eg 2: 10 + 20 > 40
	 * 		size = size = 20
	 */
	
	if ( offset < length ) {
		if ( offset + size > length ) {
			size = length - offset;
		}
		memcpy( buf , hello_str + offset , size );
	}
	else {
		size = 0;
	}
	
	return size;

}

static int
memFS_opt_args(void *data, const char *arg, int key, struct fuse_args *oargs)
{
	printf("memFS_opt_args : \n");
	if (key == FUSE_OPT_KEY_NONOPT && !f->dev) {
		f->dev = strdup(arg);
		return (0);
	}
	return (1);
}

static struct fuse_operations memFS_ops = {
	.getattr = memFS_fuse_getattr,
	.readdir = memFS_fuse_readdir,
	.open	 = memFS_fuse_open,
	.read 	 = memFS_fuse_read,
};

int
main(int argc, char **argv)
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	/*fuse_opt_parse(&args, NULL, NULL, memFS_opt_args);

	if (!f->dev)
		errx(1, "missing file system parameter");

	memFS_init(f->dev);
	*/
	
	return (fuse_main(args.argc, args.argv, &memFS_ops, NULL));
}
