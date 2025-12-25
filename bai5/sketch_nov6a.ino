void setup() {
  Serial.begin(9600);     // Cấu hình UART 9600 8N1
}

void loop() {
  Serial.println("Nghia IoT!");   // Gửi chuỗi
  delay(1000);                    // Chờ 1 giây gửi lại
}