#include "Test_Day1.h"

#include "Fls_File.h"

#include <stdio.h>
#include <string.h>

/* Kiem tra kich thuoc flash.bin dung theo cau hinh bai tap. */
static Std_ReturnType Test_CheckFlashSize(void)
{
	FILE *fp;
	long size;

	fp = fopen("flash.bin", "rb");
	if (fp == NULL)
	{
		return E_NOT_OK;
	}

	if (fseek(fp, 0L, SEEK_END) != 0)
	{
		(void)fclose(fp);
		return E_NOT_OK;
	}

	size = ftell(fp);
	if (size < 0L)
	{
		(void)fclose(fp);
		return E_NOT_OK;
	}

	if (fclose(fp) != 0)
	{
		return E_NOT_OK;
	}

	return ((uint32_t)size == FLS_TOTAL_SIZE) ? E_OK : E_NOT_OK;
}

/* Test 1: khoi tao file flash moi va xac nhan size. */
static Std_ReturnType Test1_CreateFlash(void)
{
	(void)remove("flash.bin");

	if (Fls_Init() != E_OK)
	{
		return E_NOT_OK;
	}

	return Test_CheckFlashSize();
}

/* Test 2: ghi/ doc lai du lieu co dung khong. */
static Std_ReturnType Test2_WriteRead(void)
{
	uint8_t writeData[4] = {0xAAu, 0xBBu, 0xCCu, 0xDDu};
	uint8_t readData[4] = {0u, 0u, 0u, 0u};

	if (Fls_Write(0x20u, writeData, sizeof(writeData)) != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fls_Read(0x20u, readData, sizeof(readData)) != E_OK)
	{
		return E_NOT_OK;
	}

	return (memcmp(writeData, readData, sizeof(writeData)) == 0) ? E_OK : E_NOT_OK;
}

/* Test 3: khoi tao lai module nhung du lieu van con (persistence). */
static Std_ReturnType Test3_Persistence(void)
{
	uint8_t writeData[4] = {0x11u, 0x22u, 0x33u, 0x44u};
	uint8_t readData[4] = {0u, 0u, 0u, 0u};

	if (Fls_Write(0x24u, writeData, sizeof(writeData)) != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fls_Init() != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fls_Read(0x24u, readData, sizeof(readData)) != E_OK)
	{
		return E_NOT_OK;
	}

	return (memcmp(writeData, readData, sizeof(writeData)) == 0) ? E_OK : E_NOT_OK;
}

/* Test 4: erase sector A va dam bao sector B khong bi anh huong. */
static Std_ReturnType Test4_EraseSector(void)
{
	uint8_t valueA = 0x00u;
	uint8_t valueB = 0x00u;
	const uint8_t markerA = 0x5Au;
	const uint8_t markerB = 0xA5u;

	if (Fls_Write(0x0010u, &markerA, 1u) != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fls_Write(0x0410u, &markerB, 1u) != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fls_EraseSector(0u) != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fls_Read(0x0010u, &valueA, 1u) != E_OK)
	{
		return E_NOT_OK;
	}

	if (Fls_Read(0x0410u, &valueB, 1u) != E_OK)
	{
		return E_NOT_OK;
	}

	if (valueA != 0xFFu)
	{
		return E_NOT_OK;
	}

	if (valueB != markerB)
	{
		return E_NOT_OK;
	}

	return E_OK;
}

/* Chay tong hop gate Day 1. */
Std_ReturnType Test_RunDay1(void)
{
	uint32_t passed = 0u;
	uint32_t total = 4u;

	(void)printf("===== DAY 1 TESTS =====\n");

	if (Test1_CreateFlash() == E_OK)
	{
		passed++;
		(void)printf("[PASS] Test1_CreateFlash\n");
	}
	else
	{
		(void)printf("[FAIL] Test1_CreateFlash\n");
	}

	if (Test2_WriteRead() == E_OK)
	{
		passed++;
		(void)printf("[PASS] Test2_WriteRead\n");
	}
	else
	{
		(void)printf("[FAIL] Test2_WriteRead\n");
	}

	if (Test3_Persistence() == E_OK)
	{
		passed++;
		(void)printf("[PASS] Test3_Persistence\n");
	}
	else
	{
		(void)printf("[FAIL] Test3_Persistence\n");
	}

	if (Test4_EraseSector() == E_OK)
	{
		passed++;
		(void)printf("[PASS] Test4_EraseSector\n");
	}
	else
	{
		(void)printf("[FAIL] Test4_EraseSector\n");
	}

	(void)printf("Summary: %u/%u passed\n", (unsigned int)passed, (unsigned int)total);

	return (passed == total) ? E_OK : E_NOT_OK;
}
