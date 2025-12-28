//============================================
//=== Audio XA decoding
//=== Kazzuya
//============================================

#ifndef DECODEXA_H
#define DECODEXA_H

struct xa_decode;

int xa_decode_sector( struct xa_decode *xdp,
					   unsigned char *sectorp,
					   int is_first_sector );

#endif
