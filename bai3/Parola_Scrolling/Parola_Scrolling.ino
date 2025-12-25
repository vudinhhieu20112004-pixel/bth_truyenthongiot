#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 10

MD_Parola parola = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

String inputText = "CHAO BAN";  // Dòng mặc định
bool newMessage = false;

void setup() {
  Serial.begin(9600);
  parola.begin();
  parola.displayClear();
  parola.displayText((char*)inputText.c_str(), PA_CENTER, 80, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

void loop() {
  // Đọc dữ liệu từ Virtual Terminal
  if (Serial.available() > 0) {
    inputText = Serial.readStringUntil('\n');  // đọc tới khi nhấn Enter
    inputText.trim();  // loại bỏ khoảng trắng và ký tự thừa

    if (inputText.length() > 0) {
      newMessage = true;
    }
  }

  // Khi có tin nhắn mới → reset ngay và hiển thị dòng mới
  if (newMessage) {
    parola.displayClear();       // xóa toàn bộ nội dung cũ
    parola.displayReset();       // reset hiệu ứng
    parola.displayText((char*)inputText.c_str(), PA_CENTER, 20, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    newMessage = false;
  }

  // Cho hiệu ứng chạy liên tục
  if (parola.displayAnimate()) {
    parola.displayReset();
  }
}
