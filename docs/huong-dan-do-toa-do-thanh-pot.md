# Hướng dẫn đo tọa độ thanh HP / SP / MP cho server PT khác

Khi mang tool sang server PT khác (đổi độ phân giải hoặc vị trí HUD), phải đo lại vùng quét cho 3 thanh. Hướng dẫn này dùng cursor coord đã có sẵn trong tool — **không cần screenshot, không cần Paint**.

---

## Hiểu hệ tọa độ trước

Tool quét theo **pixel của vùng client cửa sổ game** (không tính title bar Windows).

- `(0, 0)` = góc trái-trên vùng client cửa sổ game.
- `X` tăng sang phải, `Y` tăng xuống dưới.
- Đơn vị = pixel theo size cửa sổ game **lúc tool đang chạy**.

Server đổi resolution → tọa độ thay đổi → phải đo lại.

---

## Quy trình đo (làm cho từng thanh)

### Chuẩn bị

1. Mở game, đăng nhập vào server PT đích.
2. **Đứng yên cho cả HP, SP, MP đều đầy 100%** (uống pot nếu cần).
3. Mở tool WindowHelper. Trên header tool có dòng:
   ```
   Cursor: screen=(X,Y) client=(X,Y) clientSize=WxH
   ```
   Số `client=(X,Y)` chính là tọa độ mình cần đo. Nó cập nhật khi chuột di chuyển.

### Đo thanh HP

1. **Không click vào tool** — giữ focus ở game.
2. Di chuột tới **góc trái-trên** của thanh HP (vị trí pixel đầu tiên có màu đỏ ở góc trên-trái).
3. Liếc sang tool, đọc `client=(X1, Y1)`. Ghi ra giấy / nhớ tạm.
4. Di chuột tới **góc phải-dưới** của thanh HP (pixel cuối có màu đỏ ở góc dưới-phải).
5. Đọc `client=(X2, Y2)`.
6. Tính 4 số:

| Ô nhập trong tool | Công thức |
|---|---|
| Tọa độ X | `X1` |
| Tọa độ Y | `Y1` |
| Chiều rộng | `X2 − X1` |
| Chiều cao | `Y2 − Y1` |

7. Mở tool → bấm nút xanh **"Hiệu chỉnh nhận diện HP/MP/SP (cho server khác)"**.
8. Mở mục "Thanh HP (máu)" → nhập 4 số.
9. Xem dòng **"Nhận diện hiện tại: X%"** ngay phía trên.
   - ≈100% → đúng vùng. Sang thanh kế tiếp.
   - <80% hoặc thấp bất thường → đo lại / điều chỉnh ±1-2 px.

### Đo thanh SP

Lặp đúng quy trình trên cho thanh SP (màu vàng).

### Đo thanh MP

Lặp cho thanh MP (màu xanh).

---

## Lưu lại để dùng nhanh lần sau

Sau khi cả 3 thanh đều hiển thị ≈100%:

1. Trong window calibration, nhập **Tên server / ghi chú** (ví dụ `ptvn-1024`, `pttitan-fhd`).
2. Bấm **"Lưu thành cấu hình"**.
3. File JSON được tạo trong thư mục `presets\` cạnh exe.

Lần sau mở tool:
- Bấm **"Hiệu chỉnh nhận diện HP/MP/SP"**.
- Chọn cấu hình từ dropdown **"Chọn cấu hình"** → 3 thanh tự nhận diện lại.

---

## Lưu ý quan trọng

- **Cursor coord chỉ refresh khi chuột di chuyển.** Đứng yên = số đứng yên. Di chuột rồi nhìn nhanh tool.
- **Đo theo pixel chính xác.** Sai 5-10 px là detect tụt xuống 50-80% → bot sẽ uống pot sai.
- **Không thay đổi resolution game** sau khi đo. Đổi size cửa sổ → phải đo lại.
- **Bar nằm ngang** (server custom HUD): vẫn đo cùng cách, chỉ ra `Chiều rộng > Chiều cao` thay vì ngược lại.

---

## Khi nào cần đo lại

- Đổi resolution game (windowed ↔ fullscreen, 1024 ↔ 1280, …).
- Đổi server có UI khác.
- Update game, vị trí HUD dịch chuyển.

---

## Kiểm tra sau khi đo xong

1. Bật tool, vào game đầy máu → cả 3 thanh trong calibration hiển thị ≈100%.
2. Uống 1 viên pot HP → xem % HP có giảm theo đúng tỉ lệ không.
3. Đi đánh quái cho HP/MP giảm → các % thay đổi mượt, không nhảy lung tung.

Nếu OK → tool đã sẵn sàng dùng trên server này.
