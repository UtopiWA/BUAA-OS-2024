#include <lib.h>

int flag[256];
/*
char *strcat(char *dest, const char *src)
{
	char *tmp = dest;	 
	while (*dest)
		dest++;
	while ((*dest++ = *src++) != '\0')
		;
	return tmp;
}

int rmloop(char *path) {	
	int fd, type;
	struct Filefd *ffd;
	struct File *dir;

	fd = open(path, O_RDONLY);
	ffd = (struct Filefd *)num2fd(fd);
	dir = &(ffd->f_file);
	type = dir->f_type;
	if (type == FTYPE_REG) {
		return remove(path);
	} else { // 递归删除
		char name[MAXNAMELEN];
		strcpy(name, path);

		u_int nblock = dir->f_size / BLOCK_SIZE;
		for (int i = 0; i < nblock; i++) {
			void * blk;
			try(file_get_block(dir, i, &blk));
			struct File *files = (struct File *)blk;

			for (struct File *f = files; f < files + FILE2BLK; ++f) {
				if (f->f_name) {
					strcat(name, "/");
					strcat(name, f->f_name);
					rmloop(name);
				}
			}
		}
		return remove(path);
	}
	return 0;
}
*/
int remove_loop(char *path) { // imitate 'dir_lookup' in 'fs/fs.c'
	int fd, type;
	struct Filefd *ffd;

	if (flag['r']) {
		if ((fd = open(path, O_RDONLY)) >= 0) {
			rmloop(path); // 递归删除
		} else {
			if (flag['f']) {
				return 0;	
			} else {
				user_panic("rm: cannot remove '%s': No such file or directory", path);
			}
		}
	} else {
		if ((fd = open(path, O_RDONLY)) >= 0) {
			ffd = (struct Filefd *)num2fd(fd);
			type = ffd->f_file.f_type;	
			if (type == FTYPE_REG) {
				return remove(path);
			} else {
				user_panic("rm: cannot remove '%s': Is a directory", path);
			}
		} else {
			user_panic("rm: cannot remove '%s': No such file or directory", path);
		}
	}
	return 0;
}

int main(int argc, char **argv) {
	
	ARGBEGIN {
	case 'r':
	case 'f':
		flag[(u_char)ARGC()]++;
		break;
	}
	ARGEND
	
	remove_loop(argv[0]);

	return 0;
}
/*
#include <lib.h>
int main() {
	return 0;
}
*/
