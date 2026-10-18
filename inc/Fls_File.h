#ifndef FLS_FILE_H
#define FLS_FILE_H

#include "Std_Types.h"

#define FLS_TOTAL_SIZE          2048u
#define FLS_SECTOR_SIZE         1024u
#define FLS_NUMBER_OF_SECTORS   2u

Std_ReturnType Fls_Init(void);

Std_ReturnType Fls_Read(
	uint32_t address,
	uint8_t *data,
	uint32_t length);

Std_ReturnType Fls_Write(
	uint32_t address,
	const uint8_t *data,
	uint32_t length);

Std_ReturnType Fls_EraseSector(
	uint8_t sectorId);

void Fls_Dump(
	uint32_t address,
	uint32_t length);

#endif
