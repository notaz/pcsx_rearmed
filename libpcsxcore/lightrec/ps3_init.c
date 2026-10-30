#include "ps3.h"

int lightrec_ps3_init(void)
{
	int ret = lightrec_ps3_init_code(NULL);
	if (ret)
		lightrec_ps3_free_code();
	return ret;
}
