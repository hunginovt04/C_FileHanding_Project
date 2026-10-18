#include "Test_Day4.h"

#include "Fee.h"
#include "Fee_Cfg.h"
#include "Fls_File.h"

#include <stdio.h>
#include <string.h>

#define TEST_FEE_RECORD_MAGIC 0xA55Au
#define TEST_FEE_RECORD_HEADER_SIZE 16u

typedef struct
{
	uint16_t magic;
	uint16_t blockId;
	uint16_t dataLength;
	uint16_t reserved;
	uint32_t sequence;
	uint32_t reserved2;
} Test_Day4_RecordHeaderType;

/* Fill pattern de mo phong nhieu version block khac nhau. */
static void Test_Day4_FillPattern(uint8_t *buffer, uint16_t size, uint8_t value)
{
	uint16_t i;
	for (i = 0u; i < size; i++)
	{
		buffer[i] = value;
	}
}

/* Doc header theo (sector, slot) de verify sau swap. */
static Std_ReturnType Test_Day4_ReadHeader(uint8_t sectorId, uint8_t slot, Test_Day4_RecordHeaderType *header)
{
	uint8_t slotBuffer[FEE_SLOT_SIZE];
	uint32_t address = ((uint32_t)sectorId * FLS_SECTOR_SIZE) + ((uint32_t)slot * FEE_SLOT_SIZE);

	if (Fls_Read(address, slotBuffer, FEE_SLOT_SIZE) != E_OK)
	{
		return E_NOT_OK;
	}

	(void)memcpy(header, slotBuffer, sizeof(*header));
	return E_OK;
}

/* Kiem tra ca sector da erase het (toan 0xFF). */
static Std_ReturnType Test_Day4_SectorIsAllFF(uint8_t sectorId)
{
	uint8_t value;
	uint32_t i;
	uint32_t base = (uint32_t)sectorId * FLS_SECTOR_SIZE;

	for (i = 0u; i < FLS_SECTOR_SIZE; i++)
	{
		if (Fls_Read(base + i, &value, 1u) != E_OK)
		{
			return E_NOT_OK;
		}
		if (value != 0xFFu)
		{
			return E_NOT_OK;
		}
	}

	return E_OK;
}

/* Scenario 1: ghi > 1 vong slot de bat buoc swap, sau do verify latest data. */
static Std_ReturnType Test_Day4_MultiWriteSwap(void)
{
	uint8_t expectedB1[16] = {0u};
	uint8_t expectedB2[32] = {0u};
	uint8_t expectedB3[8] = {0u};
	uint8_t readB1[16];
	uint8_t readB2[32];
	uint8_t readB3[8];
	uint8_t i;
	uint8_t firstByteB;

	(void)remove("flash.bin");
	if (Fee_Init() != E_OK)
	{
		return E_NOT_OK;
	}

	for (i = 0u; i < 20u; i++)
	{
		uint16_t blockId = (uint16_t)((i % 3u) + 1u);
		uint8_t pattern = (uint8_t)(0x10u + i);
		uint8_t b1[16];
		uint8_t b2[32];
		uint8_t b3[8];
		if (blockId == 1u)
		{
			Test_Day4_FillPattern(b1, 16u, pattern);
			if (Fee_Write(1u, b1) != E_OK)
			{
				return E_NOT_OK;
			}
			(void)memcpy(expectedB1, b1, sizeof(expectedB1));
		}
		else if (blockId == 2u)
		{
			Test_Day4_FillPattern(b2, 32u, pattern);
			if (Fee_Write(2u, b2) != E_OK)
			{
				return E_NOT_OK;
			}
			(void)memcpy(expectedB2, b2, sizeof(expectedB2));
		}
		else
		{
			Test_Day4_FillPattern(b3, 8u, pattern);
			if (Fee_Write(3u, b3) != E_OK)
			{
				return E_NOT_OK;
			}
			(void)memcpy(expectedB3, b3, sizeof(expectedB3));
		}
	}

	if ((Fee_Read(1u, readB1) != E_OK) ||
		(Fee_Read(2u, readB2) != E_OK) ||
		(Fee_Read(3u, readB3) != E_OK))
	{
		return E_NOT_OK;
	}

	if ((memcmp(readB1, expectedB1, sizeof(expectedB1)) != 0) ||
		(memcmp(readB2, expectedB2, sizeof(expectedB2)) != 0) ||
		(memcmp(readB3, expectedB3, sizeof(expectedB3)) != 0))
	{
		return E_NOT_OK;
	}

	if (Test_Day4_SectorIsAllFF(0u) != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fls_Read(FLS_SECTOR_SIZE, &firstByteB, 1u) != E_OK)
	{
		return E_NOT_OK;
	}
	if (firstByteB == 0xFFu)
	{
		return E_NOT_OK;
	}

	if (Fee_Init() != E_OK)
	{
		return E_NOT_OK;
	}

	if ((Fee_Read(1u, readB1) != E_OK) ||
		(Fee_Read(2u, readB2) != E_OK) ||
		(Fee_Read(3u, readB3) != E_OK))
	{
		return E_NOT_OK;
	}

	if ((memcmp(readB1, expectedB1, sizeof(expectedB1)) != 0) ||
		(memcmp(readB2, expectedB2, sizeof(expectedB2)) != 0) ||
		(memcmp(readB3, expectedB3, sizeof(expectedB3)) != 0))
	{
		return E_NOT_OK;
	}

	return E_OK;
}

/* Scenario 2: ghi block1 lan thu 17 de kiem tra layout va sequence sau swap. */
static Std_ReturnType Test_Day4_B1_SeventeenthWrite(void)
{
	uint8_t i;
	uint8_t b1[16];
	uint8_t expectedLatest[16];
	uint8_t readBack[16];
	Test_Day4_RecordHeaderType h0;
	Test_Day4_RecordHeaderType h1;

	(void)remove("flash.bin");
	if (Fee_Init() != E_OK)
	{
		return E_NOT_OK;
	}

	for (i = 0u; i < 17u; i++)
	{
		uint8_t value = (uint8_t)(i + 1u);
		Test_Day4_FillPattern(b1, sizeof(b1), value);
		if (Fee_Write(1u, b1) != E_OK)
		{
			return E_NOT_OK;
		}
		if (i == 16u)
		{
			(void)memcpy(expectedLatest, b1, sizeof(expectedLatest));
		}
	}

	if ((Test_Day4_ReadHeader(1u, 0u, &h0) != E_OK) ||
		(Test_Day4_ReadHeader(1u, 1u, &h1) != E_OK))
	{
		return E_NOT_OK;
	}

	if ((h0.magic != TEST_FEE_RECORD_MAGIC) || (h0.blockId != 1u) || (h0.sequence != 16u))
	{
		return E_NOT_OK;
	}

	if ((h1.magic != TEST_FEE_RECORD_MAGIC) || (h1.blockId != 1u) || (h1.sequence != 17u))
	{
		return E_NOT_OK;
	}

	if (Test_Day4_SectorIsAllFF(0u) != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fee_Init() != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fee_Read(1u, readBack) != E_OK)
	{
		return E_NOT_OK;
	}

	if (memcmp(readBack, expectedLatest, sizeof(expectedLatest)) != 0)
	{
		return E_NOT_OK;
	}

	return E_OK;
}

/* Gate Day 4: tong hop cac tinh huong swap sector. */
Std_ReturnType Test_RunDay4(void)
{
	(void)printf("===== DAY 4 TESTS =====\n");

	if (Test_Day4_MultiWriteSwap() != E_OK)
	{
		(void)printf("[FAIL] Multi-write swap scenario\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Multi-write swap scenario\n");

	if (Test_Day4_B1_SeventeenthWrite() != E_OK)
	{
		(void)printf("[FAIL] B1 seventeenth write scenario\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] B1 seventeenth write scenario\n");

	Fee_ShowLayout();
	(void)printf("DAY 4 GATE: PASS\n");
	return E_OK;
}
