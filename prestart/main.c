/**************************************************/
// New file
//
// The main implementation of post-loader-prestart program.
//
// 2017.08.22, by ysl
/**************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <pwd.h>
#include <sys/wait.h>     // for waitpid()
#include <linux/limits.h> // for PATH_MAX
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "wine/prestart_util.h"

#define ERR_EXIT(msg)       \
    do                      \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define MAXLINE 2048
#define LISTENQ 20
#define MAX_EVENT_NUMBER 1024
#define EPWAIT_TIMEOUT 500 // ms

#define S_PRESTARTED 1
#define S_STARTED 2
#define S_EXITED 3

typedef struct _PrestartAppInfo
{
    LIST_ENTRY ConfigPrestartAppList;
    char Wineprefix[PATH_MAX];
    char FullPath[PATH_MAX];
    char **Argv;
    // int singleton; // indicates an application is singleton.
} PrestartAppInfo, *PPrestartAppInfo;

typedef struct _ConfigPrestartList
{
    LIST_ENTRY ConfigPrestartAppList;
    size_t Count;
} ConfigPrestartAppList;

typedef struct _PrestartItem
{
    LIST_ENTRY PrestartedAppList;
    PPrestartAppInfo AppInfo;
    int Status;
    pid_t pid;
    int sig_randnum;
} PrestartItem, *PPrestartItem;

typedef struct _PrestartList
{
    LIST_ENTRY PrestartedAppList;
} PrestartList;

static PrestartList prestart_list;

static const char prestart_list_conf[] = "/.wine-prestart/prestart.list";
static ConfigPrestartAppList config_prestart_list;
static const char cAppInfoDelim = '#';

static const char *commands[] =
    {
        "quit", // exit the program
        "list", // list prestarted applications
        NULL};

static void prestart_init(void);
static void prestart_cleanup(void);

static void init_config_prestart_list(void);
static void destroy_config_prestart_list(void);
static void init_prestart_list(void);
static void destroy_prestart_list(void);
static void insert_prestart_list(PPrestartAppInfo appInfo);

static int signal_start(pid_t pid, int randnum);
static int signal_kill_process(pid_t pid, int randnum);

static void sig_child_handler(int signum);
static void sig_end_program_handler(int signum);
static void init_signal();

static int check_prestart(PPrestartAppInfo appInfo);
static int start_app(PPrestartAppInfo appInfo);

static void cmd_list_prestart_apps(void);

static void usage(const char *app)
{
    printf("Usage: %s \n", app);
    //exit(0);
}

static void ls_commands(void)
{
    const char **cmd = commands;

    fprintf(stderr, "supported commands:\n");
    while (*cmd)
    {
        fprintf(stderr, "\t%s\n", *cmd);
        cmd++;
    }
}

static void setnonblocking(int sock)
{
    int opts;
    opts = fcntl(sock, F_GETFL);
    if (opts < 0)
    {
        ERR_EXIT("fcntl(sock,GETFL)");
    }
    opts = opts | O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opts) < 0)
    {
        ERR_EXIT("fcntl(sock,SETFL,opts)");
    }
}

int main(int argc, char *argv[])
{
	int quit = 0;
    int i, listenfd, connfd, sockfd, epfd;
    char data[MAXLINE];
    socklen_t clilen;
    struct sockaddr_un clientaddr;
    struct sockaddr_un serveraddr;
    //声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件
    struct epoll_event ev, events[MAX_EVENT_NUMBER];

    srand((unsigned int)time(NULL));
    prestart_init();

    listenfd = socket(PF_UNIX, SOCK_STREAM, 0);
    //把socket设置为非阻塞方式
    setnonblocking(listenfd);

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sun_family = AF_UNIX;
    strcpy(serveraddr.sun_path, get_prestart_socket_path());

    bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    listen(listenfd, LISTENQ);

    //生成用于处理accept的epoll专用的文件描述符
    epfd = epoll_create(256);
    //设置与要处理的事件相关的文件描述符
    ev.data.fd = listenfd;
    //设置要处理的事件类型
    ev.events = EPOLLIN | EPOLLET;
    //注册epoll事件
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    ev.data.fd = STDIN_FILENO;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);

    while (quit == 0)
    {
        //等待epoll事件的发生
        int nfds = epoll_wait(epfd, events, MAX_EVENT_NUMBER, EPWAIT_TIMEOUT);
        //处理所发生的所有事件
        for (i = 0; i < nfds; i++)
        {
            sockfd = events[i].data.fd;
            if (sockfd == listenfd) //如果新监测到一个SOCKET用户连接到了绑定的SOCKET端口，建立新的连接。
            {
                connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clilen);
                if (connfd < 0)
                {
                    // if (errno == EINTR)
                    //     continue;
                    fprintf(stderr, "accept: %s\n", strerror(errno));
                    continue;
                }
                setnonblocking(connfd);

                //设置用于读操作的文件描述符
                ev.data.fd = connfd;
                //设置用于注测的读操作事件
                ev.events = EPOLLIN | EPOLLET;
                //注册ev
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            }                                // if listenfd
            else if (sockfd == STDIN_FILENO) // user standard input
            {
                int rdlen = fscanf(stdin, "%s", data);
                if (rdlen < 0)
                {
                    fprintf(stderr, "read: %s\n", strerror(errno));
                }
                else if (rdlen == 0)
                {
                    // empty
                }
                else
                {
                    // fprintf(stderr, "stdin: %s\n", data);
                    if (!strcmp(data, "quit"))
                    {
						quit = 1;
						break;
                    }
                    else if (!strcmp(data, "list"))
                    {
                        cmd_list_prestart_apps();
                    }
                    else
                    {
                        ls_commands();
                    }
                }
            }                                    // if STDIN
            else if (events[i].events & EPOLLIN) //如果是已经连接的用户，并且收到数据，那么进行读入。
            {
                // read data
                int readdone = 0;
                int rcvlen = 0;
                for (;;)
                {
                    rcvlen = recv(sockfd, data, MAXLINE, 0);
                    if (rcvlen < 0)
                    {
                        if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
                            continue;
                        else // read error
                        {
                            perror("recv");
                            break;
                        }
                    }
                    else if (rcvlen == 0)
                        continue;
                    else
                    {
                        // fprintf(stderr, "received %c\n", data[0]);
                        data[rcvlen] = '\0';
                        readdone = 1;
                        break;
                    }
                } // for-loop reading data
                if (readdone)
                {
                    PrestartAppInfo appInfo;
                    strcpy(appInfo.FullPath, data);
                    //if (check_prestart(&appInfo))
                    if (start_app(&appInfo) != -1) // start succeeded.
                    {
                        sprintf(data, "%d", 1);
                    }
                    else
                    {
                        sprintf(data, "%d", 0);
                    }
                    send(sockfd, data, strlen(data), 0);
                }
                close(sockfd);
                events[i].data.fd = -1;
            } // if EPOLLIN
        }     // for i <- 0 to nfds - 1
    }         // for-loop

    close(listenfd);
	unlink(get_prestart_socket_path());
    prestart_cleanup();
    return 0;
}

static void prestart_init(void)
{
    PLIST_ENTRY head, entry;

    if (prestart_initialize() == -1)
    {
        fprintf(stderr, "prestart_initialize failed\n");
        exit(EXIT_FAILURE);
    }

    init_signal();
    init_config_prestart_list();
    init_prestart_list();

    head = &config_prestart_list.ConfigPrestartAppList;
    for (entry = head->Flink; entry != head; entry = entry->Flink)
    {
        PPrestartAppInfo pAppInfo = CONTAINING_RECORD(entry, PrestartAppInfo, ConfigPrestartAppList);
        insert_prestart_list(pAppInfo);
    }
}

static void prestart_cleanup(void)
{
    destroy_prestart_list();
    destroy_config_prestart_list();
}

static void sig_child_handler(int signum)
{
    int status = 0;
    int ret = 0;

    for (;;)
    {
        ret = waitpid(0, &status, WNOHANG);
        if (ret > 0) // child exited
        {
            PLIST_ENTRY head, entry;
            
            fprintf(stderr, "Got child %d\n", ret);
            if (WIFEXITED(status))
            {
                fprintf(stderr, "exit status %d\n", WEXITSTATUS(status));
            }

            if (WIFSIGNALED(status))
            {
                fprintf(stderr, "child %d is terminated with signal %d\n", ret, WTERMSIG(status));
            }

            // update status
            head = &prestart_list.PrestartedAppList;
            for (entry = head->Flink; entry != head; entry = entry->Flink)
            {
                PPrestartItem pitem = CONTAINING_RECORD(entry, PrestartItem, PrestartedAppList);
                if (pitem->pid == ret)
                {
                    pitem->Status = S_EXITED;
                    break;
                }
            }
        }
        else if (ret == 0) // no child exited
        {
            fprintf(stderr, "No child exited\n");
            break;
        }
        else
        {
            if (errno == ECHILD)
            {
                fprintf(stderr, "Child does not exist\n");
            }
            else
            {
                fprintf(stderr, "Invalid argument passed to waitpid\n");
            }

            break;
        }
    } // for-loop
}

static void sig_end_program_handler(int signum)
{
    ls_commands();
}

static void init_signal()
{
    struct sigaction act;
    act.sa_handler = sig_end_program_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    act.sa_handler = sig_child_handler;
    sigaction(SIGCHLD, &act, NULL);
}

static void cmd_list_prestart_apps(void)
{
    PLIST_ENTRY head, entry;
    size_t no = 1;

    head = &prestart_list.PrestartedAppList;
    for (entry = head->Flink; entry != head; entry = entry->Flink)
    {
        PPrestartItem pitem = CONTAINING_RECORD(entry, PrestartItem, PrestartedAppList);
        fprintf(stderr, "%02lu\n--\n", no++);
        fprintf(stderr, "WINEPREFIX\t: %s\n", pitem->AppInfo->Wineprefix);
        fprintf(stderr, "AppFullPath\t: %s\n", pitem->AppInfo->FullPath);
        fprintf(stderr, "Status\t\t: %d\n", pitem->Status);
        fprintf(stderr, "\n");
    }
}

/*******************************/
// Just check exists for now
/*******************************/
static int check_prestart(PPrestartAppInfo appInfo)
{
    PLIST_ENTRY head, entry;

    head = &prestart_list.PrestartedAppList;
    for (entry = head->Flink; entry != head; entry = entry->Flink)
    {
        PPrestartItem pitem = CONTAINING_RECORD(entry, PrestartItem, PrestartedAppList);
        if (!strcmp(pitem->AppInfo->FullPath, appInfo->FullPath) && pitem->Status == S_PRESTARTED)
        {
            return 1;
        }
    }

    return 0;
}

static int start_app(PPrestartAppInfo appInfo)
{
    PLIST_ENTRY head, entry;

    head = &prestart_list.PrestartedAppList;
    for (entry = head->Flink; entry != head; entry = entry->Flink)
    {
        PPrestartItem pitem = CONTAINING_RECORD(entry, PrestartItem, PrestartedAppList);
        if (!strcmp(pitem->AppInfo->FullPath, appInfo->FullPath) && pitem->Status == S_PRESTARTED)
        {
            signal_start(pitem->pid, pitem->sig_randnum);
            pitem->Status = S_STARTED;
            break;
        }
    }

    if (entry != head) // exists and started
        return 0;
    return -1;
}

static void init_config_prestart_list(void)
{
    FILE *fconf = NULL;
    struct passwd *pwd = NULL;
    size_t len = 0;
    char line[MAXLINE];
    char confpath[PATH_MAX] = {0};

    InitializeListHead(&config_prestart_list.ConfigPrestartAppList);
    config_prestart_list.Count = 0;

    pwd = getpwuid(getuid());
    if (!pwd)
    {
        fprintf(stderr, "getpwuid: %s\n", strerror(errno));
        return;
    }
    strcpy(confpath, pwd->pw_dir);
    // remove trailing slashes
    len = strlen(confpath);
    while (len > 1 && confpath[len - 1] == '/')
        confpath[--len] = 0;
    strcat(confpath, prestart_list_conf);

    if (!(fconf = fopen(confpath, "r")))
    {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
        return;
    }

    while (fgets(line, MAXLINE, fconf)) // line ends with '\n'
    {
        PPrestartAppInfo pAppInfo = NULL;
        char *p = NULL, *q = NULL;

        if (line[0] == '\0' || line[0] == '\n') // empty line
            continue;

        pAppInfo = (PPrestartAppInfo)malloc(sizeof(PrestartAppInfo));
        p = line;
        do
        {
            // WINEPREFIX
            if (q = strchr(p, cAppInfoDelim))
            {
                *q = '\0';
                strcpy(pAppInfo->Wineprefix, p);
            }
            else
                break;

            // App full path
            p = ++q;
            if (q = strchr(p, cAppInfoDelim))
            {
                *q = '\0';
                strcpy(pAppInfo->FullPath, p);
            }
            else
            // a small trick to deal with the last element
            {
                q = p;
                while (*q)
                {
                    if (*q == '\n') // end of line
                    {
                        *q = '\0';
                        break;
                    }
                    q++;
                }

                if (q != p)
                    strcpy(pAppInfo->FullPath, p);

                break;
            }

            // Args
            // Ignore args for now
        } while (0);

        if (pAppInfo->FullPath[0] != '\0')
        {
            InsertTailList(&config_prestart_list.ConfigPrestartAppList, &pAppInfo->ConfigPrestartAppList);
            config_prestart_list.Count++;
        }
        else
            free(pAppInfo);
    }

    fclose(fconf);
}

static void destroy_config_prestart_list(void)
{
    PLIST_ENTRY head, entry;

    head = &config_prestart_list.ConfigPrestartAppList;
    entry = head->Flink;
    while (entry != head)
    {
        PLIST_ENTRY e = entry->Flink;
        PPrestartAppInfo pinfo = CONTAINING_RECORD(entry, PrestartAppInfo, ConfigPrestartAppList);

        RemoveEntryList(entry);
        free(pinfo);
        entry = e;
    }
    config_prestart_list.Count = 0;
}

static void init_prestart_list(void)
{
    InitializeListHead(&prestart_list.PrestartedAppList);
}

static void destroy_prestart_list(void)
{
    PLIST_ENTRY head, entry;

    head = &prestart_list.PrestartedAppList;
    entry = head->Flink;
    while (entry != head)
    {
        PLIST_ENTRY e = entry->Flink;
        PPrestartItem pitem = CONTAINING_RECORD(entry, PrestartItem, PrestartedAppList);

        signal_kill_process(pitem->pid, pitem->sig_randnum);
        RemoveEntryList(entry);
        free(pitem);
        entry = e;
    }
}

static void insert_prestart_list(PPrestartAppInfo appInfo)
{
    PPrestartItem pitem = NULL;

    pitem = (PPrestartItem)malloc(sizeof(PrestartItem));
    pitem->AppInfo = appInfo;
    pitem->Status = S_PRESTARTED;
    pitem->sig_randnum = rand();
    pitem->sig_randnum = pitem->sig_randnum == 0 ? RAND_MAX : pitem->sig_randnum;
    pitem->pid = fork();
    if (pitem->pid < 0)
    {
        perror("fork");
        free(pitem);
        return;
    }
    else if (pitem->pid == 0) // child
    {
        char postprestart_env_name[] = "WINEPRESTART";
        char *sig_rnd_str = (char *)malloc(40);

        sprintf(sig_rnd_str, "%d", pitem->sig_randnum);
        setenv(postprestart_env_name, sig_rnd_str, 1);
        if (pitem->AppInfo->Wineprefix[0] != '\0')
        {
            setenv("WINEPREFIX", pitem->AppInfo->Wineprefix, 1);
        }
        //free(sig_rnd_str);
        execlp("wine", "wine", pitem->AppInfo->FullPath, NULL);
    }

    // parent
    InsertTailList(&prestart_list.PrestartedAppList, &pitem->PrestartedAppList);
}

static int signal_kill_process(pid_t pid, int randnum)
{
    union sigval si;

    si.sival_int = randnum;
    return sigqueue(pid, SIGTERM, si);
}

static int signal_start(pid_t pid, int randnum)
{
    union sigval si;

    si.sival_int = randnum;
    return sigqueue(pid, SIGUSR1, si);
}