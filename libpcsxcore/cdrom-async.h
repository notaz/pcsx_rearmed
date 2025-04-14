#include "psxcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CdrStat;

#ifdef HAVE_CDROM
int  rcdrom_open(const char *name, u32 *total_lba);
void rcdrom_close(void);
int  rcdrom_getTN(u8 *tn);
int  rcdrom_getTD(u32 total_lba, u8 track, u8 *rt);
int  rcdrom_getStatus(struct CdrStat *stat);

int cdrom_read_sector(void *stream, unsigned int lba, void *b);
int cdrom_is_media_inserted(void *stream);
#endif

int  cdra_init(void);
void cdra_shutdown(void);
int  cdra_open(void);
void cdra_close(void);
int  cdra_getTN(unsigned char *tn);
int  cdra_getTD(int track, unsigned char *rt);
int  cdra_getStatus(struct CdrStat *stat);
int  cdra_readTrack(const unsigned char *time);
int  cdra_readCDDA(const unsigned char *time, void *buffer);
int  cdra_readSub(const unsigned char *time, void *buffer);
int  cdra_prefetch(unsigned char m, unsigned char s, unsigned char f);

int  cdra_is_physical(void);
int  cdra_check_eject(int *inserted);
void cdra_stop_thread(void);
void cdra_set_buf_count(int count);
int  cdra_get_buf_count(void);
int  cdra_get_buf_cached_approx(void);

void *cdra_getBuffer(void);

#ifdef __cplusplus
}
#endif
