#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "../../libpcsxcore/lightrec/ps3.h"

static char directory[1024];
static char filename[1024];
static unsigned int mkdir_calls;
static unsigned int open_calls;
static int mkdir_error;
static int failures;

int ps3_test_mkdir(const char *path, unsigned int mode)
{
   mkdir_calls++;
   snprintf(directory, sizeof(directory), "%s", path);
   if (mode != 0777)
      failures++;
   errno = mkdir_error;
   return mkdir_error ? -1 : 0;
}

FILE *ps3_test_fopen(const char *path, const char *mode)
{
   open_calls++;
   snprintf(filename, sizeof(filename), "%s", path);
   if (strcmp(mode, "w"))
      failures++;
   return NULL;
}

int main(void)
{
   lightrec_ps3_executable_path = "/dev_hdd0/game/OTHER0001/USRDIR/cores/pcsx.self";
   lightrec_ps3_log_init();
   lightrec_ps3_open_log("test.log", "w");
   if (mkdir_calls != 1 || open_calls != 1 ||
       strcmp(directory, "/dev_hdd0/game/OTHER0001/USRDIR/cores/logs") ||
       strcmp(filename, "/dev_hdd0/game/OTHER0001/USRDIR/cores/logs/test.log"))
      return 1;
   mkdir_error = EEXIST;
   lightrec_ps3_executable_path = "/app_home/Nested/Core.SELF";
   lightrec_ps3_log_init();
   lightrec_ps3_open_log("test.log", "w");
   if (open_calls != 2 || strcmp(filename, "/app_home/Nested/logs/test.log"))
      return 2;
   mkdir_error = EACCES;
   lightrec_ps3_log_init();
   lightrec_ps3_open_log("test.log", "w");
   lightrec_ps3_executable_path = NULL;
   lightrec_ps3_log_init();
   lightrec_ps3_open_log("test.log", "w");
   lightrec_ps3_executable_path = "relative/Core.SELF";
   lightrec_ps3_log_init();
   lightrec_ps3_open_log("test.log", "w");
   if (open_calls != 2 || mkdir_calls != 3 || failures)
      return 3;
   puts("PASS: PSL1GHT core-relative paths, existing directories and unavailable paths");
   return 0;
}
