#include "libretro-cdrom.h"
#include "../deps/libretro-common/cdrom/cdrom.c"
#if defined(__linux__) && !defined(ANDROID)
//#include <linux/cdrom.h>
#endif

static int cdrom_send_command_dummy(const libretro_vfs_implementation_file *stream,
      CDROM_CMD_Direction dir, void *buf, size_t len, unsigned char *cmd, size_t cmd_len,
      unsigned char *sense, size_t sense_len)
{
   return 1;
}

static int cdrom_send_command_once(const libretro_vfs_implementation_file *stream,
      CDROM_CMD_Direction dir, void *buf, size_t len, unsigned char *cmd, size_t cmd_len)
{
   unsigned char sense[CDROM_MAX_SENSE_BYTES] = {0};
   int ret =
#if defined(__linux__) && !defined(ANDROID)
      cdrom_send_command_linux
#elif defined(_WIN32) && !defined(_XBOX)
      cdrom_send_command_win32
#else
      cdrom_send_command_dummy
#endif
         (stream, dir, buf, len, cmd, cmd_len, sense, sizeof(sense));
#ifdef CDROM_DEBUG
   if (ret && sense[2])
      cdrom_print_sense_data(sense, sizeof(sense));
#endif
   (void)cdrom_send_command_dummy;
   return ret;
}

// "extensions" to libretro-common
int cdrom_set_read_speed_x(libretro_vfs_implementation_file *stream, unsigned speed)
{
   // SET CD-ROM SPEED, DA is newer?
   unsigned char cmd1[] = {0xDA, 0, speed - 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
   unsigned char cmd2[] = {0xBB, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
   int ret;
   ret = cdrom_send_command_once(stream, DIRECTION_NONE, NULL, 0, cmd1, sizeof(cmd1));
   if (ret) {
#if defined(__linux__) && !defined(ANDROID)
      // doesn't work, too late?
      //ret = ioctl(fileno(stream->fp), CDROM_SELECT_SPEED, &speed);
#endif
   }
   if (ret) {
      speed = speed * 2352 * 75 / 1024;
      cmd2[2] = speed >> 8;
      cmd2[3] = speed;
      ret = cdrom_send_command_once(stream, DIRECTION_NONE, NULL, 0, cmd2, sizeof(cmd2));
   }
   return ret;
}

int cdrom_read_sector(libretro_vfs_implementation_file *stream,
      unsigned int lba, void *b)
{
   unsigned char cmd[] = {0xBE, 0, 0, 0, 0, 0, 0, 0, 1, 0xF8, 0, 0};
   cmd[2] = lba >> 24;
   cmd[3] = lba >> 16;
   cmd[4] = lba >> 8;
   cmd[5] = lba;
   return cdrom_send_command_once(stream, DIRECTION_IN, b, 2352, cmd, sizeof(cmd));
}

// vim:sw=3:ts=3:expandtab
