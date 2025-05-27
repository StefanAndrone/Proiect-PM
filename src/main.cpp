#include <BluetoothSerial.h>
#include <LiquidCrystal.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

LiquidCrystal lcd(14, 27, 26, 25, 33, 32);
BluetoothSerial SerialBT;

const int tempPin = 34;
const int relayPin = 13;
const int buzzerPin = 4;

unsigned long currentTimeInSeconds = 0;
unsigned long lastUpdateMillis = 0;
int set_at_least_once = 0;
bool heatingEnabled = true;

float targetTemp = 22.0;

unsigned long alarmTimeInSeconds = 0;
bool alarmActive = false;
bool buzzing = false;
unsigned long buzzStartTime = 0;

esp_adc_cal_characteristics_t adc_chars;

unsigned long parseTimeToSeconds(const String &timeStr) {
    int h = timeStr.substring(0, 2).toInt();
    int m = timeStr.substring(3, 5).toInt();
    int s = timeStr.substring(6, 8).toInt();
    return h * 3600 + m * 60 + s;
}

float readTemperatureC() {
    int analogValue = adc1_get_raw(ADC1_CHANNEL_6);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(analogValue, &adc_chars);
    float temperatureC = voltage / 10.0;
    return temperatureC;
}

String formatTime(unsigned long seconds) {
    int h = (seconds / 3600) % 24;
    int m = (seconds / 60) % 60;
    int s = seconds % 60;

    char buffer[9];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", h, m, s);
    return String(buffer);
}

void showTimeAndTemp(float temp) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(formatTime(currentTimeInSeconds));

    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.print(temp, 1);
    lcd.print((char)223);
    lcd.print("C");
}

void setup() {
    Serial.begin(115200);
    SerialBT.begin("ESP32-Clock");
    lcd.begin(16, 2);

    pinMode(relayPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW); // ensure buzzer is off
    digitalWrite(relayPin, LOW);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Time undefined");

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);

    float temp = readTemperatureC();
    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.print(temp, 1);
    lcd.print((char)223);
    lcd.print("C");
}

void loop() {
    if (millis() - lastUpdateMillis >= 1000) {
        lastUpdateMillis = millis();
        currentTimeInSeconds++;

        float currentTemp = readTemperatureC();

        if (heatingEnabled) {
            if (currentTemp < targetTemp - 0.5) {
                digitalWrite(relayPin, HIGH);
                Serial.println("Relay ON: Heating");
            } else if (currentTemp >= targetTemp - 0.5 && currentTemp <= targetTemp + 0.5) {
                Serial.println("Target temperature reached");
                digitalWrite(relayPin, LOW);
            } else {
                digitalWrite(relayPin, LOW);
                Serial.println("Relay OFF");
            }
        } else {
            digitalWrite(relayPin, LOW);
        }

        if (set_at_least_once == 1)
            showTimeAndTemp(currentTemp);
        else {
            lcd.setCursor(0, 0);
            lcd.print("Time undefined   ");
            lcd.setCursor(0, 1);
            lcd.print("Temp: ");
            lcd.print(currentTemp, 1);
            lcd.print((char)223);
            lcd.print("C   ");
        }

        // Alarm check
        if (alarmActive && currentTimeInSeconds == alarmTimeInSeconds) {
            buzzing = true;
            buzzStartTime = millis();
            digitalWrite(buzzerPin, HIGH); // Turn on active buzzer
            SerialBT.println("Alarm triggered! Buzzing...");
        }
    }

    // Handle buzzer duration
    if (buzzing && millis() - buzzStartTime >= 5000) {
        buzzing = false;
        digitalWrite(buzzerPin, LOW); // Turn off buzzer
        alarmActive = false; // Clear alarm after it rings once
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
            } else {
                SerialBT.println("Invalid format. Use SET TIME HH:MM:SS");
            }

        } else if (command.startsWith("SET TEMPERATURE")) {
            String tempStr = command.substring(15);
            tempStr.trim();
            float newTarget = tempStr.toFloat();
            if (newTarget >= 0 && newTarget <= 100) {
                targetTemp = newTarget;
                SerialBT.println("Target temp set to: " + String(targetTemp, 1) + "C");
            } else {
                SerialBT.println("Invalid temperature. Use a value between 0 and 100.");
            }

        } else if (command.equalsIgnoreCase("STOP")) {
            heatingEnabled = false;
            digitalWrite(relayPin, LOW);
            SerialBT.println("Heating stopped. Relay disabled.");

        } else if (command.equalsIgnoreCase("START")) {
            heatingEnabled = true;
            SerialBT.println("Heating enabled.");

        } else if (command.startsWith("SET ALARM")) {
            String timeStr = command.substring(10);
            if (timeStr.length() == 8 && timeStr.charAt(2) == ':' && timeStr.charAt(5) == ':') {
                alarmTimeInSeconds = parseTimeToSeconds(timeStr);
                alarmActive = true;
                SerialBT.println("Alarm set for: " + formatTime(alarmTimeInSeconds));
            } else {
                SerialBT.println("Invalid format. Use SET ALARM HH:MM:SS");
            }

        } else {
            SerialBT.println("Unknown command.");
        }
    }
}
