#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "ps3.h"

static char log_directory[1024];
#ifdef __PSL1GHT__
const char *lightrec_ps3_executable_path;
#endif

void lightrec_ps3_log_init(void)
{
	const char *executable = lightrec_ps3_executable_path;
	const char *slash;
	size_t len;
	int ret;

	log_directory[0] = 0;
	if (!executable || executable[0] != '/')
		return;
	slash = strrchr(executable, '/');
	len = slash - executable;
	if (len + sizeof("/logs") > sizeof(log_directory))
		return;
	memcpy(log_directory, executable, len);
	memcpy(log_directory + len, "/logs", sizeof("/logs"));
	ret = mkdir(log_directory, 0777);
	if (ret && errno != EEXIST)
		log_directory[0] = 0;
}

FILE *lightrec_ps3_open_log(const char *name, const char *mode)
{
	char path[1024];
	int len;

	if (!log_directory[0])
		return NULL;
	len = snprintf(path, sizeof(path), "%s/%s", log_directory, name);
	if (len < 0 || (size_t)len >= sizeof(path))
		return NULL;
	return fopen(path, mode);
}
