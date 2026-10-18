#include "Fee.h"

#include <stdio.h>
#include <string.h>

static int Cli_InitWithRecovery(void);
static int Cli_ExecuteToken(const char *token, uint8_t *b1Version, uint8_t *b2Version, uint8_t *b3Version);

static void Cli_WriteBlock1(uint8_t *version);
static void Cli_WriteBlock2(uint8_t *version);
static void Cli_WriteBlock3(uint8_t *version);
static void Cli_ReadBlock1(void);
static void Cli_ReadBlock2(void);
static void Cli_ReadBlock3(void);

static void Cli_FillPattern(uint8_t *buffer, uint16_t length, uint8_t value)
{
    uint16_t i;

    for (i = 0u; i < length; i++)
    {
        buffer[i] = value;
    }
}

static void Cli_PrintBuffer(const uint8_t *buffer, uint16_t length)
{
    uint16_t i;

    for (i = 0u; i < length; i++)
    {
        (void)printf("%02X ", buffer[i]);
        if (((i + 1u) % 16u) == 0u)
        {
            (void)printf("\n");
        }
    }

    if ((length % 16u) != 0u)
    {
        (void)printf("\n");
    }
}

static void Cli_ShowMenu(void)
{
    (void)printf("\n================================\n");
    (void)printf("Mock Fee CLI\n");
    (void)printf("================================\n");
    (void)printf("1. Write Block 1\n");
    (void)printf("2. Write Block 2\n");
    (void)printf("3. Write Block 3\n");
    (void)printf("4. Read Block 1\n");
    (void)printf("5. Read Block 2\n");
    (void)printf("6. Read Block 3\n");
    (void)printf("7. Show Layout\n");
    (void)printf("Lenh chu: write 1|2|3, read 1|2|3, show, exit\n");
    (void)printf("0. Exit\n");
    (void)printf("Lua chon: ");
    (void)fflush(stdout);
}

/* Khoi tao Fee; neu fail thi cho phep reset flash de tiep tuc demo. */
static int Cli_InitWithRecovery(void)
{
    char answer[16];

    if (Fee_Init() == E_OK)
    {
        (void)printf("Fee_Init PASS\n");
        return 1;
    }

    (void)printf("Fee_Init FAIL (layout cu co the khong hop le).\n");
    (void)printf("Ban co muon reset flash.bin va khoi tao lai khong? (y/n): ");
    (void)fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL)
    {
        return 0;
    }

    if ((answer[0] != 'y') && (answer[0] != 'Y'))
    {
        return 0;
    }

    (void)remove("flash.bin");

    if (Fee_Init() != E_OK)
    {
        (void)printf("Fee_Init FAIL ngay ca sau reset flash.bin\n");
        return 0;
    }

    (void)printf("Fee_Init PASS sau khi reset flash.bin\n");
    return 1;
}

static int Cli_ExecuteChoice(int choice, uint8_t *b1Version, uint8_t *b2Version, uint8_t *b3Version)
{
    switch (choice)
    {
        case 1:
            Cli_WriteBlock1(b1Version);
            break;
        case 2:
            Cli_WriteBlock2(b2Version);
            break;
        case 3:
            Cli_WriteBlock3(b3Version);
            break;
        case 4:
            Cli_ReadBlock1();
            break;
        case 5:
            Cli_ReadBlock2();
            break;
        case 6:
            Cli_ReadBlock3();
            break;
        case 7:
            Fee_ShowLayout();
            break;
        case 0:
            (void)printf("Bye\n");
            return 1;
        default:
            (void)printf("Lua chon khong hop le\n");
            break;
    }

    return 0;
}

/* Ho tro ca menu so va command text theo goi y cua de bai. */
static int Cli_ExecuteToken(const char *token, uint8_t *b1Version, uint8_t *b2Version, uint8_t *b3Version)
{
    int choice;

    if ((strcmp(token, "show") == 0) || (strcmp(token, "layout") == 0))
    {
        return Cli_ExecuteChoice(7, b1Version, b2Version, b3Version);
    }

    if ((strcmp(token, "exit") == 0) || (strcmp(token, "quit") == 0))
    {
        return Cli_ExecuteChoice(0, b1Version, b2Version, b3Version);
    }

    if ((strcmp(token, "write1") == 0) || (strcmp(token, "w1") == 0))
    {
        return Cli_ExecuteChoice(1, b1Version, b2Version, b3Version);
    }
    if ((strcmp(token, "write2") == 0) || (strcmp(token, "w2") == 0))
    {
        return Cli_ExecuteChoice(2, b1Version, b2Version, b3Version);
    }
    if ((strcmp(token, "write3") == 0) || (strcmp(token, "w3") == 0))
    {
        return Cli_ExecuteChoice(3, b1Version, b2Version, b3Version);
    }

    if ((strcmp(token, "read1") == 0) || (strcmp(token, "r1") == 0))
    {
        return Cli_ExecuteChoice(4, b1Version, b2Version, b3Version);
    }
    if ((strcmp(token, "read2") == 0) || (strcmp(token, "r2") == 0))
    {
        return Cli_ExecuteChoice(5, b1Version, b2Version, b3Version);
    }
    if ((strcmp(token, "read3") == 0) || (strcmp(token, "r3") == 0))
    {
        return Cli_ExecuteChoice(6, b1Version, b2Version, b3Version);
    }

    if (sscanf(token, "%d", &choice) == 1)
    {
        return Cli_ExecuteChoice(choice, b1Version, b2Version, b3Version);
    }

    return -1;
}

/* Ho tro cu phap 2 token: write 1 / read 2 */
static int Cli_ExecutePairCommand(const char *first, const char *second,
                                  uint8_t *b1Version, uint8_t *b2Version, uint8_t *b3Version)
{
    int block;

    if ((strcmp(first, "write") != 0) && (strcmp(first, "read") != 0))
    {
        return -1;
    }

    if (sscanf(second, "%d", &block) != 1)
    {
        return -1;
    }

    if (strcmp(first, "write") == 0)
    {
        if ((block >= 1) && (block <= 3))
        {
            return Cli_ExecuteChoice(block, b1Version, b2Version, b3Version);
        }
        return -1;
    }

    if ((block >= 1) && (block <= 3))
    {
        return Cli_ExecuteChoice(block + 3, b1Version, b2Version, b3Version);
    }

    return -1;
}

static void Cli_WriteBlock1(uint8_t *version)
{
    uint8_t data[16];
    uint8_t value = (uint8_t)(0x10u + *version);

    Cli_FillPattern(data, sizeof(data), value);
    if (Fee_Write(1u, data) != E_OK)
    {
        (void)printf("[FAIL] Write Block 1\n");
        return;
    }

    (*version)++;
    (void)printf("[PASS] Write Block 1, pattern = 0x%02X\n", value);
}

static void Cli_WriteBlock2(uint8_t *version)
{
    uint8_t data[32];
    uint8_t value = (uint8_t)(0xA0u + *version);

    Cli_FillPattern(data, sizeof(data), value);
    if (Fee_Write(2u, data) != E_OK)
    {
        (void)printf("[FAIL] Write Block 2\n");
        return;
    }

    (*version)++;
    (void)printf("[PASS] Write Block 2, pattern = 0x%02X\n", value);
}

static void Cli_WriteBlock3(uint8_t *version)
{
    uint8_t data[8];
    uint8_t value = (uint8_t)(0x50u + *version);

    Cli_FillPattern(data, sizeof(data), value);
    if (Fee_Write(3u, data) != E_OK)
    {
        (void)printf("[FAIL] Write Block 3\n");
        return;
    }

    (*version)++;
    (void)printf("[PASS] Write Block 3, pattern = 0x%02X\n", value);
}

static void Cli_ReadBlock1(void)
{
    uint8_t data[16];

    if (Fee_Read(1u, data) != E_OK)
    {
        (void)printf("[FAIL] Read Block 1\n");
        return;
    }

    (void)printf("[PASS] Read Block 1 (16 bytes):\n");
    Cli_PrintBuffer(data, sizeof(data));
}

static void Cli_ReadBlock2(void)
{
    uint8_t data[32];

    if (Fee_Read(2u, data) != E_OK)
    {
        (void)printf("[FAIL] Read Block 2\n");
        return;
    }

    (void)printf("[PASS] Read Block 2 (32 bytes):\n");
    Cli_PrintBuffer(data, sizeof(data));
}

static void Cli_ReadBlock3(void)
{
    uint8_t data[8];

    if (Fee_Read(3u, data) != E_OK)
    {
        (void)printf("[FAIL] Read Block 3\n");
        return;
    }

    (void)printf("[PASS] Read Block 3 (8 bytes):\n");
    Cli_PrintBuffer(data, sizeof(data));
}

int main(void)
{
    char line[32];
    char *token;
    uint8_t b1Version = 1u;
    uint8_t b2Version = 1u;
    uint8_t b3Version = 1u;

    if (Cli_InitWithRecovery() == 0)
    {
        return 1;
    }

    for (;;)
    {
        Cli_ShowMenu();
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            (void)printf("\nInput closed. Exit.\n");
            break;
        }

        token = strtok(line, " \t\r\n");
        if (token == NULL)
        {
            (void)printf("Lua chon khong hop le\n");
            continue;
        }

        while (token != NULL)
        {
            int rc;
            char *nextToken;

            nextToken = strtok(NULL, " \t\r\n");
            if (nextToken != NULL)
            {
                rc = Cli_ExecutePairCommand(token, nextToken, &b1Version, &b2Version, &b3Version);
                if (rc >= 0)
                {
                    if (rc == 1)
                    {
                        return 0;
                    }

                    token = strtok(NULL, " \t\r\n");
                    continue;
                }
            }

            rc = Cli_ExecuteToken(token, &b1Version, &b2Version, &b3Version);
            if (rc < 0)
            {
                (void)printf("Lua chon khong hop le\n");
            }
            else if (rc == 1)
            {
                return 0;
            }

            token = nextToken;
        }
    }

    return 0;
}