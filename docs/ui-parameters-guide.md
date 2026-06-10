# Hướng dẫn tham số UI — WindowHelper

Tất cả các tham số có thể chỉnh trực tiếp trong cửa sổ tool. Thay đổi auto-save sau ~500ms (debounce) hoặc bấm "Lưu ngay". Một số tham số có hot-reload (áp dụng ngay), số khác cần restart tool.

---

## Header (thông tin trạng thái)

| Mục | Ý nghĩa |
|-----|---------|
| **Đã gắn: Priston Tale [HWND=...]** | Tool đã tìm thấy và bám cửa sổ game. Nếu hiển thị "Chưa tìm thấy" → mở PT trước rồi restart tool. |
| **Cursor: screen=(X,Y) client=(X,Y) clientSize=WxH** | Vị trí chuột hiện tại theo screen coords và client coords (so với cửa sổ game). Dùng để calibrate tọa độ pot trong kho khi cần. |
| **AUTO (Đánh quái)** | Bật/tắt combat (đánh quái + buff). Toggle F8 hoặc click. Áp dụng ngay (không cần restart). |

---

## Section "Hồi phục" (pot evaluator — uống pot ingame)

Tự động uống pot HP/MP/SP khi máu/mana xuống dưới ngưỡng.

| Tham số | Default | Ý nghĩa |
|---------|---------|---------|
| **Ngưỡng hồi HP (%)** | 70% | Khi HP < N% → bấm key `1` (uống pot HP ingame). |
| **Ngưỡng hồi MP (%)** | 40% | Khi MP < N% → bấm `3`. |
| **Ngưỡng hồi SP (%)** | 40% | Khi SP < N% → bấm `2`. |
| **Ngưỡng recall HP (%)** | 18% | HP cực thấp dưới ngưỡng này → bấm phím recall (F12) để về thành. |
| **Recall ổn định (ms)** | 3000 | HP phải dưới ngưỡng recall liên tục N ms mới fire recall. Tránh recall khi HP nhấp nháy 1 frame. |
| **Cooldown bình (ms)** | 1500 | Sau khi fire pot, chờ N ms mới fire pot kế tiếp (cùng resource). Tránh spam pot khi vision chậm. |
| **Số frame xác nhận** | 2 | HP/MP/SP phải dưới ngưỡng liên tục N frame mới fire pot. Tránh false positive do vision glitch 1 frame. |

---

## Section "Đánh quái" (combat FSM)

Logic chọn mob + click attack + buff cycle.

| Tham số | Default | Ý nghĩa |
|---------|---------|---------|
| **Chu kỳ buff (giây)** | 300 | Sau N giây attack, restart buff cycle. Tránh trôi buff timer. |
| **Chờ tối thiểu khi đổi mục tiêu (ms)** | 6000 | Sau khi click 1 mob, ít nhất phải đợi N ms mới được pick mob khác. Cần ≥ engagement lock + jitter. |
| **Chờ tối đa khi đổi mục tiêu (ms)** | 22000 | Sau N ms attack 1 mob mà chưa chết → force pick mob khác. Escape hatch cho mob bug/immortal. Cover p95 TTK + safety. |
| **Khoá đánh sau shift+phải (ms)** | 5000 | Sau click shift+phải attack, im lặng N ms để game tự auto-chain attack mob đó. Tránh spam click khi đang đánh. |
| **Dao động khoá đánh (ms)** | 500 | Random hoá độ dài khoá ±N ms. Anti-pattern detect (không cố định 5000ms). |
| **Bán kính đánh (min)** | 60 | Khoảng cách tối thiểu (px) từ tâm nhân vật đến điểm click attack. |
| **Bán kính đánh (max)** | 180 | Khoảng cách tối đa. Bot sẽ click ngẫu nhiên trong vành đai [min, max]. |
| **Chờ đủ MP mới buff** | ON | Nếu MP < ngưỡng → bỏ qua buff cycle, chờ MP regen. |
| **Ngưỡng MP để buff (%)** | 15% | Ngưỡng tối thiểu MP để cho phép buff. |

---

## Section "Nạp pot từ kho" (pot refill)

Tự động mở kho theo interval, di chuột tới slot pot, Shift+1/2/3 để refill.

### Cấu hình chính

| Tham số | Default | Ý nghĩa |
|---------|---------|---------|
| **Bật nạp pot tự động** | OFF | Master switch. OFF = feature tắt hoàn toàn. |
| **Phím mở kho (VK)** | 86 ('V') | Mã virtual-key của hotkey toggle inventory. VK của PT là V (86). |
| **HP mỗi N giây** | 0 | Refill HP sau N giây kể từ lần refill HP cuối. 0 = tắt slot này. |
| **SP mỗi N giây** | 0 | Tương tự cho SP. |
| **MP mỗi N giây** | 0 | Tương tự cho MP. |

### Tinh chỉnh (delays)

Timeline 1 lần refill: `tap V` → `Chờ kho mở` → `move mouse` → `Chờ chuột di chuyển` → `Shift+N` → `Chờ sau Shift+N` → `tap V (close)` → `Chờ kho đóng` → restore cursor.

| Tham số | Default | Ý nghĩa |
|---------|---------|---------|
| **Chờ kho mở (ms)** | 400 | Sau khi nhấn V, đợi N ms cho animation kho mở xong rồi mới di chuột. |
| **Chờ kho đóng (ms)** | 200 | Sau khi nhấn V đóng kho, đợi N ms rồi mới restore cursor. |
| **Chờ chuột di chuyển (ms)** | 150 | Sau khi di chuột tới slot, đợi N ms rồi mới Shift+N. Cho game kịp nhận hover state. |
| **Chờ sau Shift+N (ms)** | 200 | Sau khi bấm Shift+1/2/3, đợi N ms rồi mới di chuột sang slot kế hoặc đóng kho. |
| **Timeout toàn refill (ms)** | 10000 | Nếu refill chưa xong trong N ms → force cleanup (release Shift, đóng kho, restore cursor). Tránh treo. |
| **Ngưỡng abort khi HP thấp (%)** | 30% | Nếu HP < N% giữa lúc refill → hủy refill, đóng kho, để pot evaluator uống pot HP ingame. |
| **Tạm dừng sau khi hủy (ms)** | 30000 | Sau abort, chờ N ms mới thử refill lại. Tránh loop abort→retry khi HP cycle quanh ngưỡng. |

### Lưu ý

- Tọa độ slot pot trong kho được **hardcode** trong code (đo từ PT, scale theo kích thước window). Không edit qua UI.
- Refill chạy độc lập với AUTO combat. Có thể bật refill mà tắt AUTO (chỉ refill, không đánh quái).

---

## Section "Buff"

4 buff slot, mỗi slot cấu hình 1 phím + delay + có shift+right-click sau buff hay không.

| Tham số | Ý nghĩa |
|---------|---------|
| **Bật buff N** | ON/OFF slot buff thứ N. Tắt = skip slot này trong buff cycle. |
| **Mã phím** | Phím F1-F12 dùng để cast buff (skill mapped trong game). |
| **Delay sau cast (ms)** | Đợi N ms sau khi cast buff trước khi cast buff kế. |
| **Chuột phải sau buff** | ON = sau cast skill, click chuột phải vào tâm nhân vật (self-buff target). OFF = chỉ press key. |

---

## Nút "Lưu ngay"

- Mọi thay đổi auto-save sau ~500ms (debounce).
- Nút "Lưu ngay" ép save ngay (bỏ debounce).
- Trạng thái: `(chưa lưu)` = dirty, `(đã lưu)` = clean.

---

## Hot-reload vs Restart

| Tham số | Hot-reload | Cần restart |
|---------|-----------|-------------|
| AUTO toggle | ✅ | |
| Ngưỡng / threshold | ✅ | |
| Delay / cooldown | ✅ | |
| Refill enable + intervalSec | ✅ | |
| Buff slot key / delay | ✅ | |
| Backend (PostMessage/SendInput) | | ✅ |
| ROI vision (config.json) | | ✅ |

Đa số tham số áp dụng ngay sau khi save. Nếu thấy không có hiệu lực → bấm "Lưu ngay" và đợi vài tick vision.

---

## Hướng dẫn build + share (cho developer)

### Build release mới và đóng gói

```powershell
# Cách 1: build mới + zip (full)
.\package.ps1

# Cách 2: đã build sẵn, chỉ đóng gói lại
.\package.ps1 -SkipBuild

# Cách 3: kèm version tag
.\package.ps1 -Version 1.0.0
# → WindowHelper-v1.0.0.zip
```

Output:
- Folder `dist/` chứa exe + config + docs
- File zip `WindowHelper-<timestamp>.zip` ~1.4MB

### Đổi tên exe (anti Xingcode3 signature)

Mỗi lần re-configure CMake sẽ random tên exe:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --preset portable
.\package.ps1
# → exe có tên svc_<random>.exe khác
```

### Dev build (nhanh, không static)

Khi đang phát triển hằng ngày, dùng build dynamic trong `build/`:

```powershell
cmake --build build --config Release --target WindowHelper
# Output: build\bin\Release\svc_*.exe (cần kèm DLLs để chạy)
```

Chỉ chạy `package.ps1` (static portable) khi muốn release cho user.

---

## Hướng dẫn cho user nhận tool

Gửi 1 file zip duy nhất: **`WindowHelper-*.zip`** (~1.4MB).

### Họ làm
1. Extract zip vào folder bất kỳ (vd `C:\WindowHelper\`)
2. Mở **Priston Tale** trước
3. Double-click **`svc_*.exe`** (tên file trong zip)
4. Bật **AUTO** trong UI hoặc nhấn **F8** toggle
5. Bot bắt đầu đánh quái + uống pot + (optional) refill từ kho

### Yêu cầu máy
- Windows 10 1809+ hoặc Windows 11
- **KHÔNG cần cài** VC++ Redist, .NET, OpenCV, hay bất kỳ thư viện nào
- **KHÔNG cần admin rights** (trừ khi PT cần)

### File trong zip
| File | Mục đích |
|------|----------|
| `svc_*.exe` | Bot chính |
| `config.json` | Cấu hình |
| `HUONG-DAN-CAU-HINH.md` | Tài liệu này |
| `README.txt` | Hướng dẫn ngắn |

### Calibrate ROI vision (nếu cần)

Nếu PT của họ chạy ở resolution/skin khác, ROI HP/MP/SP bar trong `config.json` có thể không khớp. Triệu chứng: log `[vision] valid=0` liên tục hoặc HP/MP/SP đọc sai.

Hiện chưa có UI calibrate trực quan — phải edit `config.json` thủ công. Field cần chỉnh: `advanced.roi.hp.region` (x, y, w, h), tương tự cho mp/sp.

Tọa độ pot trong kho **không cần calibrate** — bot tự scale theo kích thước window thực tế.

### Antivirus flag exe?

Static linked exe đôi khi bị heuristic flag false-positive:
- Whitelist exe trong AV
- Hoặc dùng version mới với tên exe khác (re-configure CMake)
- Không bao giờ disable AV toàn hệ thống vì 1 exe

### Crash startup?

- Windows quá cũ (< Win10 1809): không có WGC API → upgrade Windows
- PT chưa mở trước khi chạy bot: mở game trước
- HWND không tìm thấy: kiểm tra window title của PT đúng "Priston Tale" hoặc "PristonTale"
