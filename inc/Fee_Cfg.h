#ifndef FEE_CFG_H
#define FEE_CFG_H

#include "Std_Types.h"

#define FEE_SLOT_SIZE           64u
#define FEE_SLOTS_PER_SECTOR    16u
#define FEE_MAX_BLOCKS          3u

typedef struct
{
	uint16_t blockId;
	uint16_t dataSize;
} Fee_BlockConfigType;

extern const Fee_BlockConfigType Fee_BlockConfig[FEE_MAX_BLOCKS];

#endif
