#include <Arduino.h>
#include <BluetoothSerial.h>
#include <LiquidCrystal.h>

LiquidCrystal lcd(14, 27, 26, 25, 33, 32);
BluetoothSerial SerialBT;

unsigned long currentTimeInSeconds = 0;
unsigned long lastUpdateMillis = 0;
int set_at_least_once = 0;

unsigned long parseTimeToSeconds(const String &timeStr) {
    int h = timeStr.substring(0, 2).toInt();
    int m = timeStr.substring(3, 5).toInt();
    int s = timeStr.substring(6, 8).toInt();
    return h * 3600 + m * 60 + s;
}

String formatTime(unsigned long seconds) {
    int h = (seconds / 3600) % 24;
    int m = (seconds / 60) % 60;
    int s = seconds % 60;

    char buffer[9];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", h, m, s);
    return String(buffer);
}

void showTimeOnLCD() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(formatTime(currentTimeInSeconds));
}

void setup() {
    Serial.begin(115200);
    SerialBT.begin("ESP32-Clock");
    lcd.begin(16, 2);

    lcd.clear();                
    lcd.setCursor(0, 0);         
    lcd.print("Time undefined"); 
}

void loop() {
    if (millis() - lastUpdateMillis >= 1000) {
        lastUpdateMillis = millis();
        currentTimeInSeconds++;
        if(set_at_least_once == 1)
           showTimeOnLCD();
    }

    if (SerialBT.available()) {
        String command = SerialBT.readStringUntil('\n');
        command.trim();

        if (command.startsWith("SET TIME")) {
            String timeStr = command.substring(9);
            if (timeStr.length() == 8 && timeStr.charAt(2) == ':' && timeStr.charAt(5) == ':') {
                currentTimeInSeconds = parseTimeToSeconds(timeStr);
                SerialBT.println("Time set to: " + formatTime(currentTimeInSeconds));
                set_at_least_once = 1;
                showTimeOnLCD();
            } else {
                SerialBT.println("Invalid format. Use SET TIME HH:MM:SS");
            }
        }
        else {
            SerialBT.println("Unknown command.");
        }
    }
}
