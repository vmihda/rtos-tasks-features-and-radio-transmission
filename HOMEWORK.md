# Домашнє завдання: FreeRTOS tasks + Queue + LoRa

## Архітектура

```
Кнопка (GPIO4) → Button Task → Queue(buttonEventQueue) → Main Loop → Queue(radioMessageQueue) → Radio Task → LoRa TX
```

Плата: LILYGO TTGO LoRa32 V2.1 (ESP32 + LoRa SX1276). Бібліотека радіо: RadioLib.

## Реалізація трьох частин

**Головний цикл (`loop()`)** — блокується на `xQueueReceive(buttonEventQueue, ..., portMAX_DELAY)` без активного очікування (CPU звільняється іншим задачам), приймає тип кліку та вирішує, який пакет надсилати ("BUTTON SINGLE" / "BUTTON DOUBLE"), передає готове повідомлення в другу чергу для Radio Task.

**Button Task** (пріоритет 2) — опитує пін кнопки що 20 мс, робить debounce (30 мс), і визначає single/double через вікно очікування 300 мс: якщо за цей час прийшов лише один клік — SINGLE, якщо два — DOUBLE. Результат кладе в `buttonEventQueue`.

**Radio Task** (пріоритет 1) — блокується на `xQueueReceive(radioMessageQueue, ..., portMAX_DELAY)`, при отриманні повідомлення викликає `radio.transmit()` (RadioLib, SX1276) та виводить результат і затримку на Serial і OLED-екран.

## Queue

Дві черги: `buttonEventQueue` (Button Task → Main Loop) та `radioMessageQueue` (Main Loop → Radio Task). Кожна на 5 елементів, ємність фіксованого розміру структури (`ButtonEvent` / `RadioMessage`) — без динамічної пам'яті, безпечно для FreeRTOS.

## Підтвердження роботи (лог Serial Monitor)

```
[Setup] Radio initialized
[Setup] Ready. Press the external button (GPIO4) once or twice.
[ButtonTask] Detected SINGLE click
[MainLoop] Forwarding "BUTTON SINGLE" to RadioTask
[RadioTask] Sent: "BUTTON SINGLE" | press-to-send latency: 321 ms
[ButtonTask] Detected DOUBLE click
[MainLoop] Forwarding "BUTTON DOUBLE" to RadioTask
[RadioTask] Sent: "BUTTON DOUBLE" | press-to-send latency: 320 ms
```

Одинарне натискання → відправлено "BUTTON SINGLE". Подвійне натискання → відправлено "BUTTON DOUBLE". Обидва сценарії підтверджені реальним логом з фізичної плати.

## ⭐ Завдання із зіркою: вимірювання затримки

Мітка часу (`millis()`) фіксується в Button Task у момент першого натискання і передається разом з подією через обидві черги аж до Radio Task, де прямо перед `radio.transmit()` рахується різниця `millis() - pressTimestampMs`.

**Отримане значення:** ~320-321 мс для обох типів кліку.

**Де виникає затримка:** основна частина (300 мс) — це навмисне вікно очікування в Button Task, потрібне, щоб відрізнити одинарний клік від подвійного (треба почекати, чи не прийде другий клік). Решта (~20 мс) — це період опитування кнопки (`vTaskDelay(20ms)` в циклі Button Task) плюс мінімальний час на передачу через дві черги та FreeRTOS-планування, що є пренебрежимо малим порівняно з вікном очікування.
