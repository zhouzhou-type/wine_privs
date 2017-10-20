/**************************************************/
// New file, a copy of this file locates in ~/loader/
//
// 2017.08.22, by ysl
/**************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>
#include <linux/limits.h> // for PATH_MAX

#include "wine/prestart_util.h"

static const char prestart_config_dir[] = "/.wine"; // config dir relative to $HOME
static const char prestart_root_prefix[] = "/tmp/.wine-prestart";
static const char prestart_dir_prefix[] = "/prestart-";
static const char prestart_socket_name[] = "socket";
static char *prestart_socket_path = NULL;
static char *prestart_socket_dir = NULL;

static int init_prestart_socket_dir(dev_t dev, ino_t ino);
static int setup_prestart_socket_dir(void);
const char *get_prestart_socket_path(void);

/* remove all trailing slashes from a path name */
static inline void remove_trailing_slashes(char *path)
{
	int len = strlen(path);
	while (len > 1 && path[len - 1] == '/')
		path[--len] = 0;
}

static int init_prestart_socket_dir(dev_t dev, ino_t ino)
{
	char *p, *root;

	root = malloc(sizeof(prestart_root_prefix) + 12);
	sprintf(root, "%s-%u", prestart_root_prefix, getuid());

	prestart_socket_dir = malloc(strlen(root) + sizeof(prestart_dir_prefix) + 2 * sizeof(dev) + 2 * sizeof(ino) + 2);
	strcpy(prestart_socket_dir, root);
	strcat(prestart_socket_dir, prestart_dir_prefix);
	p = prestart_socket_dir + strlen(prestart_socket_dir);

	if (dev != (unsigned long)dev)
		p += sprintf(p, "%lx%08lx-", (unsigned long)((unsigned long long)dev >> 32), (unsigned long)dev);
	else
		p += sprintf(p, "%lx-", (unsigned long)dev);

	if (ino != (unsigned long)ino)
		sprintf(p, "%lx%08lx", (unsigned long)((unsigned long long)ino >> 32), (unsigned long)ino);
	else
		sprintf(p, "%lx", (unsigned long)ino);
	free(root);

	return 0;
}

static int setup_prestart_socket_dir()
{
	struct stat st;
	char *p = prestart_socket_dir;

	p++;
	while ((p = strchr(p, '/')) != NULL)
	{
		*p = '\0';
		if (stat(prestart_socket_dir, &st) != 0)
		{
			if (errno != ENOENT)
			{
				fprintf(stderr, "cannot stat %s\n", prestart_socket_dir);
				return -1;
			}
			if (mkdir(prestart_socket_dir, 0700) != 0)
			{
				fprintf(stderr, "cannot mkdir %s\n", prestart_socket_dir);
				return -1;
			}
		}
		*p = '/';
		p++;
	}
	if (stat(prestart_socket_dir, &st) != 0)
	{
		if (errno != ENOENT)
		{
			fprintf(stderr, "cannot stat %s\n", prestart_socket_dir);
			return -1;
		}
		if (mkdir(prestart_socket_dir, 0700) != 0)
		{
			fprintf(stderr, "cannot mkdir %s\n", prestart_socket_dir);
			return -1;
		}
	}

	return 0;
}

int prestart_initialize(void)
{
	char wine_config_path[PATH_MAX];
	char *wineprefix = NULL;
	struct passwd *pwd = NULL;
	struct stat statbuf;

	wineprefix = getenv("WINEPREFIX");
	if (!wineprefix)
	{
		//fprintf(stderr, "WINEPREFIX not specified\n");
		pwd = getpwuid(getuid());
		strcpy(wine_config_path, pwd->pw_dir);
		remove_trailing_slashes(wine_config_path);
		strcat(wine_config_path, prestart_config_dir);
	}
	else
	{
		strcpy(wine_config_path, wineprefix);
	}

	if (stat(wine_config_path, &statbuf) == -1)
	{
		// no such dir or permission denied
		fprintf(stderr, "stat");
		return -1;
	}
	if (!S_ISDIR(statbuf.st_mode))
	{
		fprintf(stderr, "%s is not a directory\n", wine_config_path);
		return -1;
	}
	if (statbuf.st_uid != getuid())
	{
		fprintf(stderr, "%s is not owned by you\n", wine_config_path);
		return -1;
	}

	init_prestart_socket_dir(statbuf.st_dev, statbuf.st_ino);
	if (setup_prestart_socket_dir() == -1)
	{
		return -1;
	}

	prestart_socket_path = malloc(strlen(prestart_socket_dir) + sizeof(prestart_socket_name) + 1);
	strcpy(prestart_socket_path, prestart_socket_dir);
	strcat(prestart_socket_path, "/");
	strcat(prestart_socket_path, prestart_socket_name);

	return 0;
}

const char *get_prestart_socket_path(void)
{
	if (!prestart_socket_path)
	{
		prestart_initialize();
	}

	return prestart_socket_path;
}
