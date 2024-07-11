# challenge-shell任务文档

## 实现不带 `.b` 后缀指令

主要是在`user/lib/spawn.c`中的`spawn`函数中修改，在其第一步使用文件名`prog`尝试打开文件时，如果无法打开，且文件名后缀不包含`.b`时，在文件名后添加`.b`后缀并再次尝试打开。
```C
	int fd;
	int len = strlen(prog);
	char tmp[MAXNAMELEN];
	if ((fd = open(prog, O_RDONLY)) < 0) {
		if (!(*(prog + len - 2) == '.' && *(prog + len - 1) == 'b') && len < MAXNAMELEN - 2 && fd == -E_NOT_FOUND) {
			int i = 0;
			while (prog[i]) {
				tmp[i++] = prog[i];
			}
			tmp[i++] = '.';
			tmp[i++] = 'b';
			tmp[i++] = '\0';
			prog = tmp;
		}
		if ((fd = open(prog, O_RDONLY)) < 0) {
			return fd;
		}
	}
```
## 实现指令条件执行

首先在`user/lib/libos.c`文件中修改，使得爷孙进程之间可以通信，以满足条件执行时信息传递：
```C
#define LIBENV 4

void libmain(int argc, char **argv) {
	// set env to point at our env structure in envs[].
	env = &envs[ENVX(syscall_getenvid())];
	// call user main routine
	main(argc, argv);
	// challenge-shell
	volatile struct Env *fa = &envs[ENVX(env->env_parent_id)];
	volatile struct Env *ee = env;
	int i = 0;
	int grand = fa->env_parent_id;
	
	int r = main(argc, argv);
	while (ee->env_parent_id) {
		ee = &envs[ENVX(ee->env_parent_id)]; 
		i++;
	}
	if (i != LIBENV) {
		ipc_send(grand, r, 0, 0);
	}
	// exit gracefully
	exit();
}
```
接下来全部在`user/sh.c`中修改：

首先添加一些全局变量：
```C
int pre_status; // 前面部分的布尔值
int jump; // 是否跳过当前指令
int pre_token; // 前一个操作，'&&' 或 '||' 或 0
```
然后在`runcmd`函数开头添加对全局变量的初始化：
```C
	pre_status = 1;
	jump = 0;
	pre_token = 0;
```
在实现追加重定向时已经完成`_gettoken`函数中对于`&&`和`||`的检测，只需在`parsecmd`函数中添加它们的处理。主要思路是通过爷孙进程的IPC通信来应对条件执行时跳过部分指令的处理，通过判断前面语句的布尔值来决定当前语句的处理。
```C
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
				return parsecmd(argv, rightpipe);
			}
			break;
```
接着在`parsecmd`的其他case中（主要是``; ` |``这三个符号的处理）添加父进程接收IPC的处理。并且将函数中所有返回`argc`的部分加入对`jump`变量的判断。例如符号`;`部分修改为：
```C
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
```
至此实现指令条件执行的全部功能。
## 实现更多指令

### `touch`

需要封装已经实现的`file_create`函数来实现`touch`指令和`mkdir`指令。具体修改如下：

在`fs/serv.c`中的`serve_table`中添加内容：
```C
void *serve_table[MAX_FSREQNO] = {
    // omitted ...
    [FSREQ_CREATE] = serve_create,
};
```
在`fs/serv.c`中添加`serve_create`函数，模仿`serve_open`即可：
```C
void serve_create(u_int envid, struct Fsreq_create *rq) {
	struct File *f;
	int r;
	if ((r = file_create(rq->req_path, &f)) < 0) {
		ipc_send(envid, r, 0, 0);
		return;
	}
	f->f_type = rq->f_type;
	ipc_send(envid, 0, 0, 0);
}
```
在`user/include/fsreq.h`中添加以下内容：
```C
enum {
	// omitted ...
	FSREQ_CREATE,
	MAX_FSREQNO,
};

struct Fsreq_create { //challenge-shell
	char req_path[MAXPATHLEN];
	u_int f_type;
};
```
在`user/include/lib.h`中添加：
```C
int fsipc_create(const char *, u_int);
```
在`user/lib/file.c`中添加：
```C
int create(const char *path, int type) {
	return fsipc_create(path, type);
}
```
在`user/lib/fsipc.c`中添加：
```C
int fsipc_create(const char *path, u_int f_type) {
	struct Fsreq_create *req;
	req = (struct Fsreq_create *)fsipcbuf;
	// The path is too long.
	if (strlen(path) >= MAXPATHLEN) {
		return -E_BAD_PATH;
	}
	strcpy((char *)req->req_path, path);
	req->f_type = f_type;
	return fsipc(FSREQ_CREATE, req, 0, 0);
}
```
最后实现`user/touch.c`：
```C
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
```

### `mkdir`

`mkdir`的前置部分与`touch`相同，区别仅在于传入`create`函数的`f_type`。实现`user/mkdir`即可：
```C
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
```

### `rm`

在`fs/serv.c`中添加：
```C
void *serve_table[MAX_FSREQNO] = {
// omitted ...
[FSREQ_RMLOOP] = serve_rmloop, // challenge-shell
};

void serve_rmloop(u_int envid, struct Fsreq_rmloop *rq) {
	int r;
	r = file_rmloop(rq->req_path);	
	ipc_send(envid, r, 0, 0);
}
```
在`fs/serv.h`中添加：
```C
int file_rmloop(char *path);
```
在`user/include/fsreq.h`中添加：
```C
enum {
	// omitted ...
	FSREQ_RMLOOP,
	MAX_FSREQNO,
};

struct Fsreq_rmloop { // challenge-shell
	char req_path[MAXPATHLEN];
};
```
在`user/include/lib.h`中添加：
```C
`int fsipc_rmloop(const char *);`
```
在`user/lib/file.c`中添加：
```C
int rmloop(const char *path) {
	return fsipc_rmloop(path);
}
```
在`user/lib/fsipc.c`中添加：
```C
int fsipc_rmloop(const char *path) {
	if (strlen(path) == 0 || strlen(path) >= MAXPATHLEN) {
		return -E_BAD_PATH;
	}
	struct Fsreq_rmloop *req = (struct Fsreq_rmloop *)fsipcbuf;
	strcpy(req->req_path, path);
	return fsipc(FSREQ_RMLOOP, req, 0, 0);
}
```
在`fs/fs.c`中实现处理函数，实现递归删除目录的功能。
```C
char *strcat(char *dest, const char *src)
{
	char *tmp = dest;	 
	while (*dest)
		dest++;
	while ((*dest++ = *src++) != '\0')
		;
	return tmp;
}
int file_rmloop(char *path) {
	int r;
	struct File *f;
	if ((r = walk_path(path, 0, &f, 0)) < 0) {
		return r;
	}
	if (f->f_type == FTYPE_DIR) { // 递归处理目录
		char name[MAXNAMELEN];
		strcpy(name, path);
		void *blk;
		u_int nblock = f->f_size / BLOCK_SIZE;
		for (int i = 0; i < nblock; i++) {
			try(file_get_block(f, i, &blk));
			struct File *files = (struct File *)blk;
			for (struct File *ff = files; ff < files + FILE2BLK; ++ff) {
				if (ff->f_name) {
					strcat(name, "/");
					strcat(name, ff->f_name);
					file_rmloop(name);
				}
			}
		}
	}
	return file_remove(path);
}
```
最后实现`user/rm.c`：
```C
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
```

## 实现反引号

在`user/sh.c`中进行两部分修改：

首先在`_gettoken`函数中添加处理反引号的部分：
```C
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
```
然后在`parsecmd`函数中添加处理反引号的部分。主要思路是先读出反引号包裹的字符串，fork出子进程用于处理该字符串。子进程清空父进程带来的参数，重新运行`runcmd`函数解析该字符串，解析完成后`exit`退出。另一方面，父进程等待子进程处理完毕后，从管道中读取整个字符串的结果并作为参数添加进`argv`数组，之后父进程继续执行。代码如下：
```C
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
```

## 实现注释功能

遇到井号`#`时，舍弃后面所有的信息，直接认为是该字符串的结尾。具体修改部分是在`user/sh.c`的全局变量`SYMBOLS`中添加井号`#`，并在`parsecmd`函数中的`switch`中添加井号的处理。
```C
		case '#':
			return argc;
			break;
```

## 实现历史指令

实现追加重定向时我们已经实现文件的追加打开模式，在`user/include/lib.h`中添加一行宏定义：
```C
#define O_APPEND 0x0004  /* challenge-shell, open for append redirection*/
```
其他部分都在`user/sh.c`中修改，以实现内建指令的目标。

首先在文件开头实现一些全局变量：
```C
#define HISTFILESIZE 20
int histsum; // 当前历史指令数量
int hist_offTb[HISTFILESIZE]; // 历史指令长度偏移
```
接下来实现保存历史指令和读取历史指令这两个关键函数：
```C
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
```
在`readline`函数开头定义若干变量：
```C
	int len = 0;
	int histnow = histsum; // 当前查看的指令
	char display[128]; // 显示的指令
```
接下来在`readline`函数中添加处理上键和下键的部分。我们知道点击上键时会依次输入`ascii=27,'[','A'`三个字符，而点击下键会依次输入`ascii=27,'[','B'`三个字符，所以按以下方式处理：每当读到ascii=27的字符时，向后再读2个字符，如果判定为上键或下键，就从历史指令文件中取出对应指令并输出。
```C
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
```
最后在`main`函数中实现剩余部分：

在函数开始添加创建`.mosh_history`文件并初始化全局变量的部分：
```C
if ((r = open(".mosh_history", O_TRUNC | O_CREAT)) < 0) {
		return r;
	}
	histsum = 0;
```
修改`r==0`情况下的处理：
```C
if (r == 0) {
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
```
这样就实现了历史指令。
## 实现一行多指令

每遇到分号`;`时，当前进程fork一个子进程，子进程处理分号左侧的命令，父进程等待子进程执行完毕后再继续执行。具体修改部分为`user/sh.c`文件中的`parsecmd`函数中的`switch`中添加分号的处理。需要注意的是，考虑到重定向的影响，需要在完成子进程后将输入输出改回标准输入输出。
```C
case ';':
			if ((r = fork()) < 0) {
				debugf("fail to fork!");
				exit();
			}
			*rightpipe = r;
			if (r == 0) {
				return argc;
			} else {
				wait(r);
				close(0);
				close(1);
				dup(opencons(), 1);
				dup(1, 0);
				return parsecmd(argv, rightpipe);
			}
			break;
```
## 实现追加重定向

首先在`user/include/lib.h`中添加一行宏定义，用于表示追加重定向的文件打开模式：
```user/include/lib.h
#define O_APPEND 0x0004  /* challenge-shell, open for append redirection*/
```
然后在`user/lib/file.c`的`open`函数末尾添加如下代码，用于处理追加重定向的情况，即设置文件偏移：
```C
// challenge-shell, append redirection
	if (mode & O_APPEND) {
		fd->fd_offset = size;
	}
```
然后在同文件下的`_gettoken`函数中修改原先的处理：
```C
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
```
最后在`user/sh.c`中的`parsecmd`函数添加处理部分，类似于已经实现的`>`：
```C
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
```

## 实现引号支持

主要在`user/sh.c`文件中的`_gettoken`函数中修改，具体思路是将引号内的内容解析为字符串。添加代码如下：
```C
/* 前面部分代码
if (*s == 0) {
		return 0;
}
*/

if(*s == '\"') { // 引号 
    *s++ = 0;    
    *p1 = s;  
    while(*s && *s != '\"') {  
        s++;  
    }  
    *s++ = 0;  
    *p2 = s;  
    return 'w';  
}
```

## 实现前后台任务管理

首先在文件`include/env.h`的`struct Env`中新增一个变量，用于标识后台变量。
```C
u_int env_back;
```
然后完成整套系统调用，用于修改该变量：

在`include/syscall.h`的`enum`中添加：
```C
SYS_set_env_back,
```
在`kern/env.c`的`env_create`中添加变量初始化：
```C
e->env_back = 0;
```
在`kern/syscall_all.c`的系统调用表`syscall_table`之前定义：
```C
void sys_set_env_back(u_int envid, u_int back) {
	struct Env *e;
	try(envid2env(envid , &e, 1));
	e->env_back = back;
}
```
同文件的系统调用表中添加：
```C
[SYS_set_env_back] = sys_set_env_back,
```
在`user/include/lib.h`中添加：
```C
void syscall_set_env_back(u_int envid, u_int back);
```
在`user/lib/syscall_lib.c`中添加：
```C
void syscall_set_env_back(u_int envid, u_int back) {
	return msyscall(SYS_set_env_back, envid, back);
}
```
至此实现完整的系统调用。

然后我们修改`user/lib/libos.c`文件，以兼容原先的指令条件执行功能：
```C
#include <env.h>
#include <lib.h>
#include <mmu.h>
void exit(void) {
	// After fs is ready (lab5), all our open files should be closed before dying.
#if !defined(LAB) || LAB >= 5
	close_all();
#endif
	syscall_env_destroy(0);
	user_panic("unreachable code");
}
const volatile struct Env *env;
extern int main(int, char **);
// challenge-shell
#define LIBENV 4
void libmain(int argc, char **argv) {
	// set env to point at our env structure in envs[].
	env = &envs[ENVX(syscall_getenvid())];
	// challenge-shell
	volatile struct Env *fa = &envs[ENVX(env->env_parent_id)];
	volatile struct Env *grd = &envs[ENVX(fa->env_parent_id)]; // added after fin bg
	volatile struct Env *ee = env;
	int i = 0;
	int grand = fa->env_parent_id;
	int r = main(argc, argv);
	while (ee->env_parent_id) {
		ee = &envs[ENVX(ee->env_parent_id)]; 
		i++;
		if(i>10) break;
	}
	int back = ee->env_back + fa->env_back + grd->env_back; // added after fin bg
	if (i != LIBENV && back == 0) { // changed after fin bg
		ipc_send(grand, r, 0, 0);
	}
	// exit gracefully
	exit();
}
```
接下来在`user/sh.c`中修改：

添加全局变量：
```C
#define MAXJOBNUM 16
static int job_id = 1;
static int job_envid[64];
static char job_status[20][64];
static char job_cmd[20][1024];
```
在`parsecmd`中添加`&`的处理：
```C
		case '&':
			r = fork();
			struct Env *tmp = &envs[ENVX(syscall_getenvid())];
			if (r == 0){
				if (jump) {
					const volatile syscall_set_env_back(tmp->env_id, 1);
					jump = 0;
					return 0;
				}
				return argc;
			} else {
				const volatile syscall_set_env_back(tmp->env_id, 1);
				ipc_send(tmp->env_parent_id, r, 0, 0);
				*rightpipe = 0;
				return parsecmd(argv, rightpipe);
			}
			break;
```
在`main`函数中实现`jobs fg kill`三个函数。添加在解析完`buf`之后。
```C
		if (buf[0] == 'j' && buf[1] == 'o' && buf[2] == 'b' && buf[3] == 's') {
			const volatile struct Env *tmp;
			for (int j = 1; j < job_id; j++) {
				tmp = &envs[ENVX(job_envid[j])];
				if (tmp->env_status == ENV_FREE) {
					strcpy(job_status[j], "Done");
				}
				printf("[%d] %-10s 0x%08x %s\n", j, job_status[j], job_envid[j], job_cmd[j]);
			}
			continue;
		}
		if (buf[0] == 'f' && buf[1] == 'g') {
			int len = strlen(buf) - 1;
			int sum = 0;
			const volatile struct Env *tmp;
			for (int t = 3; t <= len; t++) {
				sum = sum * 10 + (buf[t] - '0');
			}
			if (sum >= job_id) {
				printf("fg: job (%d) do not exist\n", sum);
				continue;
			}
			
			tmp = &envs[ENVX(job_envid[sum])];
			if (tmp->env_status == ENV_FREE) {
				strcpy(job_status[sum], "Done");
				printf("fg: (0x%08x) not running\n", job_envid[sum]);
				continue;
			} else {
				wait(job_envid[sum]);
				continue;
			}
		}
		if (buf[0] == 'k' && buf[1] == 'i' && buf[2] == 'l' && buf[3] == 'l') {
			int len = strlen(buf) - 1;
			int sum = 0;
			const volatile struct Env *tmp;
			for (int t = 5; t <= len; t++) {
				sum = sum * 10 + (buf[t] - '0');
			}
			if (sum >= job_id) {
				printf("fg: job (%d) do not exist\n", sum);
				continue;
			}
			tmp = &envs[ENVX(job_envid[sum])];
			if (tmp->env_status == ENV_FREE) {
				strcpy(job_status[sum], "Done");
				printf("fg: (0x%08x) not running\n", job_envid[sum]);
				continue;
			} else {
				syscall_env_destroy(job_envid[sum]);
				continue;
			}
		}
```
然后在`fork`之后，在父进程中如下修改，以实现判断后台指令和对应处理：
```C
		if (r == 0) {
			// omitted ... 
		} else { // changed after fin fg
			int len = strlen(buf) - 1;
			int flag = 0;	
			for (; len >= 0; len--) {
				if (strchr(WHITESPACE, buf[len])) {
					continue;
				}
				flag = (buf[len] == '&');
				break;
			}
			if (flag) {
				struct Env *tmp = &envs[ENVX(syscall_getenvid())];
				int rr = ipc_recv(0, 0, 0);
				wait(r);
				if (rr) {
					strcpy(job_cmd[job_id], buf);
					job_envid[job_id] = rr;
					strcpy(job_status[job_id], "Running");
					job_id++;
				}
			}
			wait(r);
		}
```
然后，给定的`kern/syscall_all.c`的部分系统调用函数有问题，修改如下：
```C
int sys_cgetc(void) {
	int ch;
	ch = scancharc();
	return ch;
}

int sys_env_destroy(u_int envid) {
	// omitted ...
	try(envid2env(envid, &e, 0));
	// omitted ...
}
```
至此就实现了后台指令的全部内容。