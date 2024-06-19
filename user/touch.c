#include <lib.h>

int main(int argc, char **argv) {
	int fd;

	if ((fd = open(argv[1], O_RDONLY)) >= 0) {
		return 0;
	}
	if(create(argv[1], FTYPE_REG) < 0) {
		user_panic("touch: cannot touch '%s': No such file or directory", argv[1]);
	}

	return 0;
}
