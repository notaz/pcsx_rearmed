#include "ps3.h"

int lightrec_ps3_init(void)
{
	FILE *log = NULL;
	int ret;

#if PS3_LIGHTREC_LOG
	lightrec_ps3_log_init();
	log = lightrec_ps3_open_log("ps3-lightrec-selftest.log", "w");
#endif
	ret = lightrec_ps3_init_code(log);
#if PS3_LIGHTREC_SELFTEST
	if (!ret)
		ret = lightrec_ps3_jit_selftest(log);
#endif
#if PS3_LIGHTREC_LOG
	if (log)
		fclose(log);
#endif
	if (ret)
		lightrec_ps3_free_code();
	return ret;
}
