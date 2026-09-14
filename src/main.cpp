#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <SSD1306Wire.h>
#include "version.h"
#include "log.h"
#include "cli.h"
#include "config.h"
#include "post.h"
#include "protocol.h"

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

// ---------- Battery ADC ----------
#define BATTERY_ADC_PIN 35 // on-board battery voltage divider (ADC1, Wi-Fi safe)

enum class ClickType : uint8_t { SINGLE, DOUBLE };

struct ButtonEvent {
  ClickType type;
  uint32_t pressTimestampMs; // millis() at the moment of the (first) press, used for the ⭐ latency measurement
};

struct RadioMessage {
  uint8_t payload; // Protocol::PAYLOAD_SINGLE or PAYLOAD_DOUBLE
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

      Protocol::Frame frame = Protocol::buildButtonEvent(message.payload, Post::mask());
      const char *label = (message.payload == Protocol::PAYLOAD_DOUBLE) ? "DOUBLE" : "SINGLE";

      int state = radio.transmit(reinterpret_cast<uint8_t *>(&frame), Protocol::FRAME_SIZE);

      if (state == RADIOLIB_ERR_NONE) {
        LOGI("Sent %s seq=%u latency=%lums", label, frame.seq, (unsigned long)latencyMs);
        Serial.printf("TX frame: %s\n", Protocol::toHex(frame).c_str());

        display.clear();
        display.drawString(0, 0, "Sent: " + String(label));
        display.drawString(0, 16, "seq: " + String(frame.seq));
        display.drawString(0, 40, "Latency: " + String(latencyMs) + " ms");
        display.display();
      } else {
        LOGE("RadioTask transmit failed, code %d", state);
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
    // Read from Config on every check so a runtime `config set` takes effect immediately.
    if (clickCount > 0 && (now - firstClickTime) > Config::get().doubleClickWindowMs) {
      ButtonEvent event;
      event.type = (clickCount >= 2) ? ClickType::DOUBLE : ClickType::SINGLE;
      event.pressTimestampMs = firstPressTimestamp;

      LOGI("ButtonTask detected %s click", event.type == ClickType::DOUBLE ? "DOUBLE" : "SINGLE");

      xQueueSend(buttonEventQueue, &event, 0);
      clickCount = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Log::init(LogLevel::LOG_DEBUG);
  Version::print();
  Config::load();

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
  bool radioInitOk = (state == RADIOLIB_ERR_NONE);
  if (radioInitOk) {
    radio.setOutputPower(Config::get().txPowerDbm);
    LOGI("Radio initialized, txPower=%ddBm", Config::get().txPowerDbm);
  } else {
    LOGE("Radio init failed, code %d", state);
  }

  Post::run(radioInitOk, BUTTON_PIN, BATTERY_ADC_PIN);

  xTaskCreate(buttonTask, "ButtonTask", 4096, nullptr, 2, nullptr);
  xTaskCreate(radioTask, "RadioTask", 4096, nullptr, 1, nullptr);

  LOGI("Ready. Press the external button (GPIO4) once or twice.");

  display.clear();
  display.drawString(0, 0, "Ready.");
  display.drawString(0, 16, "Press the button");
  display.display();
}

// ---------- Main loop ----------
// Waits on buttonEventQueue with a short timeout (not portMAX_DELAY) so the
// CLI still gets polled between button events, decides which packet to send
// based on click type, and hands it off to radioTask.
void loop() {
  Cli::poll();

  ButtonEvent event;

  if (xQueueReceive(buttonEventQueue, &event, pdMS_TO_TICKS(50)) == pdPASS) {
    RadioMessage message;
    message.pressTimestampMs = event.pressTimestampMs;
    message.payload = (event.type == ClickType::SINGLE) ? Protocol::PAYLOAD_SINGLE
                                                        : Protocol::PAYLOAD_DOUBLE;

    LOGI("MainLoop forwarding payload=0x%02X to RadioTask", message.payload);
    xQueueSend(radioMessageQueue, &message, 0);
  }
}
