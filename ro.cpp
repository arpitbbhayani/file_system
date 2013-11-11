#define FUSE_USE_VERSION 26

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
#include <map>
#include <list>
#include <string>
#include <iostream>
#include <libgen.h>

using namespace std;

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

/*
 * 	/
 * 	|
 * 	|____ file1 { Contents : "First file in root directory using fuse!\n" }
 * 	|
 * 	|____ directory1
 * 			|
 * 			|____ file2 { Contents : "file2 in directory1 using fuse!\n" }
 * 			|
 * 			|____ file3 { Contents : "file3 in directory1 using fuse!\n" }
 * 
 */

struct file_node {
	size_t 		size;
	uid_t 		uid;
	gid_t 		gid;
	time_t 		mtime;
	mode_t 		mode;
	nlink_t 	nlink; 
	string 		path;
	string 		name;
	string 		content;
};

map<string , file_node * > fs_file_dir;
map<string , list<string> > fs_dir;

void print_fs() {
	
	cout << "fs_dir : " << endl;
	
	typedef map<string, list<string> >::const_iterator MapIterator;
	
	for (MapIterator itr = fs_dir.begin(); itr != fs_dir.end(); itr++) {
		cout << "Key: " << itr->first << endl << "Values:" << endl;
		typedef list<string>::const_iterator ListIterator;
		for (ListIterator list_iter = itr->second.begin(); list_iter != itr->second.end(); list_iter++)
			cout << " " << *list_iter << endl;
	}
	
	
	cout << "fs_file_dir : " << endl;
	
	typedef map<string, file_node * >::const_iterator MapIterator1;
	
	for (MapIterator1 itr = fs_file_dir.begin(); itr != fs_file_dir.end(); itr++) {
		cout << "Key: " << itr->first << endl << "Values:" << endl;
		cout << " size : " << itr->second->size << endl;
		cout << " uid : " << itr->second->uid << endl;
		cout << " gid : " << itr->second->gid << endl;
		cout << " mtime : " << itr->second->mtime << endl;
		cout << " mode : " << itr->second->mode << endl;
		cout << " nlink : " << itr->second->nlink << endl;
		cout << " path : " << itr->second->path << endl;
		cout << " name : " << itr->second->name << endl;
		cout << " content : " << itr->second->content << endl;
	}
	
}

void add_directory( string dir_path ) {

	char * S = new char[dir_path.length() + 1];
	strcpy(S,dir_path.c_str());

	struct file_node * fn = new struct file_node;
	fn->size	= 2;
	fn->uid		= getuid();
	fn->gid		= getgid();
	fn->mtime	= time(NULL);
	fn->mode	= S_IFDIR | 0755;
	fn->nlink	= 2;
	fn->path	= dir_path;
	fn->name	= basename(S);
	
	fs_file_dir[dir_path] = fn;

	if ( dir_path != "/" ) {
		fs_dir[dirname(S)].push_back(dir_path);
	}
	
}

void add_file( string file_path , string file_contents ) {

	char * S = new char[file_path.length() + 1];
	strcpy(S , file_path.c_str());
	
	int size = file_contents.length() + 1;
	
	struct file_node * fn = new struct file_node;
	fn->size	= size-1;
	fn->uid		= getuid();
	fn->gid		= getgid();
	fn->mtime	= time(NULL);
	fn->mode	= S_IFREG | 0444;
	fn->nlink	= 1;
	fn->path	= file_path;
	fn->name	= basename(S);
	fn->content	= file_contents;
	
	fs_file_dir[file_path] = fn;
	fs_dir[dirname(S)].push_back(file_path);
}

void init_fs() {
	
	add_directory("/");
	add_file("/file1" , "First file in root directory using fuse!\n");
	add_directory("/directory1");
	add_file("/directory1/file2" , "file2 in directory1 using fuse!\n");
	add_file("/directory1/file3" , "file3 in directory1 using fuse!\n");
 
}

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
memFS_search_entry(void *data, const char *name, const struct stat *st, off_t offs) {
	
	struct memFS_search_data *sd = (struct memFS_search_data *) data;

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
	
	int return_val = 0;
	
	memset(st , 0 , sizeof( struct stat ));

	if ( fs_file_dir.find(path) != fs_file_dir.end() ) {
		/* Path exists */
		file_node * fn = fs_file_dir[path];
		
		st->st_mode		= fn->mode;
		st->st_nlink	= fn->nlink;
		st->st_uid		= fn->uid;
		st->st_gid		= fn->gid;
		st->st_atime	= fn->mtime;
		st->st_mtime	= fn->mtime;
		st->st_ctime	= fn->mtime;
		st->st_size		= fn->size;
	}
	else {
		/* Path does not exists */
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

	filler(buf , "." , NULL , 0);
	filler(buf , ".." , NULL , 0);
	
	if ( fs_dir.find(path) != fs_dir.end() ) {
		
		list<string> list_of_children = fs_dir[path];
		typedef list<string>::const_iterator ListIterator;
	
		for (ListIterator itr = list_of_children.begin(); itr != list_of_children.end(); itr++) {
			filler( buf, (fs_file_dir[*itr]->name).c_str() , NULL, 0);
		}
	}
	else {
		return -ENOENT;
	}
	
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
	
	int return_val = 0;
	
	if ( fs_file_dir.find(path) == fs_file_dir.end() ) {
		return_val = -EACCES;
	}
	
	/*
	 * 	O_RDONLY : value is 0
	 * 	111 & 011 -> 011 -> TRUE so no access
	 * 	100 & 011 -> 000 -> FALSE 
	 */
	/*if( (fi->flags & 3) != O_RDONLY ) {
      return -EACCES;
	}*/
	
	return return_val;
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

	size_t length = 0;
	
	if ( fs_file_dir.find(path) == fs_file_dir.end() ) {
		return -ENOENT;
	}
	
	/*
	 * 	if offset = 50 but file has 40
	 * 	so condition is false so return 0 as no bytes read in buffer;
	 * 	otherwise
	 * 	eg 1: 10 + 70 > 40
	 * 		size = 40 - 10 = 30
	 * 	eg 2: 10 + 20 > 40
	 * 		size = size = 20
	 */
	
	const char * file_content = fs_file_dir[path]->content.c_str();
	length = fs_file_dir[path]->content.length();
	
	if ( (size_t) offset < length ) {
		if ( offset + size > length ) {
			size = length - offset;
		}
		memcpy( buf , file_content + offset , size );
	}
	else {
		size = 0;
	}
	
	return size;

}

static int
memFS_opt_args(void *data, const char *arg, int key, struct fuse_args *oargs) {
	
	if (key == FUSE_OPT_KEY_NONOPT && !f->dev) {
		f->dev = strdup(arg);
		return (0);
	}
	return (1);
}

struct fuse_function:fuse_operations {
	fuse_function() {
		getattr	= memFS_fuse_getattr;
		readdir = memFS_fuse_readdir;
		open	= memFS_fuse_open;
		read 	= memFS_fuse_read;
	}
};

static struct fuse_function fuse_ops;

int main(int argc, char **argv) {
	
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	
	init_fs();

	/*
	 * print_fs();
	 */
	
	return (fuse_main(args.argc, args.argv, &fuse_ops, NULL));
}
