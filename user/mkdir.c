#include <lib.h>

int main(int argc, char **argv) {
	int fd, flag = 0;

	ARGBEGIN {
	case 'p':
		flag = 1;
		break;
	}
	ARGEND

	if (flag == 0) {
		if ((fd = open(argv[0], O_RDONLY)) >= 0) {
			user_panic("mkdir: cannot create directory '%s': File exists", argv[0]);
		}
		if (create(argv[0], FTYPE_DIR) < 0) {
			user_panic("mkdir: cannot create directory '%s': No such file or directory", argv[0]);
		}
	} else { // ignore the fault
		if ((fd = open(argv[0], O_RDONLY)) >= 0) {
			return 0;
		}
		if (create(argv[0], FTYPE_DIR) < 0) {
			// 递归创建目录
		}
	}

	return 0;
}
