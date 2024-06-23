#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()#`"

// challenge-shell
#define HISTFILESIZE 20
int histsum; // 当前历史指令数量
int hist_offTb[HISTFILESIZE]; // 历史指令长度偏移

// challenge-shell, for '&&' and '||'
int pre_status; // 前面部分的布尔值
int jump; // 是否跳过当前指令
int pre_token; // 前一个操作，'&&' 或 '||' 或 0

/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes ('\0'), so that the
 *   returned token is a null-terminated string.
 */
int _gettoken(char *s, char **p1, char **p2) {
	*p1 = 0;
	*p2 = 0;
	if (s == 0) {
		return 0;
	}

	while (strchr(WHITESPACE, *s)) {
		*s++ = 0;
	}
	if (*s == 0) {
		return 0;
	}

	if (*s == '\"') { // 引号
		*s++ = 0;
		*p1 = s;
		while (*s && *s != '\"') {
			s++;
		}
		*s++ = 0;
		*p2 = s;
		return 'w';
	}

	if (*s == '`') { // 反引号
		*s++ = 0;
		*p1 = s;
		while (*s && *s != '`') {
			s++;
		}
		*s++ = 0;
		*p2 = s;
		return '`';
	}

	if (strchr(SYMBOLS, *s)) {
		int t = *s;
		*p1 = s;
		*s++ = 0;
		*p2 = s;
		
		if (t == *s && (t == '>' || t == '&' || t == '|')) { // find ">>", "&&" or "||"
			*s++ = 0;
			*p2 = s;
			if (t == '>') {
				return 'a';
			} else if (t == '&') {
				return 'b';
			} else if (t == '|') {
				return 'c';
			}
		} else {
			return t;
		}
		return t;
	}

	*p1 = s;
	while (*s && !strchr(WHITESPACE SYMBOLS, *s)) {
		s++;
	}
	*p2 = s;
	return 'w';
}

int gettoken(char *s, char **p1) {
	static int c, nc;
	static char *np1, *np2;

	if (s) {
		nc = _gettoken(s, &np1, &np2);
		return 0;
	}
	c = nc;
	*p1 = np1;
	nc = _gettoken(np2, &np1, &np2);
	return c;
}

#define MAXARGS 128

void runcmd(char *s);

int parsecmd(char **argv, int *rightpipe) {
	int argc = 0;
	while (1) {
		char *t;
		int fd, r;
		int c = gettoken(0, &t);
		
		switch (c) {
		case 0:	
			if (jump) { // changed after fin '&&||'
				jump = 0;
				return 0;
			} else {
				return argc;
			}
			// return argc;
		case 'w':
			if (argc >= MAXARGS) {
				debugf("too many arguments\n");
				exit();
			}
			argv[argc++] = t;
			break;
		case '<':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: < not followed by word\n");
				exit();
			}
			// Open 't' for reading, dup it onto fd 0, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (1/3) */
			fd = open(t, O_RDONLY);
			if (fd < 0) {
				debugf("can't open '%s' for reading!", t);
				exit();
			}
			dup(fd, 0);
			close(fd);

			//user_panic("< redirection not implemented");

			break;
		case '>':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: > not followed by word\n");
				exit();
			}
			// Open 't' for writing, create it if not exist and trunc it if exist, dup
			// it onto fd 1, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (2/3) */
			fd = open(t, O_WRONLY | O_CREAT | O_TRUNC);
			if (fd < 0) {
				debugf("can't open '%s' for writing!", t);
				exit();
			}
			dup(fd, 1);
			close(fd);

			//user_panic("> redirection not implemented");

			break;
		case '|':;
			/*
			 * First, allocate a pipe.
			 * Then fork, set '*rightpipe' to the returned child envid or zero.
			 * The child runs the right side of the pipe:
			 * - dup the read end of the pipe onto 0
			 * - close the read end of the pipe
			 * - close the write end of the pipe
			 * - and 'return parsecmd(argv, rightpipe)' again, to parse the rest of the
			 *   command line.
			 * The parent runs the left side of the pipe:
			 * - dup the write end of the pipe onto 1
			 * - close the write end of the pipe
			 * - close the read end of the pipe
			 * - and 'return argc', to execute the left of the pipeline.
			 */
			int p[2];
			/* Exercise 6.5: Your code here. (3/3) */
			if (pipe(p) != 0) {
				debugf("fail to create a pipe!");
				exit();
			}
			if ((r = fork()) < 0) {
				debugf("fail to fork!");
				exit();
			}
			*rightpipe = r;
			if (r == 0) {
				dup(p[0], 0);
				close(p[0]);
				close(p[1]);
				return parsecmd(argv, rightpipe);
			} else {
				ipc_recv(0, 0, 0); // challenge-shell, added after fin '&&||'
				dup(p[1], 1);
				close(p[0]);
				close(p[1]);
				if (jump) { // changed after fin '&&||'
					jump = 0;
					return 0;
				} else {
					return argc;
				}
				// return argc;
			}

			//user_panic("| not implemented");

			break;
		case ';':
			if ((r = fork()) < 0) {
				debugf("fail to fork!");
				exit();
			}
			*rightpipe = r;
			if (r == 0) {
				if (jump) { // changed after fin '&&||'
					jump = 0;
					return 0;
				} else {
					return argc;
				}
				// return argc;
			} else {
				ipc_recv(0, 0, 0); // challenge-shell, added after fin '&&||'
				wait(r);
				close(0);
				close(1);
				dup(opencons(), 1);
				dup(1, 0);
				return parsecmd(argv, rightpipe);
			}
			break;
		case '#':
			if (jump) { // changed after fin '&&||'
				jump = 0;
				return 0;
			} else {
				return argc;
			}
			// return argc;
			break;
		case '`':;
			char cmd[1024];
			int i = 0;
			for (i = 0; *t; ) {
				cmd[i++] = *t++;
			}
			cmd[i] = 0;
			if (pipe(p) != 0) {
				debugf("fail to create a pipe!");
				exit();
			}
			if ((r = fork()) < 0) {
				debugf("fail to fork!");
				exit();
			}
			// *rightpipe = r;
			if (r == 0) {
				dup(p[1], 1);
				close(p[0]);
				close(p[1]);

				while (argc) {
					argv[--argc] = 0;
				}
				runcmd(cmd);
				exit();
			} else {
				ipc_recv(0, 0, 0); // added after fin '&&||'

				dup(p[0], 0);
				close(p[1]);

				char result[1024];
				int tot = 0, n = 0;

				while (n = read(p[0], result + tot, sizeof(result) - 1 - tot)) {
					tot += n;
				} // read pipe
				argv[argc++] = result;
				close(p[0]);
				wait(r);
			}
			break;
		case 'a': // >>
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: >> not followed by word\n");
				exit();
			}
			fd = open(t, O_WRONLY | O_CREAT | O_APPEND);
			if (fd < 0) {
				debugf("can't open '%s' for writing!", t);
				exit();
			}
			dup(fd, 1);
			close(fd);
			break;
		case 'b': // &&
			r = fork();
			if (r == 0) {
				if (jump) {
					return 0;
				} else {
					return argc;
				}
			} else {
				int son;
				if (jump) {
					jump = 0;
					son = 0;
				} else {
					son = ipc_recv(0, 0, 0);
				}

				if (pre_token == 0) {
					pre_status = (son == 0);
				} else if (pre_token == 'c' && pre_status == 0 && son == 0) {
					pre_status = 1;
				} else if (pre_token == 'b' && pre_status == 1 && son) {
					pre_status = 0;
				}

				pre_token = 'b';
				wait(r);
				jump = (pre_status == 0);
				*rightpipe = 0;
				dup(0, 1);
				dup(1, 0);
				return parsecmd(argv, rightpipe);
			}
			break;
		case 'c': // ||
			r = fork();
			if (r == 0) {
				if (jump) {
					return 0;
				} else {
					return argc;
				}
			} else {
				int son;
				if (jump) {
					jump = 0;
					son = 0;
				} else {
					son = ipc_recv(0, 0, 0);
				}

				if (pre_token == 0) {
					pre_status = (son == 0);
				} else if (pre_token == 'c' && pre_status == 0 && son == 0) {
					pre_status = 1;
				} else if (pre_token == 'b' && pre_status == 1 && son) {
					pre_status = 0;
				}

				pre_token = 'c';
				wait(r);
				jump = (pre_status == 1);
				*rightpipe = 0;
				dup(0, 1);
				dup(1, 0);
				return parsecmd(argv, rightpipe);
			}
			break;

		}
	}

	if (jump) { // changed after fin '&&||'
		jump = 0;
		return 0;
	} else {
		return argc;
	}
	// return argc;
}

void runcmd(char *s) {
	gettoken(s, 0);

	char *argv[MAXARGS];
	int rightpipe = 0;
	
	// challenge-shell
	pre_status = 1;
	jump = 0;
	pre_token = 0;

	int argc = parsecmd(argv, &rightpipe);
	if (argc == 0) {
		return;
	}
	argv[argc] = 0;

	int child = spawn(argv[0], argv);
	close_all();
	if (child >= 0) {
		wait(child);
	} else {
		debugf("spawn %s: %d\n", argv[0], child);
	}
	if (rightpipe) {
		wait(rightpipe);
	}
	exit();
}

void readline(char *buf, u_int n) {
	int r;
	// challenge-shell
	int len = 0;
	int histnow = histsum; // 当前查看的指令
	char display[128]; // 显示的指令

	for (int i = 0; i < n; i++) {
		if ((r = read(0, buf + i, 1)) != 1) {
			if (r < 0) {
				debugf("read error: %d\n", r);
			}
			exit();
		}

		// challenge-shell, key UDLR
		if (buf[i] == 27) {
			for (int j = 0; j < 2; j++) {
				i++;
				if ((r = read(0, buf + i, 1)) != 1) {
					if (r < 0) {
						debugf("read error: %d\n", r);
					}
					exit();
				}
			}

			if (buf[i] == 'A') { // up
				printf("%c[B", 27); // 防止光标向上移动
				if (histnow) {
					histnow--;
					gethist(display, histnow);
					strcpy(buf, display);
					for (int j = 0; j < len; j++) {
						printf("\b");
					}
					for (int j = 0; j < len; j++) {
						printf(" ");
					}
					for (int j = 0; j < len; j++) {
						printf("\b");
					}
					printf("%s", buf);
					len = strlen(buf);
					i = len;
				}
			} else if (buf[i] == 'B') { // down
				if (histnow < histsum) {
					histnow++;
					gethist(display, histnow);
					strcpy(buf, display);
					for (int j = 0; j < len; j++) {
						printf("\b");
					}
					for (int j = 0; j < len; j++) {
						printf(" ");
					}
					for (int j = 0; j < len; j++) {
						printf("\b");
					}
					printf("%s", buf);
					len = strlen(buf);
					i = len;
				}
			}
		}

		if (buf[i] == '\b' || buf[i] == 0x7f) {
			if (i > 0) {
				i -= 2;
			} else {
				i = -1;
			}
			if (buf[i] != '\b') {
				// printf("\b");
				printf("\b \b"); // changed
			}
		}
		if (buf[i] == '\r' || buf[i] == '\n') {
			buf[i] = 0;
			return;
		}
	}
	debugf("line too long\n");
	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
		;
	}
	buf[0] = 0;
}

char buf[1024];

void usage(void) {
	printf("usage: sh [-ix] [script-file]\n");
	exit();
}

// challenge-shell
char *strcat(char *dest, const char *src)
{
	char *tmp = dest;	 
	while (*dest)
		dest++;
	while ((*dest++ = *src++) != '\0')
		;
	return tmp;
}

void savehist(char *cmd) { // 保存历史指令
	int fd, r;
	if ((fd = open(".mosh_history", O_WRONLY | O_APPEND)) < 0) {
		user_panic("open .mosh_history failed\n");
	}
	if ((r = write(fd, cmd, strlen(cmd))) != strlen(cmd)
		|| (r = write(fd, "\n", 1)) != 1) {
		user_panic("error when writing .mosh_history\n");
	}
	hist_offTb[histsum++] = strlen(cmd) + 1 + (histsum > 0 ? hist_offTb[histsum - 1] : 0);	
	
	close(fd);
}

void gethist(char *cmd, int idx) { // 由偏移得到历史指令 
	int fd, r, offset, len;
	if ((fd = open(".mosh_history", O_RDONLY)) < 0) {
		user_panic("open .mosh_history failed\n");
	}
	if (idx >= histsum) {
		*cmd = 0;
		return;
	}
	offset = idx > 0 ? hist_offTb[idx - 1] : 0;
	len = hist_offTb[idx] - offset;

	if ((r = seek(fd, offset)) < 0) {
		user_panic("seek failed\n");
	}
	if ((r = readn(fd, cmd, len)) != len) {
		user_panic("error when reading .mosh_history");
	}
	
	close(fd);
	cmd[len - 1] = 0; // truncate the '\n'
}

int main(int argc, char **argv) {
	int r;
	int interactive = iscons(0);
	int echocmds = 0;
	printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	printf("::                                                         ::\n");
	printf("::                     MOS Shell 2024                      ::\n");
	printf("::                                                         ::\n");
	printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	// challenge-shell, create '.mosh_history'
	if ((r = open(".mosh_history", O_TRUNC | O_CREAT)) < 0) {
		return r;
	}
	histsum = 0;

	if (argc > 1) {
		usage();
	}
	if (argc == 1) {
		close(0);
		if ((r = open(argv[0], O_RDONLY)) < 0) {
			user_panic("open %s: %d", argv[0], r);
		}
		user_assert(r == 0);
	}
	for (;;) {
		if (interactive) {
			printf("\n$ ");
		}
		readline(buf, sizeof buf);
		savehist(buf); // challenge-shell

		if (buf[0] == '#') {
			continue;
		}
		if (echocmds) {
			printf("# %s\n", buf);
		}
		if ((r = fork()) < 0) {
			user_panic("fork: %d", r);
		}
		if (r == 0) {
			// challenge-shell
			if (strcmp(buf, "history") == 0) {
				char tmp[1024] = "cat < .mosh_";
				strcat(tmp, buf);
				runcmd(tmp);
			} else {
				runcmd(buf);
			}

			exit();
		} else {
			wait(r);
		}
	}
	return 0;
}
