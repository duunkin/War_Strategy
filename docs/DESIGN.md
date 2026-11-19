1. Tóm tắt thiết kế 1.0
   Thể loại: Game RTS (chiến thuật thời gian thực) nhìn từ trên xuống, đồ họa pixel, lập trình bằng C++ với thư viện SDL2, build bằng CMake (CLion hỗ trợ sẵn CMake), chạy trên cả Windows, Linux và macOS.
   Bản đồ: Kích thước 128×96 ô, tỉ lệ 5 đất : 1 biển và có biển bao quanh. Sử dụng thuật toán Simplex noise để sinh độ cao (biển, đất bằng, đồi núi), sau đó thêm rừng, sông hồ và hỗ trợ khởi tạo theo seed.
   Phe phái: Mỗi bên có một tư lệnh (tư lệnh chết là thua), quân đội có sẵn từ đầu trận và không có yếu tố kinh tế.
   Quân chủng: Gồm 5 loại là bộ binh, pháo binh, tăng thiết giáp, không quân và thủy quân (tàu chỉ bắn từ biển vào bờ).
   Chiến thuật: Có cơ chế sương mù chiến tranh. Địa hình ảnh hưởng trực tiếp đến tốc độ di chuyển, tầm nhìn, khả năng phòng thủ và phục kích.
   Điều khiển: Hỗ trợ chọn từng đơn vị, chọn theo vùng và ghép nhóm đơn vị.
   Điều kiện kết thúc ván: Tiêu diệt được tư lệnh đối phương, hoặc một bên hết quân chỉ còn lại tư lệnh, hoặc khi hết giờ thì phân thắng bại bằng cách so sánh số lượng quân.
   Chế độ chơi: Người đấu với AI, AI đấu với AI (phục vụ huấn luyện) và chế độ xem lại (replay).
2. Quyết định kiến trúc quan trọng nhất
   Tách biệt mô phỏng và hiển thị: Tách hẳn phần mô phỏng (sim/) khỏi phần hiển thị (SDL2) ngay từ đầu để phục vụ tính năng AI tự học và AI vs AI.
   Module sim/: Là lõi game thuần C++, không include thư viện SDL. Module này chạy theo bước thời gian cố định (ví dụ 20 tick/giây) và có thể chạy ở chế độ headless (không giao diện) với tốc độ nhanh gấp hàng trăm lần để AI tự đấu và huấn luyện.
   Module render/ và ui/: Chỉ đọc dữ liệu trạng thái từ sim/ để hiển thị lên màn hình.
   Module ai/: Đọc trạng thái game (bị giới hạn bởi sương mù chiến tranh, AI không được "nhìn lén") và gửi lệnh điều khiển tương tự như người chơi.
   Tính tất định (Deterministic): Cùng một seed và cùng chuỗi lệnh đầu vào sẽ luôn cho ra cùng một kết quả. Nhờ đó, tính năng replay chỉ cần lưu seed và danh sách lệnh thay vì lưu hình ảnh, đồng thời giúp việc phát triển chế độ chơi mạng kiểu lockstep sau này dễ dàng hơn. Để đảm bảo tính nhất quán giữa các nền tảng, cần tránh dùng số thực dấu phẩy động (float) không ổn định (thay bằng số nguyên hoặc fixed-point cho phần logic) và tự viết bộ sinh số ngẫu nhiên thay vì dùng std::rand.
   Phân chia công việc nhóm: Các thành viên có thể chia nhau phụ trách từng module gồm sinh bản đồ (map generation), mô phỏng/quân đội (simulation), AI và hiển thị/giao diện (render/UI). Nhóm sử dụng Git để quản lý mã nguồn và cần chốt sớm giao diện kết nối (interface) giữa các module.
3. Lộ trình đề xuất

Mốc
Nội dung
Kết quả kiểm tra
M1
Cửa sổ SDL2, vòng lặp game, camera cuộn, vẽ lưới ô
Chạy được trên cả 3 hệ điều hành
M2
Sinh bản đồ bằng Simplex + seed, rừng, sông hồ, biển; minimap
Cùng seed ra cùng bản đồ
M3
Đơn vị bộ binh: chọn, di chuyển, tìm đường (A*), ảnh hưởng địa hình
Điều khiển được quân trên bản đồ
M4
Chiến đấu: tầm đánh, sát thương, giáp, tư lệnh, điều kiện thắng
Chơi được ván đơn giản hai bên
M5
Thêm pháo binh, tăng, tàu, không quân; sương mù chiến tranh
Đủ 5 loại quân
M6
UI: bảng thông tin, ghép nhóm, menu, cài đặt, âm thanh
Giao diện dùng được
M7
AI chiến thuật cơ bản (luật + cây hành vi)
Người đấu được với AI
M8
Chế độ headless, AI vs AI, ghi log, replay
Chạy hàng nghìn trận tự động
M9
AI tự học
AI cải thiện sau các trận thua
M10
Cân bằng, sửa lỗi, đóng gói 3 nền tảng
Chơi trọn vẹn một ván, 1 giờ trở lên

4. Rủi ro cần lưu ý
   AI tự học là phần khó nhất: Huấn luyện bằng học tăng cường (reinforcement learning) cho game RTS rất nặng và tốn nhiều thời gian. Giải pháp vừa sức hơn cho phiên bản 1.0 là xây dựng AI chiến thuật bằng luật và cây hành vi ở mốc M7, sau đó cho AI "học" bằng cách tinh chỉnh các tham số (như mức độ hung hăng, thời điểm tấn công, cách chia quân) thông qua thuật toán tiến hóa hoặc tự điều chỉnh sau mỗi trận thua. Cách tiếp cận này mang lại hiệu quả rõ rệt mà không đòi hỏi hạ tầng machine learning phức tạp.
   Phạm vi quân đội rộng: Việc triển khai đủ 5 loại quân, bao gồm cả không quân và hải quân, có khối lượng công việc khá lớn. Nhóm nên hoàn thiện bộ binh trước, sau đó bổ sung dần các binh chủng còn lại theo lộ trình mốc M5.
   Hiệu năng cho ván đấu dài: Một trận đấu kéo dài từ 1 giờ trở lên trên bản đồ 128×96 đòi hỏi thuật toán tìm đường cho số lượng lớn đơn vị, hệ thống sương mù và AI phải vận hành mượt mà, do đó cần tối ưu hiệu năng ngay từ đầu.
   Đồ họa pixel: Cần chốt kích thước ô pixel sớm vì nếu thay đổi về sau sẽ phải vẽ lại toàn bộ tài nguyên đồ họa.
5. Các điểm cần chốt thêm & Phản hồi hiện tại
   Kích thước ô pixel (16×16, 32×32 hay 64×64; với bản đồ 128×96 ô ở mức 32×32 sẽ là 4096×3072 pixel nên cần camera cuộn và thu phóng):
   Đã chốt: Chọn kích thước 32×32 vì 64×64 quá nhiều pixel còn 16×16 lại quá ít, đồng thời tích hợp tính năng thu phóng (zoom) để giảm giật lag và đỡ mỏi mắt.
   Số lượng quân mỗi bên lúc bắt đầu (vài chục hay vài trăm đơn vị để xác định yêu cầu hiệu năng):
   Đã chốt: Chia lực lượng thành các sư đoàn với quy mô khoảng 3.000–5.000 quân.
   Mức độ mô phỏng chỉ số thực tế (mô phỏng sát thực tế như đạn đạo, xuyên giáp hay chỉ mượn thông số gần thực để cân bằng):
   Đã chốt: Giai đoạn đầu tập trung vào cơ chế cân bằng, phân chia thành đạn thường và đạn nổ (dùng để tiêu diệt bộ binh).
   Nguồn đồ họa pixel art (nhóm tự vẽ hay dùng bộ asset miễn phí):
   Sử dụng các bộ asset miễn phí.
   Phân công nhân sự trong nhóm (số lượng thành viên và thế mạnh từng người):
   Ghi chú: Bạn đảm nhận mảng thế mạnh là chiến thuật, thiết kế thông số, cân bằng game và địa hình.
   Tính năng lưu/tải game (với ván đấu 1 giờ trở lên, có cần lưu/tải giữa chừng không):
   Không lưu giữ chừng, chơi một ván liên tục.
   Bước tiếp theo trong tài liệu: Sau khi chốt xong các câu hỏi trên (hoặc chọn phương án mặc định), tài liệu thiết kế chi tiết (cấu trúc thư mục, các lớp chính, file CMakeLists.txt mẫu và mã nguồn khởi đầu cho M1, M2) sẽ được khởi tạo để nhóm bắt đầu lập trình.
6. Danh sách khí tài hiện đại được bổ sung
   Xe tăng & Thiết giáp (Tank):
   Leopard 2A: Tăng hạng nặng.
   T-14 Armata: Tăng hạng trung.
   Puma: Thiết giáp/tăng hạng nhẹ.
   Pháo binh:
   Caesar: Pháo tự hành.
   Gepard: Pháo phòng không tự hành.
   Không quân (Máy bay):
   Su-57: Máy bay tiêm kích.
   B-2 Spirit: Máy bay ném bom chiến lược.
   Thủy quân (Tàu chiến):
   JS Shiro FFM-3 (Lớp Mogami): Khinh hạm.
   JS DD-151 (Lớp Asagiri): Khu trục hạm.
7. Quy mô & Thông số chi tiết lực lượng
   Số lượng bộ binh mỗi đội: 30.000 quân (chia làm 10 sư đoàn, mỗi sư đoàn 3.000 quân).
8. Thông số & Đặc tính của Tư lệnh
   Máu: 100
   Sát thương: 50
   Vũ khí: Không chiến bằng trí não, trang bị súng ngắn Desert Eagle.
   Tầm bắn: 40m
   Giáp & Tốc độ: Giáp 0 | Tốc độ di chuyển 40 km/h
   Đặc tính kỹ năng: Hiển thị biểu tượng ngôi sao lớn (1 ô). Có khả năng tàng hình/ẩn nấp khi kẻ địch chưa tiến lại gần dưới 50m. Khi Tư lệnh chết, trận đấu lập tức kết thúc.
9. Chi tiết thông số các đơn vị quân đội
   Leopard 2A: Máu 2.000 | Tốc độ 60 km/h | Đạn xuyên giáp 200, Đạn nổ 200/15m² | Sư đoàn: 80 chiếc
   T-14 Armata: Máu 1.750 | Tốc độ 75 km/h | Đạn xuyên giáp 150, Đạn nổ 150/15m² | Sư đoàn: 100 chiếc
   Puma: Máu 1.250 | Tốc độ 80 km/h | Đạn xuyên giáp 100, Đạn nổ 100/10m² | Sư đoàn: 200 chiếc
   Caesar: Máu 1.000 | Tốc độ 55 km/h | Đạn nổ 200/30m² | Sư đoàn: 50 khẩu
   Gepard: Máu 750 | Tốc độ 65 km/h | Đạn xuyên giáp 20x50 viên | Sư đoàn: 50 khẩu
   Su-57: Máu 500 | Tốc độ 1.000 km/h | Tên lửa theo dõi 200 | Sư đoàn: 50 chiếc
   B-2 Spirit: Máu 1.000 | Tốc độ 850 km/h | Bom thường 400, Bom diện rộng 400/40m² | Sư đoàn: 10 chiếc
   JS Shiro FFM-3 (Mogami): Máu 4.000 | Tốc độ 30 hải lý/h | Đạn nổ 300/35m² | Sư đoàn: 20 tàu
   JS DD-151 (Asagiri): Máu 3.500 | Tốc độ 40 hải lý/h | Đạn nổ 150/20m² | Sư đoàn: 50 tàu


