# Mock FEE C Training

## 1. Muc tieu
Du an mo phong mot phan nho cua AUTOSAR FEE tren file nhi phan `flash.bin`.
Kien truc theo layer:

- Test/App
- Fee (logic record append, read latest, swap sector)
- Fls_File (flash file driver)

## 2. Cau truc thu muc

- `inc/`
  - `Std_Types.h`: `Std_ReturnType` (`E_OK`, `E_NOT_OK`)
  - `Fls_File.h`: API low-level flash file
  - `Fee_Cfg.h`: cau hinh block/slot
  - `Fee.h`: API Fee
  - `Test_Day1.h` ... `Test_Day4.h`: test gate headers
- `src/`
  - `Fls_File.c`: thao tac doc/ghi/erase tren `flash.bin`
  - `Fee_Cfg.c`: bang cau hinh block
  - `Fee.c`: core Fee
  - `Test_Day1.c` ... `Test_Day4.c`: test theo ngay
  - `main.c`: chuong trinh CLI visual de demo write/read/show layout
- `Makefile`: build voi gcc

## 3. Thong so he thong

- Tong dung luong flash: `2048` bytes
- So sector: `2`
- Moi sector: `1024` bytes
- Slot size: `64` bytes
- Slot/sector: `16`
- Header size: `16` bytes
- Payload toi da moi slot: `48` bytes

## 4. Cau hinh block logic

Trong `src/Fee_Cfg.c`:

- Block `1`: `16` bytes
- Block `2`: `32` bytes
- Block `3`: `8` bytes

## 5. Dinh dang record
Moi slot co layout:

- Header (`16` bytes):
  - `magic` (`0xA55A`)
  - `blockId`
  - `dataLength`
  - `reserved`
  - `sequence`
  - `reserved2`
- Payload (`dataLength` bytes)
- Phan con lai fill `0xFF`

Record hop le can:

- `magic` dung
- `blockId` ton tai trong config
- `dataLength` dung theo block config
- `sequence > 0`
- `reserved == 0` va `reserved2 == 0`
- Tail bytes sau payload la `0xFF`

## 6. Logic Fee chinh

### 6.1 Fee_Init
- Goi `Fls_Init`.
- Quet ca 2 sector de reconstruct:
  - sector nao active
  - slot tiep theo (`nextFreeSlot`)
  - sequence tiep theo (`nextSequence`)
- Rule:
  - Ca 2 sector rong: active = sector 0, sequence = 1.
  - Ca 2 sector deu co data: fail (state khong hop le).

### 6.2 Fee_Write
- Validate input.
- Neu sector active full thi swap truoc.
- Tao record moi (append-only) voi `sequence` tang dan.
- Sequence boundary:
  - Cho phep ghi record co `sequence = UINT32_MAX`.
  - Sau do mark exhausted (`nextSequence = 0`) va chan write tiep.

### 6.3 Fee_Read
- Tim latest record cua block trong sector active (sequence lon nhat).
- Chi copy payload (`dataLength`) ra buffer user.

### 6.4 Fee_Swap
- Tim latest record cua moi block trong sector cu.
- Copy latest records sang sector spare theo thu tu slot lien tuc.
- Erase sector cu.
- Doi active sector.

## 7. Day-by-day implementation

### Day1 (FLS co ban)
- Tao `flash.bin` neu chua co, fill `0xFF`.
- Read/Write theo dia chi.
- Erase tung sector.
- Kiem tra persistence qua lan init tiep theo.

### Day2 (Fee append)
- Dinh nghia header record.
- Ghi append nhieu version cho 1 block.
- Kiem tra header/payload/slot free.
- In layout de debug.

### Day3 (Recovery + sequence)
- Recover state sau restart.
- Read latest version sau restart.
- Test sequence boundary voi `UINT32_MAX`.

### Day4 (Swap sector)
- Tu dong swap khi full sector.
- Giu lai latest record moi block.
- Erase sector cu sau swap.
- Verify du lieu sau restart.

## 8. Build va run (Windows + MinGW gcc)

Tu thu muc root project:

```bash
make
./main.exe
```

Khi chay `main.exe`, chuong trinh hien menu CLI:

```text
1. Write Block 1
2. Write Block 2
3. Write Block 3
4. Read Block 1
5. Read Block 2
6. Read Block 3
7. Show Layout
0. Exit
```

Ngoai menu so, CLI ho tro command text:

```text
write1 | w1 | read1 | r1
write2 | w2 | read2 | r2
write3 | w3 | read3 | r3
show | layout
exit | quit
```

Neu `Fee_Init` fail (vi `flash.bin` cu khong hop le), CLI se hoi co reset `flash.bin` hay khong.
Chon `y` de reset nhanh va tiep tuc demo.

Pattern du lieu de nhin nhanh khi demo:

- Block 1: bat dau tu `0x11`, tang dan moi lan write
- Block 2: bat dau tu `0xA1`, tang dan moi lan write
- Block 3: bat dau tu `0x51`, tang dan moi lan write

Neu muon clean nhanh (thu cong):

```bash
del main.exe
```

## 9. Test gate hien co

- `Test_RunDay1`: FLS init/read/write/persistence/erase.
- `Test_RunDay2`: append record + verify header/payload.
- `Test_RunDay3`: restart recovery + sequence edge.
- `Test_RunDay4`: swap flow + scenario write lan 17.

Luu y:

- Cac file test van duoc giu nguyen de tham khao/hoc tap.
- `main.c` hien tai khong auto goi test day gate nua, chi chay CLI visual theo yeu cau demo.

## 10. Ky thuat da duoc harden trong lan polish nay

- Sua loi nghiem trong trong `Fee_Read`:
  - truoc day doc ca slot 64 bytes vao buffer user
  - hien tai chi doc payload dung kich thuoc block
- Dong nhat xu ly `blockIndex` theo kieu signed (`int32_t`) de tranh so sanh sentinel sai.
- Bo sung comment ngan gon tieng Viet cho cac ham chinh.

## 11. Gia dinh va pham vi

- Khong mo phong wear leveling thuc te.
- Khong co CRC/ECC.
- Khong co power-fail atomicity day du nhu production FEE.
- Muc tieu chinh la hoc append-log, recovery, va swap co ban.

## 12. Cach doc nhanh layout

Ky hieu trong `Fee_ShowLayout`:

- `[.] FREE`: slot trong
- `[V]`: ban ghi latest cua block
- `[X]`: ban ghi cu hon
- `[!] INVALID/READ_ERROR`: du lieu hoac truy cap loi

## 13. Tai lieu de bai

- `mock_fee_c_training_5days_v2.md`
