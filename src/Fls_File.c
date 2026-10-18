#include "Fls_File.h"

#include <stdio.h>

static const char *Fls_FileName = "flash.bin";

/* Kiem tra pham vi truy cap de tranh vuot kich thuoc flash mo phong. */
static Std_ReturnType Fls_CheckRange(uint32_t address, uint32_t length)
{
	if (length == 0u)
	{
		return E_NOT_OK;
	}

	if (address > FLS_TOTAL_SIZE)
	{
		return E_NOT_OK;
	}

	if (length > (FLS_TOTAL_SIZE - address))
	{
		return E_NOT_OK;
	}

	return E_OK;
}

/* Khoi tao flash file: tao moi va fill 0xFF neu chua ton tai, neu da ton tai thi chi check size. */
Std_ReturnType Fls_Init(void)
{
	FILE *fp;
	long size;

	fp = fopen(Fls_FileName, "rb");
	if (fp == NULL)
	{
		uint32_t remaining;
		uint8_t fill[128];
		uint32_t i;

		for (i = 0u; i < (uint32_t)sizeof(fill); i++)
		{
			fill[i] = 0xFFu;
		}

		fp = fopen(Fls_FileName, "wb");
		if (fp == NULL)
		{
			return E_NOT_OK;
		}

		remaining = FLS_TOTAL_SIZE;
		while (remaining > 0u)
		{
			uint32_t chunk = (remaining > (uint32_t)sizeof(fill)) ? (uint32_t)sizeof(fill) : remaining;
			if (fwrite(fill, 1u, chunk, fp) != chunk)
			{
				(void)fclose(fp);
				return E_NOT_OK;
			}
			remaining -= chunk;
		}

		if (fflush(fp) != 0)
		{
			(void)fclose(fp);
			return E_NOT_OK;
		}

		if (fclose(fp) != 0)
		{
			return E_NOT_OK;
		}

		return E_OK;
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

	if ((uint32_t)size != FLS_TOTAL_SIZE)
	{
		return E_NOT_OK;
	}

	return E_OK;
}

/* Doc du lieu tu dia chi vat ly trong flash mo phong. */
Std_ReturnType Fls_Read(uint32_t address, uint8_t *data, uint32_t length)
{
	FILE *fp;

	if (data == NULL)
	{
		return E_NOT_OK;
	}

	if (Fls_CheckRange(address, length) != E_OK)
	{
		return E_NOT_OK;
	}

	fp = fopen(Fls_FileName, "rb");
	if (fp == NULL)
	{
		return E_NOT_OK;
	}

	if (fseek(fp, (long)address, SEEK_SET) != 0)
	{
		(void)fclose(fp);
		return E_NOT_OK;
	}

	if (fread(data, 1u, length, fp) != length)
	{
		(void)fclose(fp);
		return E_NOT_OK;
	}

	if (fclose(fp) != 0)
	{
		return E_NOT_OK;
	}

	return E_OK;
}

/* Ghi du lieu vao dia chi vat ly trong flash mo phong. */
Std_ReturnType Fls_Write(uint32_t address, const uint8_t *data, uint32_t length)
{
	FILE *fp;

	if (data == NULL)
	{
		return E_NOT_OK;
	}

	if (Fls_CheckRange(address, length) != E_OK)
	{
		return E_NOT_OK;
	}

	fp = fopen(Fls_FileName, "rb+");
	if (fp == NULL)
	{
		return E_NOT_OK;
	}

	if (fseek(fp, (long)address, SEEK_SET) != 0)
	{
		(void)fclose(fp);
		return E_NOT_OK;
	}

	if (fwrite(data, 1u, length, fp) != length)
	{
		(void)fclose(fp);
		return E_NOT_OK;
	}

	if (fflush(fp) != 0)
	{
		(void)fclose(fp);
		return E_NOT_OK;
	}

	if (fclose(fp) != 0)
	{
		return E_NOT_OK;
	}

	return E_OK;
}

/* Erase 1 sector bang cach ghi lai toan bo sector ve 0xFF. */
Std_ReturnType Fls_EraseSector(uint8_t sectorId)
{
	FILE *fp;
	uint32_t address;
	uint32_t remaining;
	uint8_t fill[128];
	uint32_t i;

	if (sectorId >= FLS_NUMBER_OF_SECTORS)
	{
		return E_NOT_OK;
	}

	for (i = 0u; i < (uint32_t)sizeof(fill); i++)
	{
		fill[i] = 0xFFu;
	}

	address = (uint32_t)sectorId * FLS_SECTOR_SIZE;

	fp = fopen(Fls_FileName, "rb+");
	if (fp == NULL)
	{
		return E_NOT_OK;
	}

	if (fseek(fp, (long)address, SEEK_SET) != 0)
	{
		(void)fclose(fp);
		return E_NOT_OK;
	}

	remaining = FLS_SECTOR_SIZE;
	while (remaining > 0u)
	{
		uint32_t chunk = (remaining > (uint32_t)sizeof(fill)) ? (uint32_t)sizeof(fill) : remaining;
		if (fwrite(fill, 1u, chunk, fp) != chunk)
		{
			(void)fclose(fp);
			return E_NOT_OK;
		}
		remaining -= chunk;
	}

	if (fflush(fp) != 0)
	{
		(void)fclose(fp);
		return E_NOT_OK;
	}

	if (fclose(fp) != 0)
	{
		return E_NOT_OK;
	}

	return E_OK;
}

/* Ham dump hexdump de debug nhanh noi dung flash (khong dung trong luong core). */
void Fls_Dump(uint32_t address, uint32_t length)
{
	uint8_t value;
	uint32_t i;

	if (Fls_CheckRange(address, length) != E_OK)
	{
		return;
	}

	for (i = 0u; i < length; i++)
	{
		if ((i % 16u) == 0u)
		{
			(void)printf("\n0x%04X: ", (unsigned int)(address + i));
		}

		if (Fls_Read(address + i, &value, 1u) != E_OK)
		{
			(void)printf("?? ");
		}
		else
		{
			(void)printf("%02X ", value);
		}
	}

	(void)printf("\n");
}
