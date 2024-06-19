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

		int cnt = 0;
		char name[32][MAXNAMELEN];
		strcpy(name[0], argv[0]);

		while (create(name[cnt], FTYPE_DIR) < 0) { // 向上层找到能创建的目录
			int len = strlen(name[cnt]);
			for (; len >= 0 && name[cnt][len] != '/'; len--)
				;
			if (len < 0) {
				return 0;
			}
			strcpy(name[cnt + 1], name[cnt]);
			name[++cnt][len] = '\0';		
		}
		cnt--;
		for (; cnt >= 0; cnt--) { // 再依次向下创建目录
			if (create(name[cnt], FTYPE_DIR) < 0) {
				return 0;
			}
		}

	}

	return 0;
}
