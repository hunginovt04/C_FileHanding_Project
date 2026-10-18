#include "Test_Day3.h"

#include "Fee.h"
#include "Fee_Cfg.h"
#include "Fls_File.h"

#include <limits.h>
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

/* Fill mang du lieu theo 1 mau de so sanh de dang. */
static void Test_Day3_FillPattern(uint8_t *buffer, uint16_t length, uint8_t value)
{
	uint16_t i;
	for (i = 0u; i < length; i++)
	{
		buffer[i] = value;
	}
}

/* Doc header 1 slot de check sequence sau restart. */
static Std_ReturnType Test_Day3_ReadHeader(uint8_t slot, Test_FeeRecordHeaderType *header)
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

/* Ghi truc tiep record raw vao flash de tao testcase sequence boundary. */
static Std_ReturnType Test_Day3_WriteRawRecord(uint8_t slot, uint16_t blockId, uint16_t dataLength, uint32_t sequence, uint8_t pattern)
{
	uint8_t slotBuffer[FEE_SLOT_SIZE];
	Test_FeeRecordHeaderType header;
	uint16_t i;
	uint32_t address = (uint32_t)slot * FEE_SLOT_SIZE;

	if ((dataLength + TEST_FEE_RECORD_HEADER_SIZE) > FEE_SLOT_SIZE)
	{
		return E_NOT_OK;
	}

	(void)memset(slotBuffer, 0xFF, sizeof(slotBuffer));

	header.magic = TEST_FEE_RECORD_MAGIC;
	header.blockId = blockId;
	header.dataLength = dataLength;
	header.reserved = 0u;
	header.sequence = sequence;
	header.reserved2 = 0u;

	(void)memcpy(slotBuffer, &header, sizeof(header));
	for (i = 0u; i < dataLength; i++)
	{
		slotBuffer[TEST_FEE_RECORD_HEADER_SIZE + i] = pattern;
	}

	return Fls_Write(address, slotBuffer, FEE_SLOT_SIZE);
}

/* Case 1: init gap max sequence thi write tiep phai bi chan (exhausted). */
static Std_ReturnType Test_Day3_TestSequenceMaxFromInit(void)
{
	uint8_t readB1[16];
	uint8_t writeB2[32];
	uint16_t i;

	(void)remove("flash.bin");
	if (Fls_Init() != E_OK)
	{
		return E_NOT_OK;
	}

	if (Test_Day3_WriteRawRecord(0u, 1u, 16u, UINT32_MAX, 0x77u) != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fee_Init() != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fee_Read(1u, readB1) != E_OK)
	{
		return E_NOT_OK;
	}

	for (i = 0u; i < 16u; i++)
	{
		if (readB1[i] != 0x77u)
		{
			return E_NOT_OK;
		}
	}

	for (i = 0u; i < 32u; i++)
	{
		writeB2[i] = 0x88u;
	}

	/* Next write must fail because init reconstructed exhausted sequence state. */
	if (Fee_Write(2u, writeB2) != E_NOT_OK)
	{
		return E_NOT_OK;
	}

	return E_OK;
}

/* Case 2: cho phep ghi record co sequence = UINT32_MAX roi moi exhausted. */
static Std_ReturnType Test_Day3_TestWriteToMaxThenExhausted(void)
{
	uint8_t writeB2[32];
	Test_FeeRecordHeaderType h3;
	uint16_t i;

	(void)remove("flash.bin");
	if (Fls_Init() != E_OK)
	{
		return E_NOT_OK;
	}

	/* Prepare maxSequence = UINT32_MAX - 1 so next write uses UINT32_MAX. */
	if (Test_Day3_WriteRawRecord(0u, 1u, 16u, UINT32_MAX - 1u, 0x66u) != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fee_Init() != E_OK)
	{
		return E_NOT_OK;
	}

	for (i = 0u; i < 32u; i++)
	{
		writeB2[i] = 0x99u;
	}

	if (Fee_Write(2u, writeB2) != E_OK)
	{
		return E_NOT_OK;
	}

	if (Test_Day3_ReadHeader(1u, &h3) != E_OK)
	{
		return E_NOT_OK;
	}

	if (h3.sequence != UINT32_MAX)
	{
		return E_NOT_OK;
	}

	/* After successfully writing UINT32_MAX, sequence must be exhausted. */
	if (Fee_Write(3u, (const uint8_t[8]){0x11u,0x11u,0x11u,0x11u,0x11u,0x11u,0x11u,0x11u}) != E_NOT_OK)
	{
		return E_NOT_OK;
	}

	return E_OK;
}

/* Gate Day 3: recovery sau restart + validate behavior sequence sat bien. */
Std_ReturnType Test_RunDay3(void)
{
	uint8_t b1v1[16];
	uint8_t b1v2[16];
	uint8_t b2v1[32];
	uint8_t b3v1[8];
	uint8_t readB1[16];
	uint8_t readB3[8];
	Test_FeeRecordHeaderType h3;

	(void)printf("===== DAY 3 TESTS =====\n");

	(void)remove("flash.bin");

	if (Fee_Init() != E_OK)
	{
		(void)printf("[FAIL] Init run1\n");
		return E_NOT_OK;
	}

	Test_Day3_FillPattern(b1v1, sizeof(b1v1), 0x11u);
	Test_Day3_FillPattern(b1v2, sizeof(b1v2), 0x22u);
	Test_Day3_FillPattern(b2v1, sizeof(b2v1), 0xAAu);
	Test_Day3_FillPattern(b3v1, sizeof(b3v1), 0x55u);

	if ((Fee_Write(1u, b1v1) != E_OK) ||
		(Fee_Write(2u, b2v1) != E_OK) ||
		(Fee_Write(1u, b1v2) != E_OK))
	{
		(void)printf("[FAIL] Run1 writes\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Run1 writes\n");

	if (Fee_Init() != E_OK)
	{
		(void)printf("[FAIL] Init run2 recovery\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Init run2 recovery\n");

	if (Fee_Read(1u, readB1) != E_OK)
	{
		(void)printf("[FAIL] Read B1 after restart\n");
		return E_NOT_OK;
	}
	if (memcmp(readB1, b1v2, sizeof(b1v2)) != 0)
	{
		(void)printf("[FAIL] B1 latest data mismatch\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Read latest B1 after restart\n");

	if (Fee_Write(3u, b3v1) != E_OK)
	{
		(void)printf("[FAIL] Write B3 after restart\n");
		return E_NOT_OK;
	}

	if (Test_Day3_ReadHeader(3u, &h3) != E_OK)
	{
		(void)printf("[FAIL] Read slot3 header\n");
		return E_NOT_OK;
	}

	if ((h3.magic != TEST_FEE_RECORD_MAGIC) ||
		(h3.blockId != 3u) ||
		(h3.dataLength != 8u) ||
		(h3.sequence != 4u) ||
		(h3.reserved != 0u) ||
		(h3.reserved2 != 0u))
	{
		(void)printf("[FAIL] Slot3 header mismatch\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Sequence continues to 4\n");

	if (Fee_Read(3u, readB3) != E_OK)
	{
		(void)printf("[FAIL] Read B3\n");
		return E_NOT_OK;
	}
	if (memcmp(readB3, b3v1, sizeof(b3v1)) != 0)
	{
		(void)printf("[FAIL] B3 data mismatch\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Read B3 data\n");

	if (Fee_Read(99u, readB1) != E_NOT_OK)
	{
		(void)printf("[FAIL] Invalid blockId should fail\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Invalid block read rejected\n");

	if (Test_Day3_TestSequenceMaxFromInit() != E_OK)
	{
		(void)printf("[FAIL] UINT32_MAX from init behavior\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] UINT32_MAX from init behavior\n");

	if (Test_Day3_TestWriteToMaxThenExhausted() != E_OK)
	{
		(void)printf("[FAIL] Write UINT32_MAX then exhausted behavior\n");
		return E_NOT_OK;
	}
	(void)printf("[PASS] Write UINT32_MAX then exhausted behavior\n");

	Fee_ShowLayout();
	(void)printf("DAY 3 GATE: PASS\n");
	return E_OK;
}
