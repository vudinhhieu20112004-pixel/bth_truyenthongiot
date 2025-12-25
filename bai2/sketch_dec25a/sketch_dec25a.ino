#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

// Khởi tạo thư viện
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

// Định nghĩa các chân (dựa trên hình ảnh)
#define BUZZER_PIN 12
#define MODE_PIN   11
#define UP_PIN     10
#define DOWN_PIN   9
#define SET_PIN    8

// Biến lưu trữ báo thức
byte alarmHour = 6;
byte alarmMinute = 0;
bool isAlarmOn = false;
bool isAlarmRinging = false;

// Biến cho máy trạng thái (state machine)
enum Mode { 
  DISPLAY_TIME, 
  SET_ALARM_HOUR, 
  SET_ALARM_MINUTE 
};
Mode currentMode = DISPLAY_TIME;
Mode lastMode = (Mode)-1; // Dùng để phát hiện thay đổi mode

// Biến cho xử lý nút nhấn (chống dội & nhấn giữ)
bool lastUpState = HIGH;
bool lastDownState = HIGH;
bool lastSetState = HIGH;

unsigned long modePressStartTime = 0;
bool modeLongPressHandled = false;
unsigned long ignoreButtonsUntil = 0; // Chống dội khi tắt báo thức

// Biến hiệu ứng nhấp nháy
bool blinkState = false;
unsigned long lastBlinkTime = 0;


void setup() {
  // Khởi tạo LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);

  // Khởi tạo & Kiểm tra RTC
  if (!rtc.begin()) {
    lcd.print("RTC not found!");
    while (1); // Dừng nếu không tìm thấy RTC
  }

  // Cài đặt thời gian cho RTC từ máy tính nếu RTC chưa chạy
  if (!rtc.isrunning()) {
    lcd.clear();
    lcd.print("RTC adjust...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    delay(2000);
  }

  // Cài đặt Pin
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Tắt còi lúc khởi động

  // Cài đặt các nút nhấn với điện trở kéo lên nội
  pinMode(MODE_PIN, INPUT_PULLUP);
  pinMode(UP_PIN, INPUT_PULLUP);
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(SET_PIN, INPUT_PULLUP);

  lcd.clear();
}

void loop() {
  // Xử lý logic nút nhấn
  handleButtons();

  // Kiểm tra báo thức
  checkAlarm();

  // Cập nhật màn hình LCD
  updateDisplay();
}

/**
 * @brief Xử lý tất cả logic nút nhấn (nhấn, thả, nhấn giữ)
 */
void handleButtons() {
  // Bỏ qua nếu đang trong thời gian "chờ" (sau khi tắt báo thức)
  if (millis() < ignoreButtonsUntil) {
    // Cập nhật trạng thái cũ để tránh nhấn nhầm
    lastUpState = (digitalRead(UP_PIN) == LOW);
    lastDownState = (digitalRead(DOWN_PIN) == LOW);
    lastSetState = (digitalRead(SET_PIN) == LOW);
    return;
  }

  // Đọc trạng thái các nút
  bool modeState = (digitalRead(MODE_PIN) == LOW);
  bool upState = (digitalRead(UP_PIN) == LOW);
  bool downState = (digitalRead(DOWN_PIN) == LOW);
  bool setState = (digitalRead(SET_PIN) == LOW);

  // 1. Tắt báo thức đang kêu
  if (isAlarmRinging && (modeState || upState || downState || setState)) {
    isAlarmRinging = false;
    digitalWrite(BUZZER_PIN, LOW);
    // Bỏ qua các nút nhấn trong 500ms để tránh thao tác nhầm
    ignoreButtonsUntil = millis() + 500; 
    return;
  }

  // 2. Xử lý nút MODE (Nhấn ngắn & Nhấn giữ)
  if (modeState) { // Nút đang được nhấn
    if (modePressStartTime == 0) { // Mới nhấn
      modePressStartTime = millis();
      modeLongPressHandled = false;
    } else if (!modeLongPressHandled && (millis() - modePressStartTime > 2000)) {
      // Đã giữ 2 giây (Long Press)
      if (currentMode == DISPLAY_TIME) {
        isAlarmOn = !isAlarmOn; // Bật/Tắt báo thức
      }
      modeLongPressHandled = true; // Đánh dấu đã xử lý
    }
  } else { // Nút được thả ra
    if (modePressStartTime > 0 && !modeLongPressHandled) {
      // Nhấn ngắn (Short Press)
      if (currentMode == DISPLAY_TIME) {
        currentMode = SET_ALARM_HOUR;
      }
      // Theo mô tả, MODE không dùng để thoát, chỉ SET mới thoát
    }
    modePressStartTime = 0; // Reset
  }

  // 3. Xử lý nút UP (Chỉ nhấn ngắn)
  if (upState && !lastUpState) { // Phát hiện sườn xuống (mới nhấn)
    if (currentMode == SET_ALARM_HOUR) {
      alarmHour = (alarmHour + 1) % 24;
    } else if (currentMode == SET_ALARM_MINUTE) {
      alarmMinute = (alarmMinute + 1) % 60;
    }
  }

  // 4. Xử lý nút DOWN (Chỉ nhấn ngắn)
  if (downState && !lastDownState) { // Phát hiện sườn xuống
    if (currentMode == SET_ALARM_HOUR) {
      alarmHour = (alarmHour == 0) ? 23 : alarmHour - 1;
    } else if (currentMode == SET_ALARM_MINUTE) {
      alarmMinute = (alarmMinute == 0) ? 59 : alarmMinute - 1;
    }
  }

  // 5. Xử lý nút SET (Chỉ nhấn ngắn)
  if (setState && !lastSetState) { // Phát hiện sườn xuống
    if (currentMode == SET_ALARM_HOUR) {
      currentMode = SET_ALARM_MINUTE; // Chuyển sang chỉnh phút
    } else if (currentMode == SET_ALARM_MINUTE) {
      currentMode = DISPLAY_TIME; // Lưu và thoát
    }
  }

  // Lưu trạng thái nút cho vòng lặp tiếp theo
  lastUpState = upState;
  lastDownState = downState;
  lastSetState = setState;
}

/**
 * @brief Kiểm tra xem có đến giờ báo thức không
 */
void checkAlarm() {
  if (isAlarmRinging) {
    // Nếu đang kêu, bật còi (có thể làm nhấp nháy nếu muốn)
    digitalWrite(BUZZER_PIN, HIGH);
    return;
  }

  // Kiểm tra nếu báo thức BẬT và đang ở màn hình chính
  if (isAlarmOn && currentMode == DISPLAY_TIME) {
    DateTime now = rtc.now();
    if (now.hour() == alarmHour && now.minute() == alarmMinute && now.second() == 0) {
      isAlarmRinging = true; // Bắt đầu kêu
    }
  }
}

/**
 * @brief Cập nhật màn hình LCD dựa trên mode hiện tại
 */
void updateDisplay() {
  // Cập nhật trạng thái nhấp nháy
  if (millis() - lastBlinkTime > 500) {
    blinkState = !blinkState;
    lastBlinkTime = millis();
  }

  // Xóa màn hình nếu chuyển mode
  if (currentMode != lastMode) {
    lcd.clear();
    lastMode = currentMode;
  }

  DateTime now = rtc.now();

  switch (currentMode) {
    case DISPLAY_TIME:
      // Hiển thị thời gian
      lcd.setCursor(0, 0);
      lcd.print("Time: ");
      if (now.hour() < 10) lcd.print('0');
      lcd.print(now.hour());
      lcd.print(':');
      if (now.minute() < 10) lcd.print('0');
      lcd.print(now.minute());
      lcd.print(':');
      if (now.second() < 10) lcd.print('0');
      lcd.print(now.second());

      // Hiển thị trạng thái báo thức
      lcd.setCursor(0, 1);
      lcd.print("Alarm: ");
      if (alarmHour < 10) lcd.print('0');
      lcd.print(alarmHour);
      lcd.print(':');
      if (alarmMinute < 10) lcd.print('0');
      lcd.print(alarmMinute);
      lcd.print(isAlarmOn ? " ON " : " OFF");
      break;

    case SET_ALARM_HOUR:
      lcd.setCursor(0, 0);
      lcd.print("Set Alarm Hour");
      lcd.setCursor(0, 1);
      
      // Hiệu ứng nhấp nháy cho giờ
      if (blinkState) {
        lcd.print("  "); // Tương đương 2 ký tự
      } else {
        if (alarmHour < 10) lcd.print('0');
        lcd.print(alarmHour);
      }
      lcd.print(":");
      if (alarmMinute < 10) lcd.print('0');
      lcd.print(alarmMinute);
      lcd.print("        "); // Xóa phần còn lại của dòng
      break;

    case SET_ALARM_MINUTE:
      lcd.setCursor(0, 0);
      lcd.print("Set Alarm Min");
      lcd.setCursor(0, 1);
      if (alarmHour < 10) lcd.print('0');
      lcd.print(alarmHour);
      lcd.print(":");

      // Hiệu ứng nhấp nháy cho phút
      if (blinkState) {
        lcd.print("  ");
      } else {
        if (alarmMinute < 10) lcd.print('0');
        lcd.print(alarmMinute);
      }
      lcd.print("        "); // Xóa phần còn lại của dòng
      break;
  }
}