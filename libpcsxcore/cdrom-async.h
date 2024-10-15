
#ifdef __cplusplus
extern "C" {
#endif

struct CdrStat;

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
