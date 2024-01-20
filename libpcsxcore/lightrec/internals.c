// internals: stuff lightrec's public API doesn't provide
#include <assert.h>
#include "lightrec-private.h"
#include "blockcache.h"
#include "internals.h"

void lightrec_plugin_clear_block_caches(struct lightrec_state *state)
{
	if (state == NULL)
		return;

	lightrec_invalidate_all(state);
	lightrec_free_block_cache(state->block_cache);
	state->block_cache = lightrec_blockcache_init(state);
	assert(state->block_cache);
}
