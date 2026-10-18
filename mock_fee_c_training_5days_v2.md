# Mini Project C – Persistent Block Storage Inspired by Fee

> **Thời gian thực hiện:** 5 ngày  
> **Ngôn ngữ:** C  
> **Mục tiêu chính:** luyện `FILE`, binary file, `struct`, pointer, module separation, logical/physical mapping, persistent storage và thuật toán đơn giản.  
> **Lưu ý:** Đây **không phải** là implementation AUTOSAR Fee hoàn chỉnh. Đây là bài mô phỏng được đơn giản hóa để học cách tư duy.

---

# 1. Bối cảnh bài toán

Trong chương trình C thông thường, dữ liệu nằm trong RAM.

Ví dụ:

```c
uint32_t counter = 100;
```

Khi chương trình chạy:

```text
counter = 100
```

Nhưng khi chương trình kết thúc:

```text
Program Exit
    ↓
RAM mất dữ liệu
```

Lần chạy tiếp theo:

```text
counter lại được khởi tạo từ đầu
```

Trong hệ thống nhúng thực tế, có nhiều loại dữ liệu cần được giữ lại sau reset hoặc mất nguồn, ví dụ:

- cấu hình thiết bị;
- giá trị calibration;
- lifetime counter;
- diagnostic data;
- user settings.

Trong bài này, chúng ta sẽ dùng một file:

```text
flash.bin
```

để mô phỏng bộ nhớ Flash.

Ý tưởng:

```text
Program
   │
   │ read/write
   ▼
flash.bin
```

Dữ liệu trong `flash.bin` vẫn còn sau khi chương trình kết thúc.

---

# 2. Mục tiêu học tập

Sau bài này, học viên cần hiểu và làm được:

## Kiến thức C

- sử dụng `FILE *`;
- `fopen()`;
- `fclose()`;
- `fseek()`;
- `fread()`;
- `fwrite()`;
- làm việc với binary file;
- sử dụng `struct`;
- truyền buffer bằng pointer;
- chia chương trình thành `.c` và `.h`;
- đọc/ghi dữ liệu theo offset;
- kiểm tra lỗi và return status.

## Tư duy hệ thống

Hiểu sự khác nhau giữa:

```text
Logical data
```

và:

```text
Physical storage
```

Ví dụ:

```text
Block 1
```

không có nghĩa là:

```text
Block 1 luôn nằm ở address 0x100
```

Một Block có thể được ghi nhiều lần ở nhiều vị trí khác nhau.

---

# 3. Concept Overview – Simplified Fee

Phần này cần đọc **trước khi bắt đầu code**. Mục tiêu là hiểu 5 khái niệm:

```text
Block → Slot → Record → Sequence → Sector Swap
```

## 3.1 Block là gì?

Application không làm việc trực tiếp với Flash address. Application chỉ biết các **Logical Block**:

```text
Block 1 = VehicleConfig
Block 2 = Calibration
Block 3 = LifetimeCounter
```

```text
Application
     │
     │ "Hãy lưu Block 1"
     ▼
    Fee
```

Application không cần biết Block 1 đang nằm ở byte nào trong `flash.bin`.

> **Block là danh tính logic của dữ liệu, không phải vị trí vật lý.**

Một Block có thể được ghi nhiều lần và xuất hiện tại nhiều vị trí khác nhau trong Flash nhưng vẫn là cùng một logical block.

---

## 3.2 Slot là gì?

`flash.bin` được chia thành 2 sector. Mỗi sector được chia tiếp thành các **slot** cố định:

```text
flash.bin

┌──────────────────────────────────────────────────┐
│ Sector A                                         │
│                                                  │
│ Slot0 | Slot1 | Slot2 | ... | Slot14 | Slot15  │
└──────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────┐
│ Sector B                                         │
│                                                  │
│ Slot0 | Slot1 | Slot2 | ... | Slot14 | Slot15  │
└──────────────────────────────────────────────────┘
```

Trong bài:

```text
1 Sector = 1024 bytes
1 Slot   =   64 bytes

=> 16 slots / sector
```

> **Slot là vị trí vật lý dùng để chứa một record.**

---

## 3.3 Record là gì?

Mỗi lần gọi `Fee_Write()`, Fee tạo một **record mới** và ghi record đó vào một slot trống.

Một record gồm:

```text
┌────────────────────────────────┐
│ Header                         │
│                                │
│ magic                          │
│ blockId                        │
│ dataLength                     │
│ sequence                       │
├────────────────────────────────┤
│ Data                           │
└────────────────────────────────┘
```

Ví dụ:

```text
Slot 3

┌────────────────────────────────┐
│ magic      = 0xA55A            │
│ blockId    = 1                 │
│ dataLength = 16                │
│ sequence   = 5                 │
├────────────────────────────────┤
│ VehicleConfig data             │
└────────────────────────────────┘
```

Có thể nhớ ngắn gọn:

```text
Block  = "đây là dữ liệu gì?"
Slot   = "record đang nằm ở đâu?"
Record = "một lần lưu cụ thể của Block"
```

---

## 3.4 Tại sao cùng một Block có thể xuất hiện ở nhiều Slot?

Giả sử chương trình thực hiện:

```text
Write Block 1
Write Block 2
Write Block 1
Write Block 3
Write Block 1
```

Fee **không overwrite record cũ**. Mỗi lần write sẽ append một record mới:

```text
Sector A

Slot 0 : Block 1, Seq 1
Slot 1 : Block 2, Seq 2
Slot 2 : Block 1, Seq 3
Slot 3 : Block 3, Seq 4
Slot 4 : Block 1, Seq 5
Slot 5 : FREE
...
```

Block 1 đang xuất hiện ở Slot 0, Slot 2 và Slot 4, nhưng chỉ có **một version hiện tại**.

---

## 3.5 Sequence dùng để làm gì?

Sequence cho biết thứ tự các record được tạo:

```text
Block 1 Seq 1
Block 2 Seq 2
Block 1 Seq 3
Block 3 Seq 4
Block 1 Seq 5
```

Với Block 1:

```text
Seq 1
Seq 3
Seq 5  ← mới nhất
```

Rule của bài:

> **Với cùng một Block ID, record có Sequence lớn nhất là version mới nhất.**

---

## 3.6 Visual: VALID, OLD và FREE

Visualizer sử dụng ký hiệu:

```text
[V] = latest record của Block
[X] = old record
[.] = free slot
```

Ví dụ:

```text
Sector A : ACTIVE

Slot 00 : [X] B01 Seq01
Slot 01 : [V] B02 Seq02
Slot 02 : [X] B01 Seq03
Slot 03 : [V] B03 Seq04
Slot 04 : [V] B01 Seq05
Slot 05 : [.] FREE
Slot 06 : [.] FREE
...
```

`[X]` **không có nghĩa record đã bị xóa khỏi Flash**. Nó chỉ có nghĩa record đó không còn là version mới nhất của block nữa.

Trong Core, trạng thái `[V]` và `[X]` chỉ được **suy ra từ Sequence**, không cần lưu xuống Flash.

---

## 3.7 Logical View và Physical View

Application nhìn dữ liệu theo **Logical View**:

```text
Block 1 -> Seq05
Block 2 -> Seq02
Block 3 -> Seq04
```

Flash thực tế có **Physical View**:

```text
Slot 0 -> B01 Seq01
Slot 1 -> B02 Seq02
Slot 2 -> B01 Seq03
Slot 3 -> B03 Seq04
Slot 4 -> B01 Seq05
```

Visual:

```text
LOGICAL VIEW                    PHYSICAL VIEW

Block 1 -> Seq05        ──────> Slot 4 : B01 Seq05 [LATEST]
Block 2 -> Seq02        ──────> Slot 1 : B02 Seq02 [LATEST]
Block 3 -> Seq04        ──────> Slot 3 : B03 Seq04 [LATEST]

                               Slot 0 : B01 Seq01 [OLD]
                               Slot 2 : B01 Seq03 [OLD]
```

Thông điệp cần nhớ:

> **Logical Block chỉ có một version hiện tại. Physical storage có thể chứa nhiều version của cùng một Block.**

---

## 3.8 `Fee_Read()` thực sự làm gì?

Khi gọi:

```c
Fee_Read(1, data);
```

Fee không biết sẵn Block 1 nằm ở Slot nào. Fee phải scan:

```text
Slot 0 -> B01 Seq1
Slot 1 -> B02 Seq2
Slot 2 -> B01 Seq3
Slot 3 -> B03 Seq4
Slot 4 -> B01 Seq5
```

Sau đó chọn:

```text
B01 Seq5
```

Flow:

```text
Block ID
   │
   ▼
Scan all records
   │
   ▼
Find same Block ID
   │
   ▼
Find maximum Sequence
   │
   ▼
Read Data from that Slot
```

---

## 3.9 Tại sao cần Sector Swap?

Nếu liên tục append record:

```text
Slot 0 used
Slot 1 used
...
Slot 15 used
```

Sector sẽ đầy.

Ví dụ:

```text
Sector A

[X] B01 Seq01
[X] B02 Seq02
[X] B01 Seq03
[X] B03 Seq04
[X] B01 Seq05
...
[V] B03 Seq14
[V] B02 Seq15
[V] B01 Seq16

FULL
```

Nhưng dữ liệu hiện tại thực sự chỉ cần:

```text
B01 Seq16
B02 Seq15
B03 Seq14
```

Các version cũ không còn cần thiết.

---

## 3.10 Sector Swap hoạt động như thế nào?

Khi Sector A đầy:

```text
Sector A = ACTIVE
Sector B = ERASED
```

Fee thực hiện:

```text
1. Tìm latest B01
2. Tìm latest B02
3. Tìm latest B03
4. Copy chúng sang Sector B
5. Erase Sector A
6. Sector B trở thành ACTIVE
7. Tiếp tục ghi record mới
```

Chỉ copy Block đã từng được ghi. Ví dụ chỉ có B01 thì bỏ qua B02 và B03; không tạo record rỗng cho chúng.

### Trước Swap

```text
Sector A : ACTIVE              Sector B : ERASED

[X] B01 Seq01                  [.]
[X] B02 Seq02                  [.]
[X] B01 Seq03                  [.]
...                            [.]
[V] B03 Seq14                  [.]
[V] B02 Seq15                  [.]
[V] B01 Seq16                  [.]
```

### Trong Swap

```text
B01 Seq16  ------------------>  Sector B Slot 0
B02 Seq15  ------------------>  Sector B Slot 1
B03 Seq14  ------------------>  Sector B Slot 2
```

### Sau Swap

```text
Sector A : ERASED              Sector B : ACTIVE

[.]                            [V] B01 Seq16
[.]                            [V] B02 Seq15
[.]                            [V] B03 Seq14
[.]                            [.] FREE
...                            ...
```

Nếu sau đó có `Write B01`, record mới sẽ append tiếp:

```text
Sector B

Slot 0 : B01 Seq16
Slot 1 : B02 Seq15
Slot 2 : B03 Seq14
Slot 3 : B01 Seq17
```

---

## 3.11 Mental Model cần nhớ

```text
Application
     │
     │ Block ID
     ▼
    Fee
     │
     │ chọn Slot / tìm latest Record
     ▼
Physical Flash Layout
     │
     ▼
  Fls_File
     │
     ▼
 flash.bin
```

Application nghĩ:

```text
"Lưu Block 1"
```

Fee nghĩ:

```text
"Ghi một record mới của Block 1 vào slot trống tiếp theo"
```

Khi đọc:

```text
"Quét các slot và tìm Block 1 có Sequence lớn nhất"
```

Khi sector đầy:

```text
"Copy latest records sang sector mới rồi erase sector cũ"
```

---

## 3.12 Tự kiểm tra trước khi code

Hãy tự trả lời 6 câu sau:

1. Block khác Slot như thế nào?
2. Tại sao Block 1 có thể xuất hiện ở nhiều Slot?
3. Record nào của Block 1 là record hiện tại?
4. Sequence dùng để làm gì?
5. Vì sao không overwrite record cũ?
6. Khi Sector đầy, tại sao chỉ cần copy latest record của mỗi Block?

Nếu chưa trả lời được, hãy đọc lại phần Concept Overview trước khi bắt đầu code.

---

# 4. Mini Guide – `FILE`, `fseek()`, `fread()` và `fwrite()`

Trong bài này, hãy coi `flash.bin` như một **mảng byte persistent lớn**:

```text
Address
0x000 [FF]
0x001 [FF]
0x002 [FF]
...
```

Mental model:

```text
fseek()  = đi tới một Flash address
fwrite() = ghi bytes tại address đó
fread()  = đọc bytes tại address đó
```

## 4.1 Mở file

```c
FILE *fp;

fp = fopen("flash.bin", "rb+");

if (fp == NULL)
{
    /* handle error */
}
```

`rb+` cho phép đọc và ghi binary file mà không tự động xóa dữ liệu hiện có.

> Khi file chưa tồn tại, `Fls_Init()` cần tạo file và fill toàn bộ Flash bằng `0xFF` theo yêu cầu của bài.

---

## 4.2 Di chuyển tới một address bằng `fseek()`

Ví dụ:

```c
fseek(fp, 0x20, SEEK_SET);
```

Có thể hình dung:

```text
0x00                              0x20
 │--------------------------------│
                                  ^
                              file cursor
```

`SEEK_SET` nghĩa là offset được tính từ đầu file.

---

## 4.3 Ghi bằng `fwrite()`

```c
uint8_t data[4] =
{
    0xAA,
    0xBB,
    0xCC,
    0xDD
};

fseek(fp, 0x20, SEEK_SET);

fwrite(data, 1, 4, fp);
```

Sau khi ghi:

```text
Address 0x20:

AA BB CC DD
```

Trong:

```c
fwrite(data, 1, 4, fp);
```

ý nghĩa là:

```text
data = buffer nguồn
1    = size mỗi phần tử = 1 byte
4    = số phần tử cần ghi
fp   = file cần ghi
```

---

## 4.4 Đọc bằng `fread()`

```c
uint8_t readBuffer[4];

fseek(fp, 0x20, SEEK_SET);

fread(readBuffer, 1, 4, fp);
```

Sau đó:

```text
readBuffer[0] = 0xAA
readBuffer[1] = 0xBB
readBuffer[2] = 0xCC
readBuffer[3] = 0xDD
```

---

## 4.5 Ví dụ với Fee Slot

Trong project:

```text
FEE_SLOT_SIZE = 64 bytes
```

Địa chỉ Slot:

```c
slotAddress =
    sectorBase
    + slotIndex * FEE_SLOT_SIZE;
```

Ví dụ:

```text
Sector A base = 0
Slot index     = 3
```

thì:

```text
slotAddress = 0 + 3 * 64
            = 192
            = 0xC0
```

```text
Sector A

Slot 0 -> 0x000
Slot 1 -> 0x040
Slot 2 -> 0x080
Slot 3 -> 0x0C0   <--- cần truy cập
```

---

## 4.6 Fee không được gọi FILE API trực tiếp

Đúng:

```text
Fee_Write()
     │
     ▼
Fls_Write()
     │
     ▼
fseek()
fwrite()
```

Sai:

```text
Fee_Write()
     │
     ├── fopen()
     ├── fseek()
     └── fwrite()
```

`Fee` chỉ quan tâm:

```text
address + data + length
```

`Fls_File` chịu trách nhiệm thao tác với `flash.bin`.

---

## 4.7 Luôn kiểm tra lỗi

Ví dụ:

```c
if (address > FLS_TOTAL_SIZE ||
    length > FLS_TOTAL_SIZE - address)
{
    return E_NOT_OK;
}

if (fseek(fp, (long)address, SEEK_SET) != 0)
{
    return E_NOT_OK;
}

if (fwrite(data, 1u, length, fp) != length)
{
    return E_NOT_OK;
}
```

Sau write có thể dùng và kiểm tra kết quả:

```c
if (fflush(fp) != 0)
{
    return E_NOT_OK;
}
```

để đẩy buffer của C runtime sang hệ điều hành. Điều này không bảo đảm dữ liệu tồn tại sau mất điện đột ngột; xử lý mất điện nằm ngoài phạm vi bài.

---

## 4.8 Mental Model cho FILE API

Chỉ cần nhớ:

```text
flash.bin = vùng Flash giả lập

fseek()   = chọn address
fwrite()  = ghi bytes
fread()   = đọc bytes
```

Sau này khi chuyển sang MCU thật, `flash.bin` sẽ được thay bằng Flash hardware, nhưng tư duy:

```text
address + data + length
```

vẫn giữ nguyên.

---

# 5. Kiến trúc project

Project phải được chia thành các layer sau:

```text
+--------------------------------+
|       Application / Test       |
|                                |
| CLI / test scenario            |
+---------------+----------------+
                |
                v
+--------------------------------+
|              Fee               |
|                                |
| Fee_Init()                     |
| Fee_Write()                    |
| Fee_Read()                     |
| Fee_ShowLayout()               |
+---------------+----------------+
                |
                v
+--------------------------------+
|            Fls_File            |
|                                |
| Fls_Init()                     |
| Fls_Read()                     |
| Fls_Write()                    |
| Fls_EraseSector()              |
+---------------+----------------+
                |
                v
            flash.bin
```

## Rule quan trọng

Module `Fee` **không được gọi trực tiếp**:

```c
fopen();
fseek();
fread();
fwrite();
```

Các API file chỉ được sử dụng bên trong:

```text
Fls_File.c
```

Mục tiêu là tách trách nhiệm:

```text
Fee
"Block nào cần lưu?"

Fls_File
"Đọc/ghi byte ở address nào?"
```

---

# 6. Cấu hình Flash giả lập

Sử dụng cấu hình cố định sau:

```c
#define FLS_TOTAL_SIZE          2048u
#define FLS_SECTOR_SIZE         1024u
#define FLS_NUMBER_OF_SECTORS   2u

#define FEE_SLOT_SIZE           64u
#define FEE_SLOTS_PER_SECTOR    16u
```

Flash layout:

```text
flash.bin
size = 2048 bytes

0x000
  |
  | Sector A
  | 1024 bytes
  |
0x3FF
-------------------------
0x400
  |
  | Sector B
  | 1024 bytes
  |
0x7FF
```

Mỗi sector có:

```text
16 slot
```

Mỗi slot:

```text
64 bytes
```

---

# 7. Tại sao dùng Fixed Slot?

Trong project thực tế, record có thể có kích thước khác nhau.

Tuy nhiên để đơn giản hóa bài tập, chúng ta dùng:

```text
1 record = 1 slot = 64 bytes
```

Nhờ vậy việc tính địa chỉ rất đơn giản:

```c
address =
    sectorBaseAddress
    + slotIndex * FEE_SLOT_SIZE;
```

Ví dụ:

```text
Sector A base = 0x000
```

thì:

```text
Slot 0 -> 0x000
Slot 1 -> 0x040
Slot 2 -> 0x080
Slot 3 -> 0x0C0
...
```

vì:

```text
0x40 = 64 bytes
```

---

# 8. Logical Block

Project có 3 Logical Block:

| Block ID | Name | Data Size |
|---|---|---:|
| 1 | VehicleConfig | 16 bytes |
| 2 | Calibration | 32 bytes |
| 3 | LifetimeCounter | 8 bytes |

Configuration gợi ý:

```c
typedef struct
{
    uint16_t blockId;
    uint16_t dataSize;
} Fee_BlockConfigType;
```

Ví dụ:

```c
static const Fee_BlockConfigType Fee_BlockConfig[] =
{
    {1u, 16u},
    {2u, 32u},
    {3u, 8u}
};
```

---

# 9. Record Format

Mỗi lần gọi:

```c
Fee_Write(...)
```

Fee sẽ tạo một record mới.

Một slot 64 bytes được chia thành:

```text
+--------------------------------+
| Header                         |
| 16 bytes                       |
+--------------------------------+
| Data                           |
| tối đa 48 bytes                |
+--------------------------------+
```

Header gợi ý:

```c
typedef struct
{
    uint16_t magic;
    uint16_t blockId;
    uint16_t dataLength;
    uint16_t reserved;
    uint32_t sequence;
    uint32_t reserved2;
} Fee_RecordHeaderType;
```

Giá trị `magic`:

```c
#define FEE_RECORD_MAGIC   0xA55Au
```

Trong environment của lớp, trainer sẽ đảm bảo:

```c
sizeof(Fee_RecordHeaderType) == 16
```

Dùng `stdint.h` để có `uint8_t`, `uint16_t`, `uint32_t` đúng độ rộng. Bài này chỉ yêu cầu đọc lại `flash.bin` trên cùng compiler và kiến trúc đã ghi file; không yêu cầu file tương thích giữa các kiến trúc khác nhau.

Mỗi lần ghi, tạo đủ **64 bytes** cho một slot: header 16 bytes, data đúng kích thước của Block, phần còn lại điền `0xFF`. Đặt `reserved = 0` và `reserved2 = 0`.

Quy ước khi scan:

```text
FREE    = cả 64 bytes đều là 0xFF
VALID   = magic đúng, Block ID có trong cấu hình, dataLength đúng,
          sequence > 0, reserved/reserved2 bằng 0 và padding là 0xFF
INVALID = mọi trường hợp còn lại
```

Không được coi một slot là FREE chỉ vì `magic` khác `FEE_RECORD_MAGIC`.

---

# 10. Ý nghĩa của Sequence Number

Giả sử ghi:

```text
Write Block 1
Write Block 2
Write Block 1
Write Block 3
Write Block 1
```

Flash sẽ có:

```text
Slot 0 : Block 1, Sequence 1
Slot 1 : Block 2, Sequence 2
Slot 2 : Block 1, Sequence 3
Slot 3 : Block 3, Sequence 4
Slot 4 : Block 1, Sequence 5
```

Block 1 có 3 version:

```text
Block 1, Seq 1
Block 1, Seq 3
Block 1, Seq 5
```

Version hiện tại là:

```text
Block 1, Seq 5
```

Rule:

> Với cùng một Block ID, record có Sequence lớn nhất là record mới nhất.

---

# 11. Không Overwrite Record Cũ

Đây là requirement quan trọng nhất.

Ví dụ:

```text
Fee_Write(Block1, A)
Fee_Write(Block1, B)
```

Không được làm:

```text
Slot 0 = A
sau đó ghi đè Slot 0 = B
```

Phải làm:

```text
Slot 0 = Block1 / A / Seq 1
Slot 1 = Block1 / B / Seq 2
```

Khi đọc Block 1:

```text
Fee_Read(Block1)
```

Fee trả về:

```text
B
```

vì:

```text
Seq 2 > Seq 1
```

---

# 12. Trạng thái OLD không cần lưu xuống Flash

Trong core project, không cần có field:

```text
VALID / OLD
```

trong record.

Ta chỉ suy ra khi hiển thị:

```text
Block1 Seq1 -> OLD
Block1 Seq2 -> VALID
```

Rule:

```text
Sequence lớn nhất của mỗi Block ID -> VALID
Các sequence cũ hơn             -> OLD
```

---

# 13. Module Fls_File

---

## 13.1 API bắt buộc

```c
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
```

Có thể bổ sung:

```c
void Fls_Dump(
    uint32_t address,
    uint32_t length);
```

để debug.

---

# 14. Fls_Init()

Khi chương trình chạy:

```text
flash.bin tồn tại?
```

## Nếu chưa tồn tại

Tạo file:

```text
flash.bin
```

với size:

```text
2048 bytes
```

và fill toàn bộ bằng:

```text
0xFF
```

Ví dụ:

```text
FF FF FF FF FF FF FF FF
FF FF FF FF FF FF FF FF
...
```

## Nếu file đã tồn tại

Chỉ mở lại file.

Kiểm tra kích thước file. Nếu khác `2048 bytes`, trả `E_NOT_OK`; không tự sửa hoặc truncate file.

Không được:

```text
reset toàn bộ thành FF
```

vì như vậy dữ liệu persistent sẽ bị mất.

---

# 15. Fls_Read()

Ví dụ:

```c
uint8_t buffer[16];

Fls_Read(
    0x100,
    buffer,
    16
);
```

Ý nghĩa:

```text
Đọc 16 bytes
bắt đầu tại address 0x100
```

Pseudo flow:

```text
Check address
    ↓
fseek()
    ↓
fread()
    ↓
return status
```

Phải kiểm tra giới hạn địa chỉ mà không làm tràn số nguyên:

```text
address <= FLS_TOTAL_SIZE
length <= FLS_TOTAL_SIZE - address
```

Nếu `length == 0` hoặc `data == NULL`, trả `E_NOT_OK`.

---

# 16. Fls_Write()

Ví dụ:

```c
uint8_t data[4] =
{
    0x11,
    0x22,
    0x33,
    0x44
};

Fls_Write(
    0x20,
    data,
    4
);
```

Sau đó Flash:

```text
Address 0x20:

11 22 33 44
```

Core project **chưa bắt buộc** kiểm tra rule Flash `1 -> 0`.

Rule này được đưa vào Bonus.

Giống `Fls_Read`, trả `E_NOT_OK` nếu `length == 0`, `data == NULL` hoặc phạm vi ghi vượt file. Kiểm tra kết quả của `fseek()`, `fwrite()` và `fflush()`.

---

# 17. Fls_EraseSector()

Ví dụ:

```c
Fls_EraseSector(0);
```

thì toàn bộ Sector A:

```text
0x000 - 0x3FF
```

phải trở thành:

```text
FF FF FF FF FF ...
```

Sector B:

```text
0x400 - 0x7FF
```

không được thay đổi.

---

# 18. DAY 1 – Fls_File

Mục tiêu cuối ngày:

```text
flash.bin
+
Read
+
Write
+
Erase
+
Persistence
```

---

## Test 1 – Create Flash

Xóa `flash.bin`.

Run chương trình.

Expected:

```text
flash.bin được tạo
size = 2048 bytes
```

---

## Test 2 – Write

Ghi:

```text
Address 0x20

AA BB CC DD
```

Sau đó đọc lại.

Expected:

```text
AA BB CC DD
```

---

## Test 3 – Persistence

Run #1:

```text
Write:

0x20 = AA BB CC DD
```

Exit chương trình.

Run #2:

```text
Read 0x20
```

Expected:

```text
AA BB CC DD
```

Không được trở thành:

```text
FF FF FF FF
```

---

## Test 4 – Erase

```text
Fls_EraseSector(0)
```

Expected:

```text
Sector A = FF
Sector B không đổi
```

---

# 19. DAY 1 Gate

Chỉ chuyển sang Fee khi tất cả pass:

```text
[ ] Fls_Init
[ ] Fls_Read
[ ] Fls_Write
[ ] Fls_EraseSector
[ ] Persistence
```

**Chuẩn bị cho Day 2:** Test Fls đã ghi dữ liệu thô tại `0x20`, nằm trong Fee Slot 0. Hãy dùng file khác cho test Fee hoặc xóa `flash.bin` của test Fls **một lần** để `Fls_Init()` tạo file sạch. Không xóa file giữa hai lần chạy của test persistence.

---

# 20. Module Fee

API bắt buộc:

```c
Std_ReturnType Fee_Init(void);

Std_ReturnType Fee_Write(
    uint16_t blockId,
    const uint8_t *data);

Std_ReturnType Fee_Read(
    uint16_t blockId,
    uint8_t *data);

void Fee_ShowLayout(void);
```

Phải gọi `Fee_Init()` trước các API Fee khác. `Fee_Init()` gọi `Fls_Init()` rồi quét file; `Fee_Write()` và `Fee_Read()` trả `E_NOT_OK` nếu chưa khởi tạo, Block ID không thuộc cấu hình hoặc `data == NULL`.

Người gọi phải cấp buffer đủ `dataSize` của Block. `Fee_Write()` đọc đúng số byte đó; `Fee_Read()` ghi đúng số byte đó. Hai API không nhận kích thước buffer nên không thể tự kiểm tra vùng nhớ của người gọi. Nếu Block chưa từng được ghi, `Fee_Read()` trả `E_NOT_OK`.

---

# 21. Fee Internal State

Core project có thể giữ:

```c
static uint8_t Fee_ActiveSector;
static uint8_t Fee_NextFreeSlot;
static uint32_t Fee_NextSequence;
```

Ví dụ khi mới format:

```text
Fee_ActiveSector = 0
Fee_NextFreeSlot = 0
Fee_NextSequence = 1
```

Chỉ cập nhật `Fee_NextFreeSlot` và sequence sau khi ghi record thành công. Nếu sequence đã đạt `UINT32_MAX`, không ghi thêm để tránh tràn số; `Fee_Read()` vẫn hoạt động.

---

# 22. Tính Physical Address của Slot

Ví dụ function:

```c
static uint32_t Fee_GetSlotAddress(
    uint8_t sectorId,
    uint8_t slotIndex)
{
    uint32_t sectorBase;

    sectorBase =
        sectorId * FLS_SECTOR_SIZE;

    return sectorBase
         + slotIndex * FEE_SLOT_SIZE;
}
```

Ví dụ:

```text
sectorId = 0
slotIndex = 3
```

thì:

```text
address = 0 + 3 * 64
        = 192
        = 0xC0
```

---

# 23. DAY 2 – Fee_Write

Trước khi gọi `Fee_Write()`, làm phiên bản đầu của `Fee_Init()` để nhận biết file mới có cả hai sector trống và chọn Sector A. Day 3 sẽ hoàn thiện `Fee_Init()` để khôi phục file đã có record.

Mục tiêu:

```text
Write logical block
       ↓
Append physical record
```

---

## 23.1 Kiểm tra Block ID

Ví dụ:

```c
Fee_Write(1, data);
```

phải tìm Block 1 trong:

```text
Fee_BlockConfig[]
```

Nếu gọi:

```c
Fee_Write(99, data);
```

thì:

```text
E_NOT_OK
```

---

## 23.2 Tạo Header

Ví dụ lần ghi đầu tiên:

```text
Block ID = 1
Data Length = 16
Sequence = 1
```

Header:

```text
magic      = 0xA55A
blockId    = 1
dataLength = 16
sequence   = 1
```

---

## 23.3 Ghi vào Next Free Slot

Ban đầu:

```text
Next Free Slot = 0
```

Ghi Block 1:

```text
Slot 0 = B01 Seq1
```

Sau đó:

```text
Next Free Slot = 1
Next Sequence  = 2
```

---

## 23.4 Ví dụ đầy đủ

Call:

```text
Fee_Write(B01, A)
Fee_Write(B02, X)
Fee_Write(B01, B)
```

Expected:

```text
Sector A

Slot 00 : B01 Seq001
Slot 01 : B02 Seq002
Slot 02 : B01 Seq003
Slot 03 : FREE
Slot 04 : FREE
...
```

---

# 24. Fee_ShowLayout()

Học viên phải implement function hiển thị trạng thái.

Ví dụ:

```text
================================================
              FEE FLASH LAYOUT
================================================

Sector A : ACTIVE

Slot 00 : [X] B01 Seq=001
Slot 01 : [V] B02 Seq=002
Slot 02 : [V] B01 Seq=003
Slot 03 : [.] FREE
Slot 04 : [.] FREE
...

Sector B : ERASED

Slot 00 : [.]
Slot 01 : [.]
...
```

Ký hiệu:

```text
[V] latest record của Block
[X] old record
[.] free slot
```

---

# 25. Làm sao xác định [V] và [X]?

Giả sử:

```text
Slot 00 B01 Seq1
Slot 01 B02 Seq2
Slot 02 B01 Seq3
```

Latest sequence:

```text
B01 -> 3
B02 -> 2
```

Do đó:

```text
Slot 00 B01 Seq1 -> [X]
Slot 01 B02 Seq2 -> [V]
Slot 02 B01 Seq3 -> [V]
```

---

# 26. DAY 2 Gate

Expected:

```text
Write B1
Write B2
Write B1
```

sau đó:

```text
show
```

phải nhìn thấy:

```text
[X] B01 Seq1
[V] B02 Seq2
[V] B01 Seq3
```

---

# 27. DAY 3 – Fee_Read

Mục tiêu:

```text
Logical Block ID
      ↓
scan physical records
      ↓
find latest sequence
      ↓
return data
```

---

# 28. Fee_Read Algorithm

Ví dụ muốn đọc:

```text
Block 1
```

Sector có:

```text
Slot 0 B01 Seq1
Slot 1 B02 Seq2
Slot 2 B01 Seq3
Slot 3 B03 Seq4
Slot 4 B01 Seq5
```

Algorithm:

```text
latestSequence = 0
latestSlot     = INVALID

for each slot
    |
    +-- blockId != 1
    |       skip
    |
    +-- blockId == 1
            |
            +-- sequence > latestSequence
                    |
                    +-- update latest
```

Kết quả:

```text
latestSlot = Slot 4
```

Sau đó đọc Data của Slot 4.

---

# 29. Ví dụ C pseudo code

Mã giả dưới đây giả định `Fee_Init()` đã kiểm tra layout thành công và đang quét active sector. Khi code thật, vẫn phải kiểm tra status của `Fee_ReadHeader()`/`Fls_Read()` trước khi dùng `header`.

```c
uint32_t latestSequence = 0u;
uint8_t latestSlot = INVALID_SLOT;

for (slot = 0u;
     slot < FEE_SLOTS_PER_SECTOR;
     slot++)
{
    Fee_ReadHeader(slot, &header);

    if (header.magic != FEE_RECORD_MAGIC)
    {
        continue;
    }

    if (header.blockId == blockId)
    {
        if (header.sequence > latestSequence)
        {
            latestSequence = header.sequence;
            latestSlot = slot;
        }
    }
}
```

Sau loop:

```text
latestSlot == INVALID_SLOT
```

thì Block chưa từng được ghi.

Return:

```text
E_NOT_OK
```

---

# 30. DAY 3 – Fee_Init

Khi chương trình restart:

```text
RAM variables mất
```

Ví dụ mất:

```text
Fee_NextFreeSlot
Fee_NextSequence
Fee_ActiveSector
```

Nhưng:

```text
flash.bin vẫn còn
```

Fee_Init phải scan lại.

---

# 31. Fee_Init Core Requirement

Core project chỉ cần support Flash layout hợp lệ. `Fee_Init()` gọi `Fls_Init()` rồi quét **cả hai sector**. Quy tắc chọn active sector:

| Sector A | Sector B | Kết quả |
|---|---|---|
| Toàn `FF` | Toàn `FF` | Chọn A; next slot 0; next sequence 1 |
| Có record hợp lệ | Toàn `FF` | Chọn A |
| Toàn `FF` | Có record hợp lệ | Chọn B |
| Có dữ liệu | Có dữ liệu | `E_NOT_OK` |

Trong sector có dữ liệu, các slot VALID phải nằm liên tiếp từ slot 0, sau đó là các slot FREE. Không được có slot INVALID hoặc hai record trùng sequence. Không bắt buộc sequence tăng theo thứ tự slot, vì swap giữ nguyên sequence khi copy các Block.

Không cần khôi phục dữ liệu từ:

- partial record;
- coecord;
- power loss;rrupted r
- interrupted swap.

Nếu layout không hợp lệ (file sai kích thước, slot INVALID, hai sector cùng có dữ liệu, USED xuất hiện sau FREE, hoặc sequence trùng):

```text
Fee_Init -> E_NOT_OK
```

---

# 32. Reconstruct Next Free Slot

Ví dụ:

```text
Slot 0 B01 Seq1
Slot 1 B02 Seq2
Slot 2 B01 Seq3
Slot 3 FREE
Slot 4 FREE
...
```

sau `Fee_Init()`:

```text
Fee_NextFreeSlot = 3
```

---

# 33. Reconstruct Next Sequence

Trong ví dụ trên:

```text
max sequence = 3
```

do đó:

```text
Fee_NextSequence = 4
```

Nếu sequence lớn nhất là `UINT32_MAX`, `Fee_Init()` vẫn thành công để cho phép đọc. Lệnh ghi tiếp theo trả `E_NOT_OK` **trước khi swap**; không cộng tràn sequence. Sau khi ghi thành công record có sequence `UINT32_MAX`, cũng phải đánh dấu đã hết sequence.

---

# 34. Persistence Test

## RUN #1

```text
Write B01 = A
Write B02 = X
Write B01 = B
```

Layout:

```text
Slot0 B01 Seq1
Slot1 B02 Seq2
Slot2 B01 Seq3
```

Exit.

---

## RUN #2

Call:

```text
Fee_Init()
```

Expected:

```text
NextFreeSlot = 3
NextSequence = 4
```

Call:

```text
Fee_Read(B01)
```

Expected:

```text
B
```

Sau đó:

```text
Fee_Write(B03)
```

Expected:

```text
Slot3 B03 Seq4
```

Không được:

```text
Seq1
```

---

# 35. DAY 3 Gate

Phải pass scenario:

```text
Run 1
    ↓
Write B1
Write B2
Write B1
    ↓
Exit
    ↓
Run 2
    ↓
Fee_Init
    ↓
Read B1 đúng
    ↓
Write B3
    ↓
Sequence tiếp tục
```

Nếu chưa pass, chưa làm Sector Swap.

---

# 36. CORE COMPLETED

Nếu pass tới Day 3, học viên đã hoàn thành Core.

Core bao gồm:

```text
FILE
 ↓
Mock Flash
 ↓
Fixed Slot
 ↓
Logical Block
 ↓
Append Record
 ↓
Sequence
 ↓
Read Latest
 ↓
Restart Reconstruction
 ↓
Visualizer
```

---

# 37. DAY 4 – Simple Sector Swap

Đây là phần Target.

Không phải tất cả học viên yếu đều bắt buộc phải hoàn thành hoàn hảo.

---

# 38. Vì sao cần Swap?

Sector A có:

```text
16 slots
```

Sau nhiều lần write:

```text
Slot 00 used
Slot 01 used
...
Slot 15 used
```

không còn chỗ.

Nhưng có nhiều record cũ.

Ví dụ:

```text
B01 Seq1
B01 Seq3
B01 Seq7
B01 Seq10
```

chỉ cần giữ:

```text
B01 Seq10
```

Các version cũ có thể bỏ.

---

# 39. Ví dụ trước Swap

```text
Sector A : ACTIVE

[X] B01 Seq01
[X] B02 Seq02
[X] B01 Seq03
[X] B03 Seq04
[X] B01 Seq05
[X] B02 Seq06
...
[V] B03 Seq14
[V] B02 Seq15
[V] B01 Seq16

FULL
```

Sector B:

```text
ERASED
```

Latest data chỉ có:

```text
B01 Seq16
B02 Seq15
B03 Seq14
```

---

# 40. Swap Algorithm

Khi:

```text
Fee_Write(...)
```

phát hiện:

```text
Fee_NextFreeSlot >= 16
```

thì:

```text
Fee_Swap()
```

Flow:

```text
1. Tìm latest record của từng Block đã từng được ghi.
   Block chưa từng ghi thì bỏ qua.

2. Kiểm tra Spare Sector đang toàn 0xFF.

3. Copy nguyên 64 bytes của mỗi latest record sang các slot đầu của Spare.
   Giữ nguyên sequence của record được copy.

4. Erase Old Active Sector.

5. Sau khi erase thành công, Spare trở thành Active.

6. NextFreeSlot = số record đã copy.

7. Thực hiện request Fee_Write ban đầu bằng NextSequence hiện tại.
   Chỉ request mới này làm sequence tăng.
```

Ví dụ nếu 16 lần ghi trước đó đều là B01, chỉ copy **một** latest record B01 vào Spare Slot 0; request mới sẽ vào Slot 1. Nếu đã từng ghi cả B01, B02 và B03 thì copy ba record như hình bên dưới.

---

# 41. Hình dung Swap

Trước:

```text
Sector A                  Sector B
ACTIVE                    ERASED

[X] B01 Seq01             [.]
[X] B02 Seq02             [.]
[X] B01 Seq03             [.]
...
[V] B03 Seq14             [.]
[V] B02 Seq15             [.]
[V] B01 Seq16             [.]
```

Copy:

```text
B01 Seq16 ───────────────> Slot 0
B02 Seq15 ───────────────> Slot 1
B03 Seq14 ───────────────> Slot 2
```

Sau:

```text
Sector A                  Sector B
ERASED                    ACTIVE

[.]                       [V] B01 Seq16
[.]                       [V] B02 Seq15
[.]                       [V] B03 Seq14
[.]                       [.] FREE
...
```

Sau đó new write:

```text
B01 Seq17
```

thì:

```text
Slot 3 -> B01 Seq17
```

---

# 42. Assumption rất quan trọng

Trong project này:

> **Giả định chương trình KHÔNG bị tắt trong quá trình Sector Swap.**

Không cần xử lý:

```text
Power loss
```

trong khi:

```text
copy
erase
switch sector
```

đang diễn ra.

---

# 43. Không cần Crash Recovery

Nếu chương trình bị kill trong swap:

```text
behavior undefined
```

Không phải lỗi của bài.

Đây là yêu cầu cố tình đơn giản hóa.

---

# 44. Active Sector

Project này **không lưu metadata riêng**: `16 slots × 64 bytes = 1024 bytes`, vừa đúng một sector. Sau swap hoàn tất, old sector được erase và chỉ một sector chứa record. Vì vậy `Fee_Init()` xác định active sector theo bảng ở mục 31.

Khi cả hai sector đều trống, chọn Sector A. Khi đúng một sector có record hợp lệ, chọn sector đó. Khi cả hai có dữ liệu, trả `E_NOT_OK`; trạng thái này có thể xảy ra nếu swap bị ngắt, vốn nằm ngoài phạm vi bài.

Không tự ý ghi metadata vào một sector mà vẫn giữ cấu hình 16 slot/sector, vì không còn chỗ cho metadata.

---

# 45. DAY 4 Gate

Test phải trigger ít nhất một swap.

Ví dụ:

```text
for i = 0 -> 19
{
    Write alternating B1/B2/B3
}
```

Expected:

```text
Sector A full
       ↓
Swap
       ↓
Sector B active
       ↓
latest B1/B2/B3 vẫn đọc đúng
```

Thêm test biên: ghi B1 đúng 16 lần, rồi ghi lần thứ 17. Sau swap chỉ cần copy một latest record B1 sang Spare Slot 0; lần ghi thứ 17 vào Slot 1. Thoát chương trình và chạy lại để kiểm tra `Fee_Init()` chọn đúng active sector.

---

# 46. DAY 5 – Integration & Test

Day 5 **không thêm feature mới**.

Mục tiêu:

```text
Test
Debug
Fix
README
Demo
```

---

# 47. Multi-Swap Test

Không chỉ test một lần.

Ghi:

```text
30 - 50 lần
```

để trigger nhiều swap.

Ví dụ:

```text
B1
B2
B1
B3
B1
B2
B3
...
```

Sau mỗi swap:

```text
Fee_Read(B1)
Fee_Read(B2)
Fee_Read(B3)
```

phải đúng.

---

# 48. Restart After Swap

Scenario:

```text
Write nhiều lần
    ↓
Swap
    ↓
Exit
    ↓
Run lại
    ↓
Fee_Init
    ↓
Read B1/B2/B3
```

Expected:

```text
đúng latest data
```

---

# 49. CLI Gợi Ý

Có thể cung cấp menu:

```text
================================
Mock Fee CLI
================================

1. Write Block 1
2. Write Block 2
3. Write Block 3

4. Read Block 1
5. Read Block 2
6. Read Block 3

7. Show Layout

0. Exit
```

Hoặc command:

```text
write 1
read 1
show
exit
```

Không bắt buộc format cụ thể.

Các lệnh `Erase Sector A/B` chỉ nên có trong menu debug riêng. Không dùng chúng trong demo persistence vì xóa active sector sẽ xóa dữ liệu đang được kiểm thử.

---

# 50. Data Test Gợi Ý

Để dễ test, không nhất thiết nhập 16–32 byte bằng tay.

Có thể tạo data pattern.

Ví dụ Block 1:

```c
uint8_t block1Data[16];
```

Version A:

```text
11 11 11 11 11 11 11 11
11 11 11 11 11 11 11 11
```

Version B:

```text
22 22 22 22 22 22 22 22
22 22 22 22 22 22 22 22
```

Version C:

```text
33 33 33 33 33 33 33 33
33 33 33 33 33 33 33 33
```

Nhìn output rất dễ nhận biết.

---

# 51. Test Scenario Mẫu

Thực hiện:

```text
Write B1 = 0x11 pattern
Write B2 = 0xAA pattern
Write B1 = 0x22 pattern
Write B3 = 0x55 pattern
Write B1 = 0x33 pattern
```

Expected layout:

```text
[X] B01 Seq01
[V] B02 Seq02
[X] B01 Seq03
[V] B03 Seq04
[V] B01 Seq05
```

Read:

```text
B01 -> 0x33 pattern
B02 -> 0xAA pattern
B03 -> 0x55 pattern
```

---

# 52. Common Mistake 1 – Reset File mỗi lần chạy

Sai:

```c
fopen("flash.bin", "wb");
```

mỗi lần startup.

`wb` có thể truncate file.

Hậu quả:

```text
Restart
 ↓
data mất
```

Hãy cẩn thận với mode mở file.

---

# 53. Common Mistake 2 – Fee dùng FILE trực tiếp

Sai:

```c
Fee_Write()
{
    fopen(...);
    fwrite(...);
}
```

Đúng:

```text
Fee
 ↓
Fls_Write()
 ↓
FILE API
```

---

# 54. Common Mistake 3 – Hard-code address theo Block ID

Sai:

```c
if (blockId == 1)
{
    address = 0x100;
}
```

Bởi vì Block 1 phải có thể xuất hiện ở nhiều slot.

---

# 55. Common Mistake 4 – Overwrite record cũ

Sai:

```text
B1 luôn ghi lại Slot 0
```

Đúng:

```text
mỗi lần write
 ↓
append record mới
```

---

# 56. Common Mistake 5 – Sequence reset sau restart

Sai:

```text
Run 1:
Seq 1
Seq 2
Seq 3

Restart

Run 2:
Seq 1
```

Đúng:

```text
Fee_Init()
 ↓
find max sequence = 3
 ↓
next = 4
```

---

# 57. Common Mistake 6 – Fee_Read lấy record đầu tiên

Sai:

```text
scan thấy B1 đầu tiên
 ↓
return ngay
```

Phải tìm:

```text
B1 có sequence lớn nhất
```

---

# 58. Common Mistake 7 – Chỉ test Swap một lần

Implementation có thể:

```text
Swap #1 PASS
Swap #2 FAIL
```

Phải test:

```text
multiple swaps
```

---

# 59. CORE Requirement

Phải hoàn thành:

```text
[ ] Fls_Init
[ ] Fls_Read
[ ] Fls_Write
[ ] Fls_EraseSector

[ ] Fixed Slot
[ ] Fee_Write append
[ ] Sequence number
[ ] Fee_Read latest

[ ] Fee_Init
[ ] Restart persistence
[ ] Fee_ShowLayout
```

---

# 60. TARGET Requirement

Mục tiêu chính nếu Core hoàn thành sớm:

```text
[ ] Two-sector Swap
[ ] Copy only latest records
[ ] Erase old sector
[ ] Continue writing after swap
[ ] Restart after swap
[ ] Multiple swap test
```

---

# 61. BONUS

Chỉ làm Bonus khi Core + Swap ổn định.

## Bonus A – Flash 1 -> 0 Rule

Flash thật thường không cho phép tùy ý chuyển:

```text
0 -> 1
```

nếu chưa erase.

Ví dụ:

```text
Current:

1010 0101

New:

1110 0101
```

có bit:

```text
0 -> 1
```

=> reject.

Có thể implement trong:

```c
Fls_Write()
```

---

# 62. BONUS B – CRC

Trainer có thể cung cấp function:

```c
uint16_t Crc16_Calculate(
    const uint8_t *data,
    uint32_t length);
```

Học viên lưu CRC cùng record.

Khi read:

```text
read data
 ↓
calculate CRC
 ↓
compare
```

Nếu mismatch:

```text
CRC ERROR
```

Không yêu cầu recover version cũ.

---

# 63. OUT OF SCOPE

Không implement:

```text
Power-loss recovery

Crash-safe swap

Partial write recovery

Async Fee_MainFunction

Job queue

Variable record size

Wear leveling production-like

Multiple partitions

NvM

MemIf

Dataset block

Redundant NvM block
```

Nếu tự ý thêm các feature này làm Core chưa ổn định:

```text
không được tính là ưu điểm
```

Ưu tiên:

```text
simple
correct
understandable
testable
```

---

# 64. Project Structure Gợi Ý

```text
project/
│
├── main.c
│
├── Std_Types.h
│
├── Fls_File.c
├── Fls_File.h
│
├── Fee.c
├── Fee.h
│
├── Fee_Cfg.c
├── Fee_Cfg.h
│
├── Test.c
├── Test.h
│
└── flash.bin
```

---

# 65. Std_Types Gợi Ý

```c
#include <stdint.h>

typedef enum
{
    E_OK = 0,
    E_NOT_OK
} Std_ReturnType;
```

`stdint.h` là một phần của C99 và được dùng trong bài này để giữ đúng độ rộng của header.

---

# 66. Suggested Implementation Order

Không code Sector Swap ngay từ đầu.

Làm đúng thứ tự:

```text
1. Fls_Init
2. Fls_Read
3. Fls_Write
4. Fls_EraseSector

5. Fee_GetSlotAddress
6. Fee_Write one record
7. Fee_Write append

8. Fee_ShowLayout

9. Fee_Read latest

10. Fee_Init recovery

11. Persistence test

====================
CORE DONE
====================

12. Fee_Swap
13. Multi-swap test

====================
TARGET DONE
====================

14. Bonus
```

---

# 67. Khi bị lỗi, debug theo layer

Ví dụ:

```text
Fee_Read trả sai data
```

Không sửa lung tung.

Kiểm tra:

```text
Step 1:
flash.bin có đúng bytes không?

Step 2:
Fls_Read có đọc đúng không?

Step 3:
Header đọc đúng không?

Step 4:
Block ID đúng không?

Step 5:
Sequence đúng không?

Step 6:
Fee_Read chọn đúng latest slot không?
```

---

# 68. Một ví dụ Debug

Expected:

```text
Slot 0 B1 Seq1
Slot 1 B2 Seq2
Slot 2 B1 Seq3
```

Nhưng `Fee_Read(B1)` trả Seq1.

Hãy in:

```text
Scan Slot 0
Block=1
Seq=1
latest=1

Scan Slot 1
Block=2
skip

Scan Slot 2
Block=1
Seq=3
latest=?
```

Nếu latest vẫn = 1:

```text
bug nằm ở comparison/update logic
```

---

# 69. Daily Plan

## Day 1

Mục tiêu:

```text
Mock Flash hoạt động
```

Checklist:

```text
[ ] create/open flash.bin
[ ] read
[ ] write
[ ] erase
[ ] persistence
```

---

## Day 2

Mục tiêu:

```text
Fee_Write + visual
```

Checklist:

```text
[ ] fixed slot
[ ] header
[ ] sequence
[ ] append
[ ] show layout
```

---

## Day 3

Mục tiêu:

```text
Fee_Read + Restart
```

Checklist:

```text
[ ] find latest
[ ] Fee_Read
[ ] Fee_Init
[ ] next free slot recovery
[ ] next sequence recovery
[ ] restart test
```

Đến đây Core phải hoàn thành.

---

## Day 4

Mục tiêu:

```text
Sector Swap
```

Checklist:

```text
[ ] detect full sector
[ ] find latest B1/B2/B3
[ ] copy to spare
[ ] erase old
[ ] switch active
[ ] continue write
```

---

## Day 5

Mục tiêu:

```text
Stabilize
```

Checklist:

```text
[ ] multi-swap
[ ] restart after swap
[ ] fix bugs
[ ] clean code
[ ] README
[ ] demo
```

Không thêm feature lớn mới.

---

# 70. Review Questions

Học viên phải trả lời được:

1. Tại sao dùng `flash.bin` thay vì biến RAM?
2. Tại sao `Fee` không được gọi `fwrite()` trực tiếp?
3. Logical Block khác Physical Address như thế nào?
4. Tại sao mỗi lần ghi Block 1 lại tạo record mới?
5. Sequence dùng để làm gì?
6. Nếu B1 có Seq1, Seq3, Seq7 thì record nào hiện tại?
7. `Fee_Read()` tìm latest record như thế nào?
8. Vì sao `Fee_Init()` phải scan Flash sau restart?
9. `Fee_NextSequence` được reconstruct như thế nào?
10. Khi sector đầy, tại sao không copy tất cả record?
11. Sau swap, tại sao chỉ cần giữ latest version?
12. Tại sao project này chưa xử lý power loss trong swap?

---

# 71. Final Demo Scenario

Demo cuối nên chạy:

```text
START
 ↓
Fee_Init
 ↓
Write B1
 ↓
Write B2
 ↓
Write B1
 ↓
Write B3
 ↓
Write nhiều lần
 ↓
Sector Full
 ↓
Fee Swap
 ↓
Continue Write
 ↓
Show Layout
 ↓
Read B1/B2/B3
 ↓
Exit
 ↓
Restart
 ↓
Fee_Init
 ↓
Read B1/B2/B3
 ↓
PASS
```

---

# 72. Mental Model Cuối Bài

Điều quan trọng nhất cần nhớ:

```text
Application
     |
     | Block ID
     v
    Fee
     |
     | Physical Slot / Address
     v
 Fls_File
     |
     v
 flash.bin
```

Application nghĩ:

```text
"Lưu Block 1"
```

Fee nghĩ:

```text
"Record mới của Block 1 nên nằm ở slot nào?"
```

Fls nghĩ:

```text
"Ghi bytes vào address nào?"
```

---

# 73. Điều trainer quan tâm nhất

Không phải:

```text
code nhiều
```

Không phải:

```text
feature nhiều
```

Mà là:

```text
Học viên có hiểu:
- dữ liệu logical
- dữ liệu physical
- append record
- latest version
- persistence
- sector swap
hay không?
```

Một implementation đơn giản, sạch và giải thích được tốt hơn một implementation phức tạp nhưng không hiểu.

---

# 74. Definition of Done

## Core Done

```text
Fls works
+
Fee append works
+
Fee_Read latest works
+
Restart persistence works
+
Layout visual works
```

## Target Done

```text
Core
+
Sector Swap
+
Multiple Swap
+
Restart after Swap
```

## Bonus Done

```text
Target
+
Flash rule hoặc CRC
```

---

**End of Assignment**
