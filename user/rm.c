#include <lib.h>

int flag[256];

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
