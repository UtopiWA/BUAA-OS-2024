#include <args.h>
#include <lib.h>
#define MAXHISNUM 20         // 最大历史记录数量
int hislen;                  // 当前历史记录数量
int his_offsetTb[MAXHISNUM]; // 每条历史指令在history文件中的偏移
#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"
#define MYSYMBOLS "#"
// 保存历史指令
static int job_id = 1;
static char status[20][50];
static char cmd[20][1024];
static int job_envid[50];

void savecmd(char *cmd)
{
    int fd;
    int r;
    if ((fd = open("/.history", O_WRONLY | O_APPEND)) < 0)
    {
        user_panic("open .history failed");
    }
    if ((r = write(fd, cmd, strlen(cmd))) != strlen(cmd))
    {
        user_panic("write error .history: %d\n", r);
    }
    if ((r = write(fd, "\n", 1)) != 1)
    {
        user_panic("write error .history: %d\n", r);
    }
    his_offsetTb[hislen++] = strlen(cmd) + 1 + (hislen > 0 ? his_offsetTb[hislen - 1] : 0);
    close(fd);
}
// 通过index得到对应的历史指令内容
void gethis(int index, char *cmd)
{
    int fd;
    int r;
    if ((fd = open("/.history", O_RDONLY)) < 0)
    {
        user_panic("open .history failed");
    }
    if (index >= hislen)
    {
        *cmd = 0;
        return;
    }
    int offset = (index > 0 ? his_offsetTb[index - 1] : 0);
    int len = (index > 0 ? his_offsetTb[index] - his_offsetTb[index - 1] : his_offsetTb[index]);
    if ((r = seek(fd, offset)) < 0)
    {
        user_panic("seek failed");
    }
    if ((r = readn(fd, cmd, len)) != len)
    {
        user_panic("read history failed");
    }
    close(fd);
    cmd[len - 1] = 0;
}
// 就是一strcat，定义在这里方便使用
char *strcat(char *dest, const char *src)
{
    char *tmp = dest;
    while (*dest)
        dest++;
    while ((*dest++ = *src++) != '\0')
        ;
    return tmp;
}

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
int _gettoken(char *s, char **p1, char **p2)
{
    *p1 = 0;
    *p2 = 0;
    if (s == 0)
    {
        return 0;
    }

    while (strchr(WHITESPACE, *s))
    {
        *s++ = 0;
    }
    if (*s == 0)
    {
        return 0;
    }

    if (*s == '\"')
    {
        *s++ = 0;
        *p1 = s;
        while (*s && *s != '\"')
        {
            s++;
        }
        *s++ = 0;
        *p2 = s;
        return 'w';
    }

    if (*s == '`')
    {
        *s = 0;
        *p1 = ++s;
        while (*s && *s != '`')
        {
            s++;
        }

        *s = 0;
        *p2 = ++s;
        return '`';
    }
    

    if (*s == '#')
    {
        *p1 = s;
        *s++ = 0;
        *p2 = s;
        return '#';
    }
    if (*s == '>' && *(s + 1) == '>')
    {
        *p1 = s;
        *s = 0;
        s += 2;
        *p2 = s;
        return 't'; // 't' for >>
    }

    if (*s == '&' && *(s + 1) == '&')
    {
        *p1 = s;
        *s = 0;
        s += 2;
        *p2 = s;
        return 'a'; // 'a' for &&
    }

     if (*s == '|' && *(s + 1) == '|')
    {
        *p1 = s;
        *s = 0;
        s += 2;
        *p2 = s;
        return 'o'; // 'o' for ||
    }
  


    if (strchr(SYMBOLS, *s))
    {
        int t = *s;
        *p1 = s;
        *s++ = 0;
        *p2 = s;
        return t;
    }

    *p1 = s;
    while (*s && !strchr(WHITESPACE SYMBOLS MYSYMBOLS, *s))
    {
        s++;
    }
    *p2 = s;
    return 'w';
}

int gettoken(char *s, char **p1)
{
    static int c, nc;
    static char *np1, *np2;

    if (s)
    {
        nc = _gettoken(s, &np1, &np2);
        return 0;
    }
    c = nc;
    *p1 = np1;
    nc = _gettoken(np2, &np1, &np2);
    return c;
}

#define MAXARGS 128
int before_status=1;
int jump=0;
int before_s='\0';
int parsecmd(char **argv, int *rightpipe)
{
   int argc=0;
    while (1)
    {
        char *t;
        int fd, r;
        int c = gettoken(0, &t);
        switch (c)
        {
        case 0:
	   if(jump==1){jump=0; return 0;}
	   else  return argc;
        case '#':
	   if(jump==1){jump=0; return 0;}
	   else return argc;
            break;
        case'&':
	    r=fork();
	    if (r == 0)
             {
		struct Env* temp_env = &envs[ENVX(syscall_getenvid())];
               syscall_set_env_back(temp_env->env_id,1);
                     if(jump==1){jump=0; return 0;}
                     else return argc;
             }
	    else{
		     struct Env* temp_env = &envs[ENVX(syscall_getenvid())];
		     ipc_send(temp_env->env_parent_id, r, 0, 0);
		 *rightpipe = 0;
                 dup(0, 1);
                 dup(1, 0);
                 return parsecmd(argv, rightpipe);

	    }

	    break;
        case ';':
            r = fork();
            if (r == 0)
            {
		    if(jump==1){jump=0; return 0;}
		    else return argc;
            }
            else
            {
		ipc_recv(0,0,0);
                wait(r);
                *rightpipe = 0;
                dup(0, 1);
                dup(1, 0);
                return parsecmd(argv, rightpipe);
            }
            break;
	case 'a':
            r = fork();
            if (r == 0)
            {
	if(jump==1){ return 0;}
	    	    else return argc;
            }
            else
            {
		int son;
		if(jump==0) son = ipc_recv(0,0,0);
		else {jump=0;son = 0;}
		if(before_s=='\0'){
			if(son==0){
				before_status=1;//true
		        }
			else before_status=-1;
		}
		else if(before_s=='o'){
			if(before_status==-1&&son==0)
				before_status=1;

		}
		else if(before_s=='a'){
			if(before_status==1&&son!=0)
				before_status=-1;
		}
		before_s='a';
	        wait(r);
		if(before_status==-1) jump=1;
		else jump=0;
                
                *rightpipe = 0;
                 dup(0, 1);
                 dup(1, 0);
                 return parsecmd(argv, rightpipe);
             }
            break;

        case 'o':
            r = fork();
            if (r == 0)
            {
		    if(jump==1){ return 0;}
		else  return argc;
            }
            else
            {
		 int son;
		 if(jump==0) son=ipc_recv(0,0,0);
		 else {jump=0;son=0;}
		 if(before_s=='\0'){
                        if(son==0){
                                before_status=1;//true
                        }
                        else before_status=-1;
                }
                else if(before_s=='o'){
                        if(before_status==-1&&son==0)
                                before_status=1;

                }
                else if(before_s=='a'){
                        if(before_status==1&&son!=0)
                                before_status=-1;
                }
                before_s='o';
		 //
		 //
		wait(r);
		if(before_status==1) jump=1;
		else jump=0;
                *rightpipe = 0;
                 dup(0, 1);
                 dup(1, 0);
		 return parsecmd(argv, rightpipe);
                
            }
            break;

        case 'w':
            if (argc >= MAXARGS)
            {
                debugf("too many arguments\n");
                exit();
            }
            argv[argc++] = t;
            break;
        case '<':
            if (gettoken(0, &t) != 'w')
            {
                debugf("syntax error: < not followed by word\n");
                exit();
            }

            fd = open(t, O_RDONLY);
            if (fd < 0)
            {
                debugf("failed to open %s", t);
                exit();
            }
            dup(fd, 0);
            close(fd);

            break;
        case 't':
            if (gettoken(0, &t) != 'w')
            {
                debugf("syntax error: >> not followed by word\n");
                exit();
            }
            fd = open(t, O_WRONLY | O_CREAT | O_APPEND); // O_APPEND为追加写
            if (fd < 0)
            {
                debugf("failed to open %s", t);
                exit();
            }
            dup(fd, 1);
            close(fd);
            break;
        case '>':
            if (gettoken(0, &t) != 'w')
            {
                debugf("syntax error: > not followed by word\n");
                exit();
            }
            fd = open(t, O_WRONLY);
            if (fd < 0)
            {
                debugf("failed to open %s", t);
                exit();
            }
            dup(fd, 1);
            close(fd);
            // user_panic("> redirection not implemented");

            break;
        //++
        case '`':;
            char s[1024];
            int i;
            for (i = 0; *t;)
                s[i++] = *t++;
            s[i] = 0;
            int p[2];
            pipe(p);
            r = fork();
            if (r == 0)
            {                 
                dup(p[1], 1); 
                close(p[1]);
                close(p[0]);
                runcmd(s); 
                exit();
            }
            else if (r > 0)
            {   ipc_recv(0,0,0);
                dup(p[0], 0); 
                close(p[1]);
                char out[1024];
                while (read(p[0], out + strlen(out), 1024))
                    ; 
                int len = strlen(out);
                out[len - 1] = 0;   // 
                argv[argc++] = out; // 
                close(p[0]);
                wait(r);
            }
            break;
        case '|':;
            pipe(p);
            *rightpipe = fork();
            if (*rightpipe == 0)
            {
                dup(p[0], 0);
                close(p[0]);
                close(p[1]);
                return parsecmd(argv, rightpipe);
            }
            else if (*rightpipe > 0)
            {
		ipc_recv(0,0,0);
                dup(p[1], 1);
                close(p[1]);
                close(p[0]);
		if(jump==1){jump=0; return 0;}
		else return argc;
            }
            break;
        }
    }
	if(jump==1){jump=0; return 0;}
	else return argc;
}
void runcmd(char *s)
{
    gettoken(s, 0);

    char *argv[MAXARGS];
    int rightpipe = 0;
    before_status=1;
    jump=0;
    before_s='\0';
    int argc = parsecmd(argv, &rightpipe);
    if (argc == 0)
    {
        return;
    }
    argv[argc] = 0;

    int child = spawn(argv[0], argv);
    close_all();
    if (child >= 0)
    {
        wait(child);
    }
    else
    {
        debugf("spawn %s: %d\n", argv[0], child);
    }
    if (rightpipe)
    {
        wait(rightpipe);
    }
    exit();
}

void readline(char *buf, u_int n)
{
    int r;
    int hisindex = hislen; // 
    char histmp[128];      //
    int len = 0;           // 
    
    for (int i = 0; i < n; i++)
    {
        if ((r = read(0, buf + i, 1)) != 1)
        {
            if (r < 0)
            {
                debugf("read error: %d\n", r);
            }
            exit();
        }
        if (buf[i] == '\b' || buf[i] == 0x7f)
        {
	    int temp = i;
            if (i > 0)
            {
                i -= 2;
            }
            else
            {
                i = -1;
            }
	    if(temp > 0)
            if (buf[i] != '\b')
            {
                printf("\b \b");
            }
        }
        if (buf[i] == '\r' || buf[i] == '\n')
        {
            buf[i] = 0;
            return;
        }
        if (buf[i] == 27)
        {
            if ((r = read(0, buf + i, 1)) != 1)
            {
                if (r < 0)
                {
                    debugf("read error: %d\n", r);
                }
                exit();
            }
            if ((r = read(0, buf + i, 1)) != 1)
            {
                if (r < 0)
                {
                    debugf("read error: %d\n", r);
                }
                exit();
            }
            if (buf[i] == 'A')
            {                       // 上
                printf("%c[B", 27); // 不许乱动
                if (hisindex > 0)
                {
                    hisindex--;
                    gethis(hisindex, histmp);
                    strcpy(buf, histmp);
                    char flush[len];
                    for (int j = 0; j < len; j++)
                        flush[j] = ' ';
                    flush[len] = 0;
                    int len1 = len;
                    while (len--)
                        printf("\b");
                    printf("%s", flush);
                    while (len1--)
                        printf("\b");
                    printf("%s", buf);
                    len = strlen(buf);
                    i = len;
                }
            }
            else if (buf[i] == 'B')
            { // 下
                if (hisindex < hislen)
                {
                    hisindex++;
                    gethis(hisindex, histmp);
                    strcpy(buf, histmp);
                    char flush[len];
                    for (int j = 0; j < len; j++)
                        flush[j] = ' ';
                    flush[len] = 0;
                    int len1 = len;
                    while (len--)
                        printf("\b");
                    printf("%s", flush);
                    while (len1--)
                        printf("\b");
                    printf("%s", buf);
                    len = strlen(buf);
                    i = len;
                }
            }
            else
            {
                if ((r = read(0, buf + i, 1)) != 1)
                {
                    if (r < 0)
                    {
debugf("read error:%d\n", r);
                    }
                    exit();
                }
            }
        }
        // 修改部分2结束
    }
    debugf("line too long\n");
    while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n')
    {
        ;
    }
    buf[0] = 0;
}

char buf[1024];

void usage(void)
{
    printf("usage: sh [-ix] [script-file]\n");
    exit();
}

int main(int argc, char **argv)
{
    int r;
    if ((r = open(".history", O_TRUNC | O_CREAT)) < 0)
        return r;
    hislen = 0;
    int interactive = iscons(0);
    int echocmds = 0;
    printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
    printf("::                                                         ::\n");
    printf("::                     MOS Shell 2024                      ::\n");
    printf("::                                                         ::\n");
    printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
    ARGBEGIN
    {
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

    if (argc > 1)
    {
        usage();
    }
    if (argc == 1)
    {
        close(0);
        if ((r = open(argv[0], O_RDONLY)) < 0)
        {
            user_panic("open %s: %d", argv[0], r);
        }
        user_assert(r == 0);
    }
    for (;;)
    {
        if (interactive)
        {
            printf("\n$ ");
        }
        readline(buf, sizeof buf);
        savecmd(buf);

        if (buf[0] == '#')
        {
            continue;
        }
        if (echocmds)
        {
            printf("# %s\n", buf);
        }

	 if (buf[0] == 'j' && buf[1] == 'o' && buf[2] == 'b' && buf[3] == 's')
        {
            int job_i = 1;
            const volatile struct Env *temp_env;
            for (job_i = 1; job_i < job_id; job_i++)
            {
                temp_env = &envs[ENVX(job_envid[job_i])];
                if (temp_env->env_status == ENV_FREE)
                {
                    char *stemp = "Done";
                    strcpy(status[job_i], stemp);
                }
                printf("[%d] %-10s 0x%08x %s\n", job_i, status[job_i], job_envid[job_i], cmd[job_i]);
            }
            continue;
        }
	  if (buf[0] == 'f' && buf[1] == 'g')
        {
            int temp_sum = 0;
            int temp_size = strlen(buf) - 1;
            int temp_i = 3;
            for (temp_i = 3; temp_i <= temp_size; temp_i++)
            {
                temp_sum = temp_sum * 10 + (buf[temp_i] - '0');
            }
            if (temp_sum >= job_id)
            {
                printf("fg: job (%d) do not exist\n", temp_sum);
                continue;
            }
            const volatile struct Env *temp_env;
            temp_env = &envs[ENVX(job_envid[temp_sum])];
            if (temp_env->env_status == ENV_FREE)
            {
                char *stemp = "Done";
                strcpy(status[temp_sum], stemp);
                printf("fg: (0x%08x) not running\n", job_envid[temp_sum]);
                continue;
            }
            else
            {
		wait(job_envid[temp_sum]);
                continue;
            }
        }





        if (buf[0] == 'k' && buf[1] == 'i' && buf[2] == 'l' && buf[3] == 'l')
        {
            int temp_sum = 0;
            int temp_size = strlen(buf) - 1;
            int temp_i = 5;
            for (temp_i = 5; temp_i <= temp_size; temp_i++)
            {
                temp_sum = temp_sum * 10 + (buf[temp_i] - '0');
            }
            if (temp_sum >= job_id)
            {
                printf("fg: job (%d) do not exist\n", temp_sum);
                continue;
            }
            const volatile struct Env *temp_env;
            temp_env = &envs[ENVX(job_envid[temp_sum])];
            if (temp_env->env_status == ENV_FREE)
            {
                char *stemp = "Done";
                strcpy(status[temp_sum], stemp);
                printf("fg: (0x%08x) not running\n", job_envid[temp_sum]);
                continue;
            }
            else
            {
                syscall_env_destroy(job_envid[temp_sum]);
                continue;
            }
        }



        if ((r = fork()) < 0)
        {
            user_panic("fork: %d", r);
        }
        if (r == 0)
        {
            if (buf[0] == 'h' && buf[1] == 'i' && buf[2] == 's' && buf[3] == 't' && buf[4] == 'o' && buf[5] == 'r' && buf[6] == 'y')
            {
                char prefix[1024] = "cat < .";
                strcat(prefix, buf);
                runcmd(prefix);
            }
            else
                runcmd(buf);
            exit();
        }
        else
        {
            int temp_size = strlen(buf) - 1;
            int temp_flag = 0;
            for (; temp_size >= 0; temp_size--)
            {
                if (strchr(WHITESPACE, buf[temp_size]))
                {
                    continue;
                }
                if (buf[temp_size] == '&')
                {
                    temp_flag = 1;
                    break;
                }
                else
                {
                    temp_flag = 0;
                    break;
                }
            }
            if (temp_flag == 1)
            { // 后台
                const volatile struct Env *temp_env;
                temp_env = &envs[ENVX(syscall_getenvid())];
                int r_temp = ipc_recv(0, 0, 0);
                wait(r);
                if (r_temp)
                {
                    strcpy(cmd[job_id], buf);
                    job_envid[job_id] = r_temp;
                    char *stemp = "Running";
                    strcpy(status[job_id], stemp);
                    job_id++;
                }
                wait(r);
            }
            else
            {
                wait(r);
            }

        }



    }
    return 0;
}


