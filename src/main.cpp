#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <SSD1306Wire.h>

// ---------- LoRa (SX1276) pins on TTGO LoRa32 V2.1 ----------
#define LORA_SCK   5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_CS    18
#define LORA_RST   23
#define LORA_DIO0  26

// ---------- OLED (SSD1306) ----------
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDRESS 0x3C

// ---------- Button ----------
#define BUTTON_PIN 4 // external SWT-9 button on breadboard: GPIO4 <-> GND, active LOW via INPUT_PULLUP
#define DEBOUNCE_MS 30
#define DOUBLE_CLICK_WINDOW_MS 300

enum class ClickType : uint8_t { SINGLE, DOUBLE };

struct ButtonEvent {
  ClickType type;
  uint32_t pressTimestampMs; // millis() at the moment of the (first) press, used for the ⭐ latency measurement
};

struct RadioMessage {
  char text[32];
  uint32_t pressTimestampMs;
};

static QueueHandle_t buttonEventQueue;
static QueueHandle_t radioMessageQueue;

SPIClass loraSPI(HSPI);
Module radioModule(LORA_CS, LORA_DIO0, LORA_RST);
SX1276 radio(&radioModule);

SSD1306Wire display(OLED_ADDRESS, OLED_SDA, OLED_SCL);

// ---------- Radio Task ----------
// Owns the SX1276 radio and performs the (blocking, tens-of-ms) transmit,
// so button detection never waits on radio hardware.
void radioTask(void *pvParameters) {
  RadioMessage message;

  for (;;) {
    if (xQueueReceive(radioMessageQueue, &message, portMAX_DELAY) == pdPASS) {
      uint32_t latencyMs = millis() - message.pressTimestampMs;

      int state = radio.transmit(message.text);

      if (state == RADIOLIB_ERR_NONE) {
        Serial.printf("[RadioTask] Sent: \"%s\" | press-to-send latency: %lu ms\n",
                      message.text, (unsigned long)latencyMs);

        display.clear();
        display.drawString(0, 0, "Sent:");
        display.drawString(0, 16, message.text);
        display.drawString(0, 40, "Latency: " + String(latencyMs) + " ms");
        display.display();
      } else {
        Serial.printf("[RadioTask] Transmit failed, code %d\n", state);
      }
    }
  }
}

// ---------- Button Task ----------
// Polls the button pin, debounces it, and distinguishes single vs double
// click using a wait-window timer. Sends the result into buttonEventQueue.
void buttonTask(void *pvParameters) {
  bool lastStableState = HIGH;
  uint32_t lastChangeTime = 0;
  uint8_t clickCount = 0;
  uint32_t firstClickTime = 0;
  uint32_t firstPressTimestamp = 0;

  for (;;) {
    bool reading = digitalRead(BUTTON_PIN);
    uint32_t now = millis();

    if (reading != lastStableState && (now - lastChangeTime) > DEBOUNCE_MS) {
      lastChangeTime = now;
      lastStableState = reading;

      if (reading == LOW) { // press detected (active LOW)
        if (clickCount == 0) {
          firstClickTime = now;
          firstPressTimestamp = now;
        }
        clickCount++;
      }
    }

    // Waiting window expired: decide single vs double and emit the event.
    if (clickCount > 0 && (now - firstClickTime) > DOUBLE_CLICK_WINDOW_MS) {
      ButtonEvent event;
      event.type = (clickCount >= 2) ? ClickType::DOUBLE : ClickType::SINGLE;
      event.pressTimestampMs = firstPressTimestamp;

      Serial.printf("[ButtonTask] Detected %s click\n",
                    event.type == ClickType::DOUBLE ? "DOUBLE" : "SINGLE");

      xQueueSend(buttonEventQueue, &event, 0);
      clickCount = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Booting...");
  display.display();

  buttonEventQueue = xQueueCreate(5, sizeof(ButtonEvent));
  radioMessageQueue = xQueueCreate(5, sizeof(RadioMessage));

  loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  int state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[Setup] Radio init failed, code %d\n", state);
    while (true) { delay(1000); }
  }
  Serial.println("[Setup] Radio initialized");

  xTaskCreate(buttonTask, "ButtonTask", 4096, nullptr, 2, nullptr);
  xTaskCreate(radioTask, "RadioTask", 4096, nullptr, 1, nullptr);

  Serial.println("[Setup] Ready. Press the external button (GPIO4) once or twice.");

  display.clear();
  display.drawString(0, 0, "Ready.");
  display.drawString(0, 16, "Press the button");
  display.display();
}

// ---------- Main loop ----------
// Blocks on buttonEventQueue (no polling, no busy-wait), decides which
// packet to send based on click type, and hands it off to radioTask.
void loop() {
  ButtonEvent event;

  if (xQueueReceive(buttonEventQueue, &event, portMAX_DELAY) == pdPASS) {
    RadioMessage message;
    message.pressTimestampMs = event.pressTimestampMs;

    if (event.type == ClickType::SINGLE) {
      strcpy(message.text, "BUTTON SINGLE");
    } else {
      strcpy(message.text, "BUTTON DOUBLE");
    }

    Serial.printf("[MainLoop] Forwarding \"%s\" to RadioTask\n", message.text);
    xQueueSend(radioMessageQueue, &message, 0);
  }
}
