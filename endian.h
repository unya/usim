/* 
 * endian.h
 * Some macros to handle endian-ness issues in usim CADR emulation
 *
 * $Id$
 *
 */

#ifndef _USIM_ENDIAN_H_
#define _USIM_ENDIAN_H_

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define __LITTLE_ENDIAN__  1
#endif

#ifdef OSX
#include <CoreFoundation/CFByteOrder.h>
#define SWAP_SHORT(x) CFSwapInt16LittleToHost(x)
#define SWAP_LONG(x) CFSwapInt32LittleToHost(x)
#else /* hand coded versions */
#define SWAP_SHORT(x) ( (((x) & 0xff00) >> 8) | (((x) & 0x00ff) << 8) )
#define SWAP_LONG(x) ( (((x) & 0xff000000) >> 24) | \
(((x) & 0x00ff0000) >> 8) | \
(((x) & 0x0000ff00) << 8) | \
(((x) & 0x000000ff) << 24) )
#endif /* def OSX */

#ifdef __BIG_ENDIAN__
#define LE_TO_SHORT(s) (SWAP_SHORT(s))
#define LE_TO_LONG(l) (SWAP_LONG(l))
#elif defined(__LITTLE_ENDIAN__)
#define LE_TO_SHORT(s) (s)
#define LE_TO_LONG(l) (l)
#else
#error "No _ENDIAN__ macro defined."
#endif

#endif
