#include "Test_Day2.h"

#include "Fee.h"
#include "Fee_Cfg.h"
#include "Fls_File.h"

#include <stdio.h>
#include <string.h>

#define TEST_FEE_RECORD_MAGIC        0xA55Au
#define TEST_FEE_RECORD_HEADER_SIZE  16u

typedef struct
{
	uint16_t magic;
	uint16_t blockId;
	uint16_t dataLength;
	uint16_t reserved;
	uint32_t sequence;
	uint32_t reserved2;
} Test_FeeRecordHeaderType;

/* Tao pattern de de nhin du lieu khi doc/so sanh. */
static void Test_FillPattern(uint8_t *buffer, uint16_t length, uint8_t value)
{
	uint16_t i;
	for (i = 0u; i < length; i++)
	{
		buffer[i] = value;
	}
}

/* Doc header tai 1 slot de verify sequence va thong tin record. */
static Std_ReturnType Test_Day2_ReadHeader(uint8_t slot, Test_FeeRecordHeaderType *header)
{
	uint8_t slotBuffer[FEE_SLOT_SIZE];
	uint32_t address = (uint32_t)slot * FEE_SLOT_SIZE;

	if (Fls_Read(address, slotBuffer, FEE_SLOT_SIZE) != E_OK)
	{
		return E_NOT_OK;
	}

	(void)memcpy(header, slotBuffer, sizeof(*header));
	return E_OK;
}

/* Kiem tra slot free (toan 0xFF). */
static Std_ReturnType Test_Day2_ExpectFreeSlot(uint8_t slot)
{
	uint8_t slotBuffer[FEE_SLOT_SIZE];
	uint8_t i;
	uint32_t address = (uint32_t)slot * FEE_SLOT_SIZE;

	if (Fls_Read(address, slotBuffer, FEE_SLOT_SIZE) != E_OK)
	{
		return E_NOT_OK;
	}

	for (i = 0u; i < FEE_SLOT_SIZE; i++)
	{
		if (slotBuffer[i] != 0xFFu)
		{
			return E_NOT_OK;
		}
	}

	return E_OK;
}

/* Gate Day 2: init sach, write append, verify header/payload/layout co ban. */
Std_ReturnType Test_RunDay2(void)
{
	uint8_t b1v1[16];
	uint8_t b1v2[16];
	uint8_t b2v1[32];
	Test_FeeRecordHeaderType h0;
	Test_FeeRecordHeaderType h1;
	Test_FeeRecordHeaderType h2;
	uint8_t slotData[32];

	(void)printf("===== DAY 2 TESTS =====\n");

	(void)remove("flash.bin");

	if (Fee_Init() != E_OK)
	{
		(void)printf("[FAIL] Fee_Init clean flash\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Fee_Init clean flash\n");

	Test_FillPattern(b1v1, sizeof(b1v1), 0x11u);
	Test_FillPattern(b2v1, sizeof(b2v1), 0xAAu);
	Test_FillPattern(b1v2, sizeof(b1v2), 0x22u);

	if (Fee_Write(1u, b1v1) != E_OK)
	{
		(void)printf("[FAIL] Fee_Write B1 v1\n");
		return E_NOT_OK;
	}

	if (Fee_Write(2u, b2v1) != E_OK)
	{
		(void)printf("[FAIL] Fee_Write B2 v1\n");
		return E_NOT_OK;
	}

	if (Fee_Write(1u, b1v2) != E_OK)
	{
		(void)printf("[FAIL] Fee_Write B1 v2\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Fee_Write append 3 records\n");

	if (Test_Day2_ReadHeader(0u, &h0) != E_OK ||
		Test_Day2_ReadHeader(1u, &h1) != E_OK ||
		Test_Day2_ReadHeader(2u, &h2) != E_OK)
	{
		(void)printf("[FAIL] Read headers\n");
		return E_NOT_OK;
	}

	if ((h0.magic != TEST_FEE_RECORD_MAGIC) || (h0.blockId != 1u) || (h0.dataLength != 16u) || (h0.sequence != 1u) ||
		(h1.magic != TEST_FEE_RECORD_MAGIC) || (h1.blockId != 2u) || (h1.dataLength != 32u) || (h1.sequence != 2u) ||
		(h2.magic != TEST_FEE_RECORD_MAGIC) || (h2.blockId != 1u) || (h2.dataLength != 16u) || (h2.sequence != 3u))
	{
		(void)printf("[FAIL] Header content / sequence\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Header content / sequence\n");

	if (Fls_Read((0u * FEE_SLOT_SIZE) + TEST_FEE_RECORD_HEADER_SIZE, slotData, 16u) != E_OK)
	{
		(void)printf("[FAIL] Read slot0 data\n");
		return E_NOT_OK;
	}
	if (memcmp(slotData, b1v1, 16u) != 0)
	{
		(void)printf("[FAIL] Slot0 payload mismatch\n");
		return E_NOT_OK;
	}

	if (Fls_Read((2u * FEE_SLOT_SIZE) + TEST_FEE_RECORD_HEADER_SIZE, slotData, 16u) != E_OK)
	{
		(void)printf("[FAIL] Read slot2 data\n");
		return E_NOT_OK;
	}
	if (memcmp(slotData, b1v2, 16u) != 0)
	{
		(void)printf("[FAIL] Slot2 payload mismatch\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Payload write check\n");

	if (Test_Day2_ExpectFreeSlot(3u) != E_OK)
	{
		(void)printf("[FAIL] Slot3 should be FREE\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Next slot is FREE\n");

	Fee_ShowLayout();
	(void)printf("DAY 2 GATE: PASS\n");

	return E_OK;
}
