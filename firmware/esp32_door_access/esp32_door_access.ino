#include <SPI.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>
#include <ESP32Servo.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include "DHT.h"
#include "BluetoothSerial.h" // Thư viện Bluetooth Classic ESP32

// --- THIẾT LẬP BLUETOOTH ---
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run make menuconfig to enable it
#endif

BluetoothSerial SerialBT;

// --- THIẾT LẬP LED RGB ---
const int RED_PIN   = 26;
const int GREEN_PIN = 25;
const int BLUE_PIN  = 12; 

// --- THIẾT LẬP CẢM BIẾN DHT11 ---
#define DHTPIN 27        // Chân OUT DHT11 nối GPIO 27
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

unsigned long lastDHTReadTime = 0;
const long DHT_INTERVAL = 2000; // Đọc DHT11 mỗi 2s
float currentTemp = 0.0;
float currentHum = 0.0;

// --- THIẾT LẬP SERVO & LCD ---
Servo doorServo;
const int SERVO_PIN = 13;
LiquidCrystal_I2C lcd(0x27, 20, 4);

// --- THIẾT LẬP RFID MFRC522 ---
MFRC522DriverPinSimple ss_pin(5);
MFRC522DriverSPI driver{ss_pin};
MFRC522 mfrc522{driver};

const String VALID_UID = "a358d4a6"; // UID thẻ hợp lệ

// --- THIẾT LẬP KEYPAD 4x4 ---
#define ROW_NUM     4
#define COLUMN_NUM  4

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte pin_rows[ROW_NUM] = {32, 33, 14, 16};
byte pin_column[COLUMN_NUM] = {17, 15, 2, 0};

Keypad keypad = Keypad(
  makeKeymap(keys),
  pin_rows,
  pin_column,
  ROW_NUM,
  COLUMN_NUM
);

// --- FLASH & TRẠNG THÁI ---
Preferences preferences;

String VALID_PASSWORD = "0953"; // Mật khẩu mặc định
String input_password = "";

enum SystemMode {
  MODE_NORMAL,         // Chế độ chờ
  MODE_ENTER_OLD_PASS, // Đổi MK - Bước 1
  MODE_NEW_PASS_INPUT  // Đổi MK - Bước 2
};

SystemMode currentMode = MODE_NORMAL;

// Khai báo hàm
void resetDisplay();
void unlockDoor();
void denyAccess();
void checkRFID();
void checkKeypad();
void updateDHT();
void displayDHTOnLCD();
void checkBluetooth();
void setRGBColor(bool red, bool green, bool blue);

void setup() {
  Serial.begin(115200);
  input_password.reserve(32);

  // Khởi tạo LED RGB
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  setRGBColor(false, false, false); // Tắt LED ban đầu

  // Khởi tạo Bluetooth với tên "ESP32_CuaThongMinh"
  SerialBT.begin("ESP32_CuaThongMinh");
  Serial.println("Bluetooth dang san sang! Hay ket noi voi 'ESP32_CuaThongMinh'");

  // Khởi tạo DHT11
  dht.begin();

  // Đọc mật khẩu từ bộ nhớ Flash
  preferences.begin("door_lock", false);
  VALID_PASSWORD = preferences.getString("pass", "0953");

  // Khởi tạo RFID
  mfrc522.PCD_Init();
  MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial);

  // Khởi tạo Servo
  doorServo.attach(SERVO_PIN);
  doorServo.write(180);

  // Khởi tạo LCD
  lcd.init();
  lcd.backlight();
  
  resetDisplay();
}

void loop() {
  updateDHT();      // Cập nhật DHT11
  checkBluetooth(); // Kiểm tra lệnh điều khiển LED qua Bluetooth
  checkRFID();      // Quét thẻ RFID
  checkKeypad();    // Kiểm tra bàn phím Keypad
}

// --- HÀM XỬ LÝ BLUETOOTH DIỀU KHIỂN LED RGB ---
void checkBluetooth() {
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    command.trim(); // Xóa khoảng trắng / ký tự xuống dòng
    //command.toUpperCase(); // Chuyển thành chữ hoa

    Serial.print("Nhan lenh Bluetooth: ");
    Serial.println(command);

    if (command == "r") {
      setRGBColor(true, false, false);
      SerialBT.println("LED: MAU DO");
    } 
    else if (command == "g") {
      setRGBColor(false, true, false);
      SerialBT.println("LED: MAU XANH LA");
    } 
    else if (command == "p") { 
    setRGBColor(true, false, true); // MAU TIM (Red + Blue) 
    SerialBT.println("LED: MAU TIM"); 
    }
    else if (command == "b") {
      setRGBColor(false, false, true);
      SerialBT.println("LED: MAU XANH DUONG");
    } 
    else if (command == "w") {
      setRGBColor(true, true, true);
      SerialBT.println("LED: MAU TRANG");
    } 
    else if (command == "off") {
      setRGBColor(false, false, false);
      SerialBT.println("LED: TAT");
    } 
    else {
      SerialBT.println("Lenh khong hop le!");
    }
  }
}

// --- HÀM BẬT/TẮT MÀU LED RGB ---
void setRGBColor(bool red, bool green, bool blue) {
  digitalWrite(RED_PIN, red ? HIGH : LOW);
  digitalWrite(GREEN_PIN, green ? HIGH : LOW);
  digitalWrite(BLUE_PIN, blue ? HIGH : LOW);
}

// --- HÀM CẬP NHẬT DHT11 ---
void updateDHT() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastDHTReadTime >= DHT_INTERVAL) {
    lastDHTReadTime = currentMillis;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      currentHum = h;
      currentTemp = t;
      displayDHTOnLCD();
    }
  }
}

// --- HIỂN THỊ DHT11 LÊN LCD ---
void displayDHTOnLCD() {
  if (currentMode == MODE_NORMAL) {
    lcd.setCursor(0, 3);
    lcd.print("T: ");
    lcd.print((int)currentTemp);
    lcd.print((char)223);
    lcd.print("C  |  H: ");
    lcd.print((int)currentHum);
    lcd.print("%   ");
  }
}

// --- HÀM XỬ LÝ RFID ---
void checkRFID() {
  if (currentMode != MODE_NORMAL) return;

  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      uidString += "0"; 
    }
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }

  if (uidString == VALID_UID) {
    unlockDoor();
  } else {
    denyAccess();
  }
}

// --- HÀM XỬ LÝ KEYPAD ---
void checkKeypad() {
  char key = keypad.getKey();
  if (!key) return;

  if (key == 'A' && currentMode == MODE_NORMAL) {
    currentMode = MODE_ENTER_OLD_PASS;
    input_password = "";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Nhap MK hien tai:");
    return;
  }

  if (key == '*') {
    input_password = "";
    currentMode = MODE_NORMAL;
    resetDisplay();
    return;
  }

  if (key == '#') {
    if (currentMode == MODE_NORMAL) {
      if (input_password == VALID_PASSWORD) {
        unlockDoor();
      } else {
        denyAccess();
      }
    } 
    else if (currentMode == MODE_ENTER_OLD_PASS) {
      if (input_password == VALID_PASSWORD) {
        currentMode = MODE_NEW_PASS_INPUT;
        input_password = "";
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Nhap MK MOI:");
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("MK cu khong dung!");
        delay(2000);
        currentMode = MODE_NORMAL;
        input_password = "";
        resetDisplay();
      }
    } 
    else if (currentMode == MODE_NEW_PASS_INPUT) {
      if (input_password.length() > 0) {
        VALID_PASSWORD = input_password;
        preferences.putString("pass", VALID_PASSWORD);
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Doi MK thanh cong!");
        delay(2000);
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("MK khong hop le!");
        delay(2000);
      }
      currentMode = MODE_NORMAL;
      input_password = "";
      resetDisplay();
    }
    return;
  }

  input_password += key;
  lcd.setCursor(0, 1);
  lcd.print("                    ");
  lcd.setCursor(0, 1);

  for (int i = 0; i < input_password.length(); i++) {
    lcd.print("*");
  }
}

// --- HÀM MỞ CỬA ---
void unlockDoor() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Moi vao");
  lcd.setCursor(0, 1);
  lcd.print("Cua dang mo");

  doorServo.write(90);
  delay(5000);
  doorServo.write(180);

  input_password = "";
  resetDisplay();
}

// --- HÀM TỪ CHỐI TRUY CẬP ---
void denyAccess() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Vui long thu lai");
  delay(3000);

  input_password = "";
  resetDisplay();
}

// --- HÀM LÀM MỚI MÀN HÌNH ---
void resetDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Quet the/Nhap MK:");
  displayDHTOnLCD();
}
