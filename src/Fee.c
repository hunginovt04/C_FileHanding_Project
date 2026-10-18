#include "Fee.h"

#include "Fee_Cfg.h"
#include "Fls_File.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define FEE_RECORD_MAGIC 0xA55Au
#define FEE_RECORD_HEADER_SIZE 16u
#define FEE_SLOT_DATA_MAX (FEE_SLOT_SIZE - FEE_RECORD_HEADER_SIZE)

typedef struct
{
	uint16_t magic;
	uint16_t blockId;
	uint16_t dataLength;
	uint16_t reserved;
	uint32_t sequence;
	uint32_t reserved2;
} Fee_RecordHeaderType;

static uint8_t Fee_Initialized = 0u;
static uint8_t Fee_ActiveSector = 0u;
static uint8_t Fee_NextFreeSlot = 0u;
static uint32_t Fee_NextSequence = 1u;

typedef struct
{
	uint8_t hasData;
	uint8_t nextFreeSlot;
	uint32_t maxSequence;
} Fee_SectorInfoType;

typedef struct
{
	uint8_t found;
	uint8_t slot;
	uint32_t sequence;
} Fee_LatestInfoType;

static Std_ReturnType Fee_BuildLatestMap(uint8_t sectorId, Fee_LatestInfoType latestMap[FEE_MAX_BLOCKS]);
static Std_ReturnType Fee_ScanSector(uint8_t sectorId, Fee_SectorInfoType *info);
static Std_ReturnType Fee_Swap(void);

/* Tinh dia chi vat ly cua 1 slot trong sector. */
static uint32_t Fee_GetSlotAddress(uint8_t sectorId, uint8_t slotIndex)
{
	uint32_t sectorBase;

	sectorBase = (uint32_t)sectorId * FLS_SECTOR_SIZE;

	return sectorBase + ((uint32_t)slotIndex * FEE_SLOT_SIZE);
}

/* Kiem tra vung nho co toan 0xFF hay khong (slot trong/trang thai erase). */
static uint8_t Fee_IsAllFF(const uint8_t *buffer, uint32_t length)
{
	uint32_t i;

	for (i = 0u; i < length; i++)
	{
		if (buffer[i] != 0xFFu)
		{
			return 0u;
		}
	}

	return 1u;
}

/* Tim chi so cau hinh block theo blockId logic. */
static int32_t Fee_FindBlockIndex(uint16_t blockId)
{
	uint32_t i;

	for (i = 0u; i < FEE_MAX_BLOCKS; i++)
	{
		if (Fee_BlockConfig[i].blockId == blockId)
		{
			return (int32_t)i;
		}
	}

	return -1;
}

/* Quet 1 sector va lay record moi nhat (sequence lon nhat) cho tung block. */
static Std_ReturnType Fee_BuildLatestMap(uint8_t sectorId, Fee_LatestInfoType latestMap[FEE_MAX_BLOCKS])
{
	uint32_t blockIdx;
	uint8_t slot;

	for (blockIdx = 0u; blockIdx < FEE_MAX_BLOCKS; blockIdx++)
	{
		latestMap[blockIdx].found = 0u;
		latestMap[blockIdx].slot = FEE_SLOTS_PER_SECTOR;
		latestMap[blockIdx].sequence = 0u;
	}

	for (slot = 0u; slot < FEE_SLOTS_PER_SECTOR; slot++)
	{
		uint8_t slotBuffer[FEE_SLOT_SIZE];
		uint32_t address = Fee_GetSlotAddress(sectorId, slot);
		Fee_RecordHeaderType header;
		int32_t blockIndex;

		if (Fls_Read(address, slotBuffer, FEE_SLOT_SIZE) != E_OK)
		{
			return E_NOT_OK;
		}

		if (Fee_IsAllFF(slotBuffer, FEE_SLOT_SIZE) == 1u)
		{
			continue;
		}

		(void)memcpy(&header, slotBuffer, sizeof(header));
		if (header.magic != FEE_RECORD_MAGIC)
		{
			return E_NOT_OK;
		}

		blockIndex = Fee_FindBlockIndex(header.blockId);
		if (blockIndex < 0)
		{
			return E_NOT_OK;
		}

		if (header.dataLength != Fee_BlockConfig[(uint32_t)blockIndex].dataSize)
		{
			return E_NOT_OK;
		}

		if (header.dataLength > FEE_SLOT_DATA_MAX)
		{
			return E_NOT_OK;
		}

		if (header.sequence == 0u)
		{
			return E_NOT_OK;
		}

		if ((header.reserved != 0u) || (header.reserved2 != 0u))
		{
			return E_NOT_OK;
		}

		if (Fee_IsAllFF(&slotBuffer[FEE_RECORD_HEADER_SIZE + header.dataLength],
						FEE_SLOT_SIZE - (FEE_RECORD_HEADER_SIZE + header.dataLength)) == 0u)
		{
			return E_NOT_OK;
		}

		if (header.sequence > latestMap[(uint32_t)blockIndex].sequence)
		{
			latestMap[(uint32_t)blockIndex].found = 1u;
			latestMap[(uint32_t)blockIndex].slot = slot;
			latestMap[(uint32_t)blockIndex].sequence = header.sequence;
		}
	}

	return E_OK;
}

/* Swap sector: copy latest records sang sector spare, erase sector cu, doi active sector. */
static Std_ReturnType Fee_Swap(void)
{
	uint8_t oldActive = Fee_ActiveSector;
	uint8_t spare = (oldActive == 0u) ? 1u : 0u;
	Fee_SectorInfoType spareInfo;
	Fee_LatestInfoType latestMap[FEE_MAX_BLOCKS];
	uint8_t copySlot = 0u;
	uint32_t blockIdx;

	if (Fee_ScanSector(spare, &spareInfo) != E_OK)
	{
		return E_NOT_OK;
	}

	if (spareInfo.hasData == 1u)
	{
		return E_NOT_OK;
	}

	if (Fee_BuildLatestMap(oldActive, latestMap) != E_OK)
	{
		return E_NOT_OK;
	}

	for (blockIdx = 0u; blockIdx < FEE_MAX_BLOCKS; blockIdx++)
	{
		uint8_t slotBuffer[FEE_SLOT_SIZE];

		if (latestMap[blockIdx].found == 0u)
		{
			continue;
		}

		if (Fls_Read(Fee_GetSlotAddress(oldActive, latestMap[blockIdx].slot), slotBuffer, FEE_SLOT_SIZE) != E_OK)
		{
			(void)Fls_EraseSector(spare);
			return E_NOT_OK;
		}

		if (Fls_Write(Fee_GetSlotAddress(spare, copySlot), slotBuffer, FEE_SLOT_SIZE) != E_OK)
		{
			(void)Fls_EraseSector(spare);
			return E_NOT_OK;
		}

		copySlot++;
	}

	if (Fls_EraseSector(oldActive) != E_OK)
	{
		(void)Fls_EraseSector(spare);
		return E_NOT_OK;
	}

	Fee_ActiveSector = spare;
	Fee_NextFreeSlot = copySlot;

	return E_OK;
}

/* Quet/validate sector de khoi phuc trang thai noi bo khi init. */
static Std_ReturnType Fee_ScanSector(uint8_t sectorId, Fee_SectorInfoType *info)
{
	uint8_t slot;
	uint8_t freeSeen = 0u;
	uint8_t usedCount = 0u;
	uint32_t seenSeq[FEE_SLOTS_PER_SECTOR];
	uint8_t seenCount = 0u;

	info->hasData = 0u;
	info->nextFreeSlot = 0u;
	info->maxSequence = 0u;

	for (slot = 0u; slot < FEE_SLOTS_PER_SECTOR; slot++)
	{
		uint8_t slotBuffer[FEE_SLOT_SIZE];
		uint32_t address = Fee_GetSlotAddress(sectorId, slot);
		Fee_RecordHeaderType header;
		int32_t blockIndex = -1;
		uint8_t i;

		if (Fls_Read(address, slotBuffer, FEE_SLOT_SIZE) != E_OK)
		{
			return E_NOT_OK;
		}

		if (Fee_IsAllFF(slotBuffer, FEE_SLOT_SIZE) == 1u)
		{
			freeSeen = 1u;
			continue;
		}

		(void)memcpy(&header, slotBuffer, sizeof(header));
		if (header.magic != FEE_RECORD_MAGIC)
		{
			return E_NOT_OK;
		}

		blockIndex = Fee_FindBlockIndex(header.blockId);
		if (blockIndex < 0)
		{
			return E_NOT_OK;
		}

		if (header.dataLength != Fee_BlockConfig[(uint32_t)blockIndex].dataSize)
		{
			return E_NOT_OK;
		}

		if (header.dataLength > FEE_SLOT_DATA_MAX)
		{
			return E_NOT_OK;
		}

		if (header.sequence == 0u)
		{
			return E_NOT_OK;
		}

		if ((header.reserved != 0u) || (header.reserved2 != 0u))
		{
			return E_NOT_OK;
		}

		if (Fee_IsAllFF(&slotBuffer[FEE_RECORD_HEADER_SIZE + header.dataLength], FEE_SLOT_SIZE - (FEE_RECORD_HEADER_SIZE + header.dataLength)) == 0u)
		{
			return E_NOT_OK;
		}

		if (freeSeen == 1u)
		{
			return E_NOT_OK;
		}
		// Check trung lap sequence (duplicate sequence)
		for (i = 0u; i < seenCount; i++)
		{
			if (seenSeq[i] == header.sequence)
			{
				return E_NOT_OK;
			}
		}
		seenSeq[seenCount] = header.sequence;
		seenCount++;

		usedCount++;
		info->hasData = 1u;
		if (header.sequence > info->maxSequence)
		{
			info->maxSequence = header.sequence;
		}
	}

	info->nextFreeSlot = usedCount;
	return E_OK;
}
/* Khoi tao Fee va khoi phuc active sector, next slot, next sequence tu flash. */
Std_ReturnType Fee_Init(void)
{
	Fee_SectorInfoType sectorA;
	Fee_SectorInfoType sectorB;
	Fee_SectorInfoType *activeInfo;

	if (Fls_Init() != E_OK)
	{
		Fee_Initialized = 0u;
		return E_NOT_OK;
	}

	if ((Fee_ScanSector(0u, &sectorA) != E_OK) ||
		(Fee_ScanSector(1u, &sectorB) != E_OK))
	{
		Fee_Initialized = 0u;
		return E_NOT_OK;
	}

	if ((sectorA.hasData == 0u) && (sectorB.hasData == 0u))
	{
		Fee_ActiveSector = 0u;
		Fee_NextFreeSlot = 0u;
		Fee_NextSequence = 1u;
		Fee_Initialized = 1u;
		return E_OK;
	}

	if ((sectorA.hasData == 1u) && (sectorB.hasData == 1u))
	{
		Fee_Initialized = 0u;
		return E_NOT_OK;
	}

	if (sectorA.hasData == 1u)
	{
		Fee_ActiveSector = 0u;
		activeInfo = &sectorA;
	}
	else
	{
		Fee_ActiveSector = 1u;
		activeInfo = &sectorB;
	}

	Fee_NextFreeSlot = activeInfo->nextFreeSlot;
	if (activeInfo->maxSequence == UINT32_MAX)
	{
		Fee_NextSequence = 0u;
	}
	else
	{
		Fee_NextSequence = activeInfo->maxSequence + 1u;
	}

	Fee_Initialized = 1u;
	return E_OK;
}

/* Ghi append 1 record moi; neu full thi swap truoc roi moi ghi. */
Std_ReturnType Fee_Write(uint16_t blockId, const uint8_t *data)
{
	int32_t blockIndex;
	Fee_RecordHeaderType header;
	uint8_t slotBuffer[FEE_SLOT_SIZE];
	uint32_t address;

	if (Fee_Initialized == 0u)
	{
		return E_NOT_OK;
	}

	if (data == NULL)
	{
		return E_NOT_OK;
	}

	blockIndex = Fee_FindBlockIndex(blockId);
	if (blockIndex < 0)
	{
		return E_NOT_OK;
	}

	if (Fee_BlockConfig[(uint32_t)blockIndex].dataSize > FEE_SLOT_DATA_MAX)
	{
		return E_NOT_OK;
	}

	if (Fee_NextSequence == 0u)
	{
		return E_NOT_OK;
	}

	if (Fee_NextFreeSlot >= FEE_SLOTS_PER_SECTOR)
	{
		if (Fee_Swap() != E_OK)
		{
			return E_NOT_OK;
		}

		if (Fee_NextFreeSlot >= FEE_SLOTS_PER_SECTOR)
		{
			return E_NOT_OK;
		}
	}

	(void)memset(slotBuffer, 0xFF, sizeof(slotBuffer));

	header.magic = FEE_RECORD_MAGIC;
	header.blockId = blockId;
	header.dataLength = Fee_BlockConfig[(uint32_t)blockIndex].dataSize;
	header.reserved = 0u;
	header.sequence = Fee_NextSequence;
	header.reserved2 = 0u;

	(void)memcpy(slotBuffer, &header, sizeof(header));
	(void)memcpy(&slotBuffer[FEE_RECORD_HEADER_SIZE], data, Fee_BlockConfig[(uint32_t)blockIndex].dataSize);

	address = Fee_GetSlotAddress(Fee_ActiveSector, Fee_NextFreeSlot);
	if (Fls_Write(address, slotBuffer, FEE_SLOT_SIZE) != E_OK)
	{
		return E_NOT_OK;
	}

	Fee_NextFreeSlot++;
	if (Fee_NextSequence == UINT32_MAX)
	{
		Fee_NextSequence = 0u;
	}
	else
	{
		Fee_NextSequence++;
	}

	return E_OK;
}

/* Doc block logic: tim latest record theo sequence lon nhat va copy payload cho user. */
Std_ReturnType Fee_Read(uint16_t blockId, uint8_t *data)
{
	int32_t blockIndex;
	uint16_t dataSize;
	Fee_LatestInfoType latestMap[FEE_MAX_BLOCKS];

	if (Fee_Initialized == 0u)
	{
		return E_NOT_OK;
	}

	if (data == NULL)
	{
		return E_NOT_OK;
	}

	blockIndex = Fee_FindBlockIndex(blockId);
	if (blockIndex < 0)
	{
		return E_NOT_OK;
	}

	if (Fee_BuildLatestMap(Fee_ActiveSector, latestMap) != E_OK)
	{
		return E_NOT_OK;
	}

	if (latestMap[blockIndex].found == 0u)
	{
		return E_NOT_OK;
	}

	dataSize = Fee_BlockConfig[(uint32_t)blockIndex].dataSize;
	if (Fls_Read(Fee_GetSlotAddress(Fee_ActiveSector, latestMap[(uint32_t)blockIndex].slot) + FEE_RECORD_HEADER_SIZE,
				 data,
				 dataSize)
				  != E_OK)
	{
		return E_NOT_OK;
	}

	return E_OK;
}

/* In layout de quan sat record moi/record cu va slot trong. */
void Fee_ShowLayout(void)
{
	uint8_t slot;
	Fee_LatestInfoType latestMap[FEE_MAX_BLOCKS];
	uint8_t slotBuffer[FEE_SLOT_SIZE];

	if (Fee_Initialized == 0u)
	{
		(void)printf("Fee not initialized\n");
		return;
	}

	if (Fee_BuildLatestMap(Fee_ActiveSector, latestMap) != E_OK)
	{
		(void)printf("Active sector layout invalid\n");
		return;
	}

	(void)printf("========================================\n");
	(void)printf("             FEE FLASH LAYOUT\n");
	(void)printf("========================================\n\n");
	(void)printf("Sector %c : ACTIVE\n\n", (Fee_ActiveSector == 0u) ? 'A' : 'B');

	for (slot = 0u; slot < FEE_SLOTS_PER_SECTOR; slot++)
	{
		Fee_RecordHeaderType header;
		int32_t blockIndex;
		uint32_t address = Fee_GetSlotAddress(Fee_ActiveSector, slot);

		if (Fls_Read(address, slotBuffer, FEE_SLOT_SIZE) != E_OK)
		{
			(void)printf("Slot %02u : [!] READ_ERROR\n", (unsigned int)slot);
			continue;
		}

		if (Fee_IsAllFF(slotBuffer, FEE_SLOT_SIZE) == 1u)
		{
			(void)printf("Slot %02u : [.] FREE\n", (unsigned int)slot);
			continue;
		}

		(void)memcpy(&header, slotBuffer, sizeof(header));
		if (header.magic != FEE_RECORD_MAGIC)
		{
			(void)printf("Slot %02u : [!] INVALID\n", (unsigned int)slot);
			continue;
		}

		blockIndex = Fee_FindBlockIndex(header.blockId);
		if (blockIndex < 0)
		{
			(void)printf("Slot %02u : [!] INVALID\n", (unsigned int)slot);
			continue;
		}

		if ((latestMap[(uint32_t)blockIndex].found == 1u) &&
			(header.sequence == latestMap[(uint32_t)blockIndex].sequence))
		{
			(void)printf("Slot %02u : [V] B%02u Seq=%03lu\n",
						 (unsigned int)slot,
						 (unsigned int)header.blockId,
						 (unsigned long)header.sequence);
		}
		else
		{
			(void)printf("Slot %02u : [X] B%02u Seq=%03lu\n",
						 (unsigned int)slot,
						 (unsigned int)header.blockId,
						 (unsigned long)header.sequence);
		}
	}

	(void)printf("\nSector %c : ERASED\n\n", (Fee_ActiveSector == 0u) ? 'B' : 'A');
	for (slot = 0u; slot < FEE_SLOTS_PER_SECTOR; slot++)
	{
		(void)printf("Slot %02u : [.] FREE\n", (unsigned int)slot);
	}
}
