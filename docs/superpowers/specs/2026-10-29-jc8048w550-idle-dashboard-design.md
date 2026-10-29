# Màn hình chờ cho Guition JC8048W550: giờ, thời tiết, GitHub AI trending

Ngày: 2026-10-29. Board: `main/boards/guition/jc8048w550/`. Mọi thay đổi nằm trong thư mục board này, không đụng lõi dùng chung.

## Ý định

Bố cục hai phần đã làm xong: cột trái 240px là emoji (GIF động 240x240, sát mép dưới), vùng phải 560px đổi giữa chat (lúc nói chuyện) và bảng thông tin (lúc chờ). Hiện bảng thông tin chỉ có các thẻ mẫu, xấu và không có dữ liệu thật.

Mục tiêu: biến vùng phải lúc chờ thành một bảng thông tin đẹp, dùng dữ liệu thật, để nhìn thiết bị lúc rảnh có ích như một đồng hồ để bàn kiêm bảng tin.

## Mục tiêu

1. **Thẻ chung giờ và thời tiết** (một thẻ):
   - Bên trái: giờ lớn (HH:MM), ngày tháng đầy đủ, tên thành phố.
   - Bên phải: thời tiết hôm nay gồm icon, nhiệt độ hiện tại, mô tả ngắn, cao/thấp, độ ẩm.
   - Hàng dưới: 7 ngày tiếp theo, mỗi ngày có thứ viết tắt, icon, nhiệt độ cao/thấp.
2. **Danh sách GitHub top 5 repo AI**, mỗi repo một dòng: hạng (huy hiệu vàng, bạc, đồng cho top 3), `owner/repo`, mô tả ngắn, số sao. Tiêu đề phần: "GitHub · AI trending".
3. **Chạm vào một repo** mở thẻ chi tiết đè lên vùng phải: tên đầy đủ, mô tả đầy đủ, ngôn ngữ chính, số sao, số fork, các topic, ngày cập nhật gần nhất. Tất cả lấy từ lần gọi danh sách, không gọi thêm. Chạm bên ngoài thẻ hoặc nút đóng thì quay lại danh sách.
4. **Dữ liệu thật**, thiết bị tự gọi:
   - Danh sách repo: API tìm kiếm của GitHub.
   - Thời tiết: Open-Meteo (không cần khóa).
   - Vị trí: suy ra từ địa chỉ IP mạng.
   - Giờ và ngày: đồng hồ của thiết bị.
5. Giao diện đẹp: icon thời tiết rõ ràng, màu và khoảng cách nhất quán trên nền `#18181C`.

Bố cục đã được người dùng duyệt qua mockup `2026-10-29-jc8048w550-idle-dashboard-mockup.html` (cùng thư mục, mở bằng trình duyệt, vẽ ở 80% kích thước thật): thẻ chung ở trên, tiêu đề phần, rồi 5 dòng repo. Dùng nó làm chuẩn cho tỷ lệ, thứ tự và màu: nền `#18181C`, thẻ `#24242c`, chữ phụ `#8f8fa0`, số sao `#f5c542`, huy hiệu hạng 1/2/3 vàng `#f5c542`, bạc `#c0c0c8`, đồng `#cd7f32`, các hạng còn lại xanh `#3b82f6`. Chữ trong mockup cỡ khoảng 12px ở 80%, tức khoảng 15-16px ở kích thước thật.

## Không phải mục tiêu

- Tóm tắt do AI viết, tải README của repo.
- Giao diện cấu hình thành phố, đơn vị, nguồn tin. Vị trí luôn tự suy ra theo IP, đơn vị là °C.
- Dự báo theo giờ, cảnh báo thời tiết, thông báo đẩy.
- Mã truy cập GitHub (dùng gọi ẩn danh), lưu dữ liệu vào flash/NVS.
- Cào trang `github.com/trending`.
- Đổi hành vi chat, trạng thái thiết bị, chuyển chế độ chờ/chat đã có.

## Điều không được xảy ra

- Treo vòng lặp chính, tác vụ LVGL hoặc tác vụ âm thanh vì gọi mạng. Việc gọi mạng chạy ở tác vụ riêng, kết quả đưa vào giao diện dưới khóa màn hình.
- Làm hết RAM nội. Log trên thiết bị cho thấy RAM nội còn trống khoảng 136 KB và mức thấp nhất từng gặp chỉ khoảng 33 KB. Mỗi lần bắt tay TLS tốn đáng kể RAM nội, nên không được gọi mạng khi đang nói chuyện, và phải đo RAM nội trước và sau.
- Làm màn hình nháy nhiều hơn hiện nay. Chỉ cập nhật chữ và icon khi dữ liệu thật sự đổi, không vẽ lại liên tục.
- Hiển thị dữ liệu cũ như thể mới. Nếu lấy dữ liệu thất bại thì giữ dữ liệu lần trước và đánh dấu rõ là cũ, hoặc hiện trạng thái "chưa có dữ liệu" nếu chưa từng lấy được.
- Bị GitHub chặn vì gọi quá nhiều.

## Ràng buộc

- Quy tắc trong `AGENTS.md`: chỉ sửa trong thư mục board, lõi dùng `Board`/`Display`, mọi thay đổi `Application` qua `Schedule()`, không chặn tác vụ chính, kiểm tra dữ liệu mạng và giữ đúng quyền sở hữu `cJSON`.
- Font chữ mặc định của board là `noto_sans_basic_30`, quá lớn cho 5 dòng repo cộng thẻ chung trong 480px. Gói font có sẵn các cỡ 14, 16 và 20 (`font_noto_sans_common_*`, và các bản `basic` tương ứng): dùng cỡ 16-20 cho nội dung thẻ và danh sách, cỡ lớn hơn cho giờ. Nếu đổi font mặc định của board trong `main/CMakeLists.txt` thì ảnh hưởng cả khung chat, cần kiểm tra lại. Font mặc định chỉ có ký tự Latin cơ bản. Mọi chữ do mình sinh ra (thứ trong tuần, mô tả thời tiết, tên thành phố) dùng ASCII: Mon, Tue, "Light rain". Tên thành phố và mô tả repo có thể chứa ký tự ngoài ASCII, cần có cách hiển thị an toàn (thay bằng `?` hoặc bỏ).
- Font icon có sẵn chỉ có khoảng 96 ký hiệu và không có icon thời tiết (nắng, mưa, mây). Cần vẽ bằng LVGL hoặc nhúng thành ảnh nhỏ.
- Nội dung API GitHub: bắt buộc gửi header `User-Agent`. API tìm kiếm ẩn danh giới hạn khoảng 10 yêu cầu mỗi phút.
- Đã biết trên thiết bị: `LCD_PCLK_HZ` hạ xuống 12 MHz làm panel mất hình, phải giữ 16 MHz. Emoji GIF động đang gây nháy nhẹ ở mép phải và người dùng chấp nhận, nên không làm nặng thêm việc vẽ.
- Build bằng ESP-IDF v6.1, nền tảng chỉ có Wi-Fi (`WifiBoard`). HTTPS gọi qua `Board::GetInstance().GetNetwork()->CreateHttp(...)`.

## Hành vi

- **Khi nào lấy dữ liệu (lựa chọn của mình, người xây có thể chỉnh):** chỉ khi Wi-Fi đã kết nối, giờ hệ thống đã đồng bộ, và thiết bị đang ở trạng thái chờ. Lần đầu lấy ngay khi đủ điều kiện, sau đó thời tiết làm mới khoảng 30 phút một lần, danh sách GitHub khoảng 60 phút một lần.
- **Truy vấn GitHub (lựa chọn của mình, người xây kiểm tra lại):** repo chủ đề AI (ví dụ topic `ai`, `llm`, `machine-learning`, `agents`), tạo trong khoảng 7 ngày gần nhất, xếp theo số sao giảm dần, lấy 5 kết quả. Người dùng chấp nhận đây là xấp xỉ "đang hot", không phải số sao tăng trong hôm nay.
- **Thời tiết:** gọi vị trí theo IP trước (vĩ độ, kinh độ, tên thành phố) bằng một dịch vụ HTTPS không cần khóa, rồi gọi Open-Meteo với tọa độ đó. Hôm nay lấy từ dữ liệu hiện tại và ngày hôm nay, 7 ngày sau lấy từ dự báo theo ngày. Vị trí được lưu trong RAM, chỉ tra lại khi thiết bị khởi động hoặc mất mạng quá lâu.
- **Biểu tượng:** ánh xạ mã thời tiết (WMO) của Open-Meteo sang một bộ icon nhỏ (nắng, ít mây, nhiều mây, sương mù, mưa, giông, tuyết).
- **Thẻ chi tiết:** mở khi chạm vào một dòng repo. Tự đóng khi thiết bị rời trạng thái chờ (bắt đầu trò chuyện). Chạm vào chỗ khác của màn hình chờ (ngoài các dòng repo) vẫn bắt đầu trò chuyện như hiện nay. Khi thẻ chi tiết đang mở, chạm ra ngoài chỉ đóng thẻ, không bắt đầu trò chuyện.
- **Mạng lỗi hoặc chưa có dữ liệu:** thẻ giờ vẫn chạy được từ đồng hồ thiết bị. Phần thời tiết và danh sách hiện trạng thái "chưa có dữ liệu" rõ ràng.
- Dữ liệu chỉ nằm trong RAM, không lưu qua lần khởi động lại.

## Quyền riêng tư

Địa chỉ IP của người dùng được gửi tới dịch vụ tra vị trí theo IP, và vị trí gần đúng gửi tới Open-Meteo. Nêu rõ điều này trong README của board.

## Để người xây quyết định

- Dịch vụ tra vị trí theo IP cụ thể (cần HTTPS, không khóa, trả về tọa độ và tên thành phố), kiểm tra giới hạn và độ ổn định.
- Cách vẽ icon thời tiết (hình vẽ bằng LVGL hay ảnh nhúng) và cách ngắt dòng, cắt chữ cho vừa các dòng repo.
- Cách tách mã: các tệp mới trong thư mục board (lấy dữ liệu, mô hình dữ liệu, giao diện), nhớ đăng ký vào `main/CMakeLists.txt` nếu cần.
- Cách phân tích JSON tiết kiệm RAM (kích thước phản hồi của API tìm kiếm với 5 kết quả khá lớn).
- Chiến lược bộ nhớ cho TLS (có thể đặt bộ đệm TLS vào PSRAM nếu cấu hình cho phép) và cách tránh tranh với âm thanh và wake word.
- Tách phần phân tích dữ liệu thành hàm thuần để có thể kiểm thử trên máy chủ nếu thuận tiện.

## Kiểm tra

- Build `guition/jc8048w550` thành công, chạy test host.
- Trên thiết bị (cần phần cứng): dữ liệu hiện đúng, icon đúng với mã thời tiết, chạm repo mở và đóng thẻ chi tiết, chạm chỗ khác vẫn bắt đầu trò chuyện, mất Wi-Fi vẫn hiển thị giờ và trạng thái "chưa có dữ liệu", RAM nội trống sau khi lấy dữ liệu, không nháy hơn trước.
- Báo rõ phần nào chưa kiểm tra được vì cần phần cứng.

## Đã chốt bổ sung sau khi người đọc kiểm tra

Đã có sẵn trong dự án:
- **Múi giờ:** đồng hồ thiết bị đã là giờ địa phương. `main/ota.cc` đặt giờ hệ thống từ `server_time` kèm `timezone_offset` của máy chủ OTA. Giờ hiển thị lấy từ đồng hồ này, không phụ thuộc phản hồi thời tiết hoặc tra IP. Nếu giờ chưa đồng bộ (năm trước 2025) thì thẻ giờ hiện `--:--`.
- **RAM và TLS:** PSRAM đã bật (octal, 80 MHz), `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC` và `CONFIG_MBEDTLS_DYNAMIC_BUFFER` đã bật sẵn, nên bộ đệm TLS đã ưu tiên PSRAM. Board đã có `sdkconfig_append` trong `config.json`, được phép thêm tùy chọn ở đó nếu cần.

Lựa chọn của mình (người xây có thể chỉnh, nêu lại trong báo cáo cuối):
- **Đang làm mới khi bắt đầu trò chuyện:** để yêu cầu đang chạy kết thúc (giới hạn thời gian chờ khoảng 10 giây), không hủy giữa chừng. Không bắt đầu yêu cầu mới trừ khi thiết bị đang ở trạng thái Idle (không tính Connecting, Listening, Speaking, Standby của chế độ khác). Trước mỗi lần lấy, bỏ qua nếu RAM nội trống thấp hơn ngưỡng an toàn mà người xây đo được trên thiết bị và ghi lại.
- **Tra vị trí theo IP:** chỉ dùng HTTPS, không dùng HTTP thuần. Nếu không có dịch vụ nào đạt thì thêm câu hỏi cho người dùng thay vì tự hạ xuống HTTP.
- **Tên thành phố khi tra IP lỗi:** hiện `--`. Thẻ giờ vẫn hiện bình thường. Nếu lấy được thời tiết trước đó thì giữ tên thành phố cũ.
- **Dữ liệu cũ:** mỗi nguồn (thời tiết, GitHub) có thời điểm lấy thành công gần nhất. Nếu lần lấy mới nhất thất bại, hoặc dữ liệu cũ hơn 2 chu kỳ làm mới, thì làm mờ phần đó và hiện nhãn nhỏ `offline`. Chưa từng lấy được thì hiện `no data`.
- **Ký tự ngoài ASCII** trong tên thành phố, mô tả và topic: thay bằng `?`.
- **Truy vấn GitHub:** nếu API không hỗ trợ gộp nhiều topic bằng OR như mô tả, dùng một lần gọi duy nhất (ví dụ topic `llm` hoặc `ai`, chọn cái cho kết quả hợp lý nhất khi thử), không gộp nhiều lần gọi, để giữ trong giới hạn tốc độ và RAM.
- **Chạm:** thẻ chung giờ/thời tiết và mọi vùng ngoài các dòng repo vẫn bắt đầu trò chuyện. Chỉ các dòng repo mở thẻ chi tiết. Khi thẻ chi tiết mở, một lớp phủ toàn vùng phải nhận chạm: chạm ngoài thẻ chỉ đóng thẻ, không bắt đầu trò chuyện. Người xây quyết định cách chặn sự kiện nổi lên khỏi `OnTap`.
- **Kiểm thử trên máy chủ:** repo chỉ có Python unittest, không có hạ tầng test C++. Không thêm hạ tầng mới. Viết hàm phân tích JSON thuần, kiểm tra bằng dữ liệu mẫu thật ghi lại trên thiết bị (log) và bằng chạy trên phần cứng.
