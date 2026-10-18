#ifndef FEE_H
#define FEE_H

#include "Std_Types.h"

Std_ReturnType Fee_Init(void);

Std_ReturnType Fee_Write(
	uint16_t blockId,
	const uint8_t *data);

Std_ReturnType Fee_Read(
	uint16_t blockId,
	uint8_t *data);

void Fee_ShowLayout(void);

#endif
