#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <Servo.h>
#include <Wire.h>
#include "SafeState.h"

// Locking mechanism definitions
#define SERVO_PIN 9
#define SERVO_LOCK_POS   0
#define SERVO_UNLOCK_POS 90
Servo lockServo;

// Initialize LCD
LiquidCrystal_I2C lcd(0x27,16,2);

// Keypad setup
const byte numRows = 4;
const byte numCols = 4;
byte rowPins[numRows] = {5, 4, 3, 2};
byte colPins[numCols] = {A3, A2, A1, A0};
char keypressed;
char keymap[numRows][numCols] =
{
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Mapping keypad
Keypad keypad = Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);

// SafeState stores the secret code in EEPROM
SafeState safeState;

// Write locked state in EEPROM
void lock() {
  lockServo.write(SERVO_LOCK_POS);
  safeState.lock();
}

void unlock() {
  lockServo.write(SERVO_UNLOCK_POS);
}

// Enter password
String inputSecretCode() {
  lcd.setCursor(5, 1);
  lcd.print("[____]");
  lcd.setCursor(6, 1);
  String result = "";
  while (result.length() < 4) {
    char key = keypad.getKey();
    if (key >= '0' && key <= '9') {
      lcd.print('*');
      result += key;
    }
    else if (key == '*') {
      lcd.setCursor(5, 1);
      lcd.print("[____]");
      lcd.setCursor(6, 1);
      result = "";
    }
  }
  return result;
}

// Set new password
bool setNewCode() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter new code:");
  String newCode = inputSecretCode();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Confirm new code");
  String confirmCode = inputSecretCode();

  if (newCode.equals(confirmCode)) {
    safeState.setCode(newCode);
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("New Code Set");
    delay(2000);
    return true;
  } else {
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Code mismatch");
    lcd.setCursor(0, 1);
    lcd.print("Door not locked!");
    delay(2000);
    return false;
  }
}

void showUnlockMessage() {
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Unlocked!");
  delay(1000);
}

void safeUnlockedLogic() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print(" # to lock");

  bool newCodeNeeded = true;
  // No new code needed if there's a code 
  if (safeState.hasCode()) {
    lcd.setCursor(0, 1);
    lcd.print("  A = new code");
    newCodeNeeded = false;
  }

  auto key = keypad.getKey();
  while (key != 'A' && key != '#') {
    key = keypad.getKey();
  }

  bool readyToLock = true;
  // Set new code if there's no code or key A is pressed
  if (key == 'A' || newCodeNeeded) {
    readyToLock = setNewCode();
    safeUnlockedLogic();
    // Stop further execution in the current instance
    return;
  }

  // Ready to lock after new code is set
  if (readyToLock) {
    lcd.clear();
    lock();
  }
}

void safeLockedLogic() {
  lcd.clear();
  lcd.print("  Door Locked!");

  String userCode = inputSecretCode();
  bool unlockedSuccessfully = safeState.unlock(userCode);

  if (unlockedSuccessfully) {
    showUnlockMessage();
    unlock();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Access Denied!");
  }
}

unsigned long lastKeyPressTime = 0;

void lcdRest() {
  char key = keypad.getKey();
  unsigned long currentTime = millis();
  bool lcdOn = true;

  if (!key) {
    // Check if 5 seconds have passed since the last key press
    if (currentTime - lastKeyPressTime >= 5000 && lcdOn) {
      lcd.noDisplay();
      lcd.noBacklight();
      lcdOn = false;
    }
  } else {
    // Key pressed, reset the timer and display the LCD
    lcd.display();
    lcd.backlight();
    lcdOn = true;
    lastKeyPressTime = currentTime;
  }
}

void setup() {
  lcd.begin(16, 2);
  lcd.init();
  lcd.backlight();
  lockServo.attach(SERVO_PIN);
  // Make sure the physical lock is sync with the EEPROM state
  Serial.begin(115200);
  if (safeState.locked()) {
    lock();
  } else {
    unlock();
  }
}

void loop() {
  if (safeState.locked()) {
    safeLockedLogic();
  } else {
    safeUnlockedLogic();
  }
}