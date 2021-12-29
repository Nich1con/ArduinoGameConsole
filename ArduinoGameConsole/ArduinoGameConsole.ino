
/* Пины кнопок */
#define WAKEUP_PRESS  500
#define BUTTON_OK     A4
#define BUTTON_UP     A1
#define BUTTON_DOWN   A3
#define BUTTON_LEFT   A0
#define BUTTON_RIGHT  A2

/* Пины дисплея */
#define OLED_VCC1      6
#define OLED_VCC0      7
#define OLED_RST       8
#define OLED_DC        9
#define OLED_CS        10
#define OLED_SPI_SPEED 4000000ul

/* Параметры источника питания */
#define INTERNAL_REF  1100
#define BATTERY_FULL  3200
#define BATTERY_EMPTY 2000
#define SLEEP_TIMEOUT 10000 // Таймаут, после которого устройство переходит в сон, если не трогать кнопки (в мс)

/* Параметры EEPROM */
#define EEPROM_KEY      0xB1  // Ключ EEPROM
#define KEY_EE_ADDR     0     // Адрес ключа в EEPROM
#define BRIGHT_EE_ADDR  1     // Адрес яркости дисплея в EEPROM
#define DINO_EE_ADDR    2     // Адрес рекорда для игры "Dinosaur game" 

/* Параметры меню */
#define MENU_FRAMERATE  30    // Частота кадров в меню (FPS)
#define APPS_AMOUNT     1     // Количество игр в меню

/* Библиотеки */
#include <SPI.h>
#include <EEPROM.h>
#include <GyverButton.h>
#include <GyverPower.h>
#include <GyverOLED.h>
#include <util/delay.h>

/* Обьекты */
GyverOLED <SSD1306_128x64, OLED_BUFFER, OLED_SPI, OLED_CS, OLED_DC, OLED_RST> oled;
GButton ok(BUTTON_OK);
GButton up(BUTTON_UP);
GButton down(BUTTON_DOWN);
GButton left(BUTTON_LEFT);
GButton right(BUTTON_RIGHT);

uint32_t globalSleepTimer = 0;

void setup() {
  power.hardwareDisable(PWR_TIMER1 | PWR_TIMER2 | PWR_I2C | PWR_UART0); // Выключаем лишнее
  power.setSleepMode(POWERDOWN_SLEEP);                                  // Спать будем глубоко
  power.bodInSleep(false);                                              // И без BOD'a

  if (EEPROM[KEY_EE_ADDR] != EEPROM_KEY) {    // Проверка EEPROM на первое включение
    EEPROM[KEY_EE_ADDR] = EEPROM_KEY;         // При первом включении устанавливаем все как надо
    EEPROM[BRIGHT_EE_ADDR] = 100;
    EEPROM[DINO_EE_ADDR] = 0;
    EEPROM[DINO_EE_ADDR + 1] = 0;
  }

  oledPower(true);              // Включаем и инициализируем дисплей
  oled.clear();                 // Сразу очищаем его

  ok.setTickMode(AUTO);         // Настраиваем все кнопки на авто-опрос
  up.setTickMode(AUTO);
  down.setTickMode(AUTO);
  left.setTickMode(AUTO);
  right.setTickMode(AUTO);

  left.setStepTimeout(100);     // Настраиваем таймауты удержания
  right.setStepTimeout(100);

  /* Настриаваем прерывания по всем кнопкам - чтобы отслеживать нажатия */
  PCICR = 1 << PCIE1;                                                               // Включаем прерывание по действию всех кнопок
  PCMSK1 = 1 << PCINT8 | 1 << PCINT9 | 1 << PCINT10 | 1 << PCINT11 | 1 << PCINT12;  // Активируем на всех пинах, где есть кнопки
  globalSleepTimer = millis();                                                      // Сброс глобального таймера сна

  /* Настриаваем АЦП для измерения напряжения питания */
  ADMUX = DEFAULT << 6 | 0b1110;      // Опорное - AVCC, вход АЦП к внутреннему опорному
  ADCSRA = 1 << ADEN | 0b101;         // Вкл. АЦП + средн. скорость АЦП
  for (uint8_t i = 0; i < 8; i++) {   // Несколько ложных преобразований - отфильтровать мусор
    ADCSRA |= 1 << ADSC;              // Запускаем преобразование
    while (ADCSRA & (1 << ADSC));     // Ждем окончания
  }

}

void loop() {
  static uint8_t menuPtr = 2;  // Положение указателя меню

  /* Обработка кнопок в главном меню */
  if (left.isClick() or left.isStep()) {  // Влево - уменьшить и сохранить яркость
    EEPROM[BRIGHT_EE_ADDR] = constrain(EEPROM[BRIGHT_EE_ADDR] - 5, 5, 100); // Уменьшаем значение в EEPROM [5-100%]
    oled.setContrast(map(EEPROM[BRIGHT_EE_ADDR], 0, 100, 0, 255));          // Устанавливаем яркость дисплея
  }

  if (right.isClick() or right.isStep()) {  // Вправо - увеличить и сохранить яркость
    EEPROM[BRIGHT_EE_ADDR] = constrain(EEPROM[BRIGHT_EE_ADDR] + 5, 5, 100); // Увеличиваем значение в EEPROM [5-100%]
    oled.setContrast(map(EEPROM[BRIGHT_EE_ADDR], 0, 100, 0, 255));          // Устанавливаем яркость дисплея
  }

  if (up.isClick()) {                       // Вверх - выбрать пункт выше
    menuPtr = constrain(menuPtr - 1, 2, APPS_AMOUNT + 1); // Двигаем указатель в пределах меню
  }

  if (down.isClick()) {                     // Вниз - выбрать пункт ниже
    menuPtr = constrain(menuPtr + 1, 2, APPS_AMOUNT + 1); // Двигаем указатель в пределах меню
  }

  if (ok.isClick()) {                       // Ок - перейти в приложение
    switch (menuPtr) {                      // В зависимости от пункта меню
      case 2: DinosaurGame(); break;        // Вызываем нужное
      case 3: break;
      case 4: break;
      case 5: break;
      case 6: break;
      case 7: break;
    }
  }

  /* Отрисовка главного меню */
  static uint32_t drawTimer = millis();
  if (millis() - drawTimer >= (1000 / MENU_FRAMERATE)) {        // По таймеру на миллис
    drawTimer = millis();
    oled.clear();                                               // Чистим дисплей
    oled.setCursor(24, 2); oled.print(F("DINOSAUR GAME"));      // Выводим название приложений
    // oled.setCursor(24, 3); oled.print(F("NEW GAME NAME"));   // пустые заготовки
    // oled.setCursor(24, 4); oled.print(F("NEW GAME NAME"));
    // oled.setCursor(24, 5); oled.print(F("NEW GAME NAME"));
    // oled.setCursor(24, 6); oled.print(F("NEW GAME NAME"));
    // oled.setCursor(24, 7); oled.print(F("NEW GAME NAME"));

    oled.setCursor(0, menuPtr); oled.print('>');                // Выводим левый указатель
    oled.setCursor(122, menuPtr); oled.print('<');              // Выводим правый указатель
    oled.home(); oled.print(F("BRIGHT: "));                     // Выводим яркость
    oled.print(EEPROM[BRIGHT_EE_ADDR]); oled.print(" % ");      // Из EEPROM
    batCheckDraw();                                             // Проверка и отрисовка заряда
    oled.update();                                              // Выводим изображение на дисплей
  }

  if (millis() - globalSleepTimer > SLEEP_TIMEOUT) {            // Проверка глобального таймера сна
    goToSleep();                                                // Если кнопки долго не нажимались - идем спать
  }
}

/* ----------------------------------------------------- Сервисные функции ----------------------------------------------------- */

/* Это прерывание вызывается при ЛЮБОМ действии ЛЮБОЙ кнопки */
ISR(PCINT1_vect) {
  globalSleepTimer = millis();  // Обновляем глобальный таймер нажатий
}

/* Уход в сон и возврат из сна */
void goToSleep(void) {
  bool wakeup;                                // Флаг пробуждения
  uint32_t timer;                             // Таймер
  oledPower(false);                           // Выключаем олед
  PCMSK1 = 1 << PCINT12;                      // Прерывание и пробуждение только по "OK"
  while (true) {                              // Бесконечный цикл
    power.sleep(SLEEP_FOREVER);               // << Уходим в сон
    bool wakeup = false;                      // >> проснулись, сбросили флаг
    uint32_t timer = millis();                // Обновили таймер
    while (ok.state()) {                      // Пока нажата кнопка
      if (millis() - timer > WAKEUP_PRESS) {  // Если кнопка нажата дольше указанного - можем просыпаться
        wakeup = true;                        // Ставим флаг
      }
    } if (wakeup) break;                      // Как только кнопка отпущена, смотрим - если флаг стоит, просыпаемся
  }
  PCMSK1 = 1 << PCINT8 | 1 << PCINT9 | 1 << PCINT10 | 1 << PCINT11 | 1 << PCINT12;  // Возвращаем обратно прерывание по всем кнопкам
  oledPower(true);                            // Подрубаем олед заного
}

/* Тестирование батареи и вывод заряда на экран */
void batCheckDraw(void) {
  static uint32_t measureTimer = millis() + 3500;  // Таймер АЦП (Стартует сразу)
  static uint8_t batCharge = 0;                    // "Заряд" батареи

  if (millis() - measureTimer >= 3000) {
    measureTimer = millis();
    /* Измеряем напряжение питания + усредняем */
    ADCSRA |= 1 << ADSC;                // Запускаем преобразование
    while (ADCSRA & (1 << ADSC));       // Ждем
    /* Пересчитываем напряжение в условный заряд */
    batCharge = constrain(map((INTERNAL_REF * 1024UL) / ADC, BATTERY_EMPTY, BATTERY_FULL, 0, 12), 0, 12);
  }

  /* Рисуем батарейку */
  oled.setCursorXY(110, 0);                             // Положение на экране
  oled.drawByte(0b00111100);                            // Пипка
  oled.drawByte(0b00111100);                            // 2 штуки
  oled.drawByte(0b11111111);                            // Передняя стенка
  for (uint8_t i = 0; i < 12; i++) {                    // 12 градаций
    if (i < 12 - batCharge)oled.drawByte(0b10000001);   // Рисуем пустые
    else oled.drawByte(0b11111111);                     // Рисуем полные
  } oled.drawByte(0b11111111);                          // Задняя стенка
}

/* Включение и выключения OLED */
void oledPower(bool state) {                                          // Включение / выключение оледа
  if (state) {                                                        // Включаем
    pinMode(OLED_VCC1, OUTPUT);                                       // Питающие пины как выходы
    pinMode(OLED_VCC0, OUTPUT);                                       // Питающие пины как выходы
    digitalWrite(OLED_VCC1, HIGH);                                    // Питающие пины в HIGH
    digitalWrite(OLED_VCC0, HIGH);                                    // Питающие пины в HIGH
    _delay_ms(15);                                                    // Даем время дисплею оклематься
    oled.init();                                                      // Инициализируем дисплей
    oled.setContrast(map(EEPROM[BRIGHT_EE_ADDR], 0, 100, 0, 255));    // Восстанавливаем яркость из EEPROM
  } else {                                                            // Выключаем
    for (uint8_t i = EEPROM[BRIGHT_EE_ADDR]; i; i--) {                // Плавно от установленной яркости
      oled.setContrast(i);                                            // Гасим дисплей
      _delay_ms(10);                                                  // С задержкой для плавности
    }
    oled.setPower(false);                                             // Выключаем программно
    digitalWrite(OLED_VCC1, LOW);                                     // Питающие пины в LOW
    digitalWrite(OLED_VCC0, LOW);                                     // Питающие пины в LOW
    pinMode(OLED_VCC1, INPUT);                                        // Питающие пины как входы
    pinMode(OLED_VCC0, INPUT);                                        // Питающие пины как входы
  }
}
