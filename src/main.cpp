#include <Arduino.h>
#include <Encoder.h>
#include <U8g2lib.h>
#include <Menu.h>
#include <Adafruit_MCP23017.h>

#define ESP32

#define clampValue(val, lo, hi) if (val > hi) val = hi; if (val < lo) val = lo;
#define maxValue(a, b) ((a > b) ? a : b)
#define minValue(a, b) ((a < b) ? a : b)

U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ A1, /* dc=*/ 21, /* reset=*/ A5);

Encoder encoder_a(15,33);
Encoder encoder_b(32,14);

Adafruit_MCP23017 mcp;

Menu::Engine *engine;

namespace State {
  typedef enum SystemMode_e {
    None      = 0,
    Default   = (1<<0),
    Settings  = (1<<1),
    Edit      = (1<<2)
  } SystemMode;
};

uint8_t systemState = State::Default;
bool lastEncoderAccelerationState = true;
uint8_t previousSystemState = State::None;

// ----------------------------------------------------------------------------

bool menuExit(const Menu::Action_t a) {
  //Encoder.setAccelerationEnabled(lastEncoderAccelerationState);
  systemState = State::Default;
  return true;
}

bool menuDummy(const Menu::Action_t a) {
  return true;
}

bool menuBack(const Menu::Action_t a) {
  if (a == Menu::actionDisplay) {
    engine->navigate(engine->getParent(engine->getParent()));
  }
  return true;
}

// ----------------------------------------------------------------------------

uint8_t menuItemsVisible = 5;
uint8_t menuItemHeight = 12;

void renderMenuItem(const Menu::Item_t *mi, uint8_t pos) {
  //ScopedTimer tm("  render menuitem");

  uint8_t y = pos * menuItemHeight + 2;

  u8g2.setCursor(10, y);

  //tft.drawRect(8, y - 2, 90, menuItemHeight, (engine->currentItem == mi) ? ST7735_RED : ST7735_BLACK);
  //tft.print(engine->getLabel(mi));

  u8g2.drawBox(8, y - 2, 90, menuItemHeight);
  u8g2.print(engine->getLabel(mi));

  // mark items that have children
  if (engine->getChild(mi) != &Menu::NullItem) {
    u8g2.print(" >  ");
  }
}
// ----------------------------------------------------------------------------

// Name, Label, Next, Previous, Parent, Child, Callback
MenuItem(miExit, "", Menu::NullItem, Menu::NullItem, Menu::NullItem, miSettings, menuExit);

MenuItem(miSettings, "Settings", miTest1, Menu::NullItem, miExit, miCalibrateLo, menuDummy);

  MenuItem(miCalibrateLo,  "Calibrate Lo", miCalibrateHi,  Menu::NullItem,       miSettings, Menu::NullItem, menuDummy);
  MenuItem(miCalibrateHi,  "Calibrate Hi", miChannel0, miCalibrateLo,  miSettings, Menu::NullItem, menuDummy);

  MenuItem(miChannel0, "Channel 0", miChannel1, miCalibrateHi, miSettings, miChView0, menuDummy);
    MenuItem(miChView0,  "Ch0:View",  miChScale0,     Menu::NullItem, miChannel0, Menu::NullItem, menuDummy);
    MenuItem(miChScale0, "Ch0:Scale", Menu::NullItem, miChView0,      miChannel0, Menu::NullItem, menuDummy);

  MenuItem(miChannel1, "Channel 1", Menu::NullItem, miChannel0, miSettings, miChView1, menuDummy);
    MenuItem(miChView1,  "Ch1:View",  miChScale1,     Menu::NullItem, miChannel1, Menu::NullItem, menuDummy);
    MenuItem(miChScale1, "Ch1:Scale", miChBack1,      miChView1,      miChannel1, Menu::NullItem, menuDummy);
    MenuItem(miChBack1,  "Back",      Menu::NullItem, miChScale1,     miChannel1, Menu::NullItem, menuBack);

MenuItem(miTest1, "Test 1 Menu", miTest2,        miSettings, miExit, Menu::NullItem, menuDummy);
MenuItem(miTest2, "Test 2 Menu", miTest3,        miTest1,    miExit, Menu::NullItem, menuDummy);
MenuItem(miTest3, "Test 3 Menu", miTest4,        miTest2,    miExit, Menu::NullItem, menuDummy);
MenuItem(miTest4, "Test 4 Menu", miTest5,        miTest3,    miExit, Menu::NullItem, menuDummy);
MenuItem(miTest5, "Test 5 Menu", miTest6,        miTest4,    miExit, Menu::NullItem, menuDummy);
MenuItem(miTest6, "Test 6 Menu", miTest7,        miTest5,    miExit, Menu::NullItem, menuDummy);
MenuItem(miTest7, "Test 7 Menu", miTest8,        miTest6,    miExit, Menu::NullItem, menuDummy);
MenuItem(miTest8, "Test 8 Menu", Menu::NullItem, miTest7,    miExit, Menu::NullItem, menuDummy);

// ----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.print("Starting...");

  mcp.begin(); // use default address 0
  mcp.pinMode(0, INPUT);
  for(int i=0; i<=9; i++)
          mcp.pullUp(i, HIGH);

  u8g2.begin();
  u8g2.setContrast(0);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_inb16_mr); // choose a suitable font
  u8g2.clearBuffer(); // clear the internal memory
  u8g2.setCursor(0, 18);
  u8g2.print("Testing");
  u8g2.sendBuffer(); // transfer internal memory to the display

  engine = new Menu::Engine(&Menu::NullItem);
  menuExit(Menu::actionDisplay); // reset to initial state
}

// ----------------------------------------------------------------------------

int16_t encMovement;
int16_t encAbsolute;
int16_t encLastAbsolute = -1;
bool updateMenu = false;

// ----------------------------------------------------------------------------

byte encoder_a_read(){
        return round(float(encoder_a.read()) / 4.0);
}

byte encoder_b_read(){
        return round(float(encoder_b.read()) / 4.0);
}

void reset_encoders(){
        encoder_a.write(0);
        encoder_b.write(0);
}

void loop() {

  // handle encoder
  encMovement = encoder_a_read();
  if (encMovement) {
    encAbsolute += encMovement;

    if (systemState == State::Settings) {
      engine->navigate((encMovement > 0) ? engine->getNext() : engine->getPrev());
      updateMenu = true;
    }
  }

  if (!mcp.digitalRead(8))
  {
    if (systemState == State::Settings) {
      engine->invoke();
      updateMenu = true;
    }
  }

  if (!mcp.digitalRead(9))
  {
    if (systemState == State::Settings) {
      engine->navigate(engine->getParent());
      updateMenu = true;
    }

    if (systemState == State::Default) {
        //Encoder.setAccelerationEnabled(!Encoder.getAccelerationEnabled());
        //tft.setTextSize(1);
        //tft.setCursor(10, 42);
        //tft.print("Acceleration: ");
        //tft.print((Encoder.getAccelerationEnabled()) ? "on " : "off");
    }
  }

  // handle button
  /*
  switch (Encoder.getButton()) {

    case ClickEncoder::Clicked:
      if (systemState == State::Settings) {
        engine->invoke();
        updateMenu = true;
      }
      break;

    case ClickEncoder::DoubleClicked:
      if (systemState == State::Settings) {
        engine->navigate(engine->getParent());
        updateMenu = true;
      }

      if (systemState == State::Default) {
          Encoder.setAccelerationEnabled(!Encoder.getAccelerationEnabled());
          tft.setTextSize(1);
          tft.setCursor(10, 42);
          tft.print("Acceleration: ");
          tft.print((Encoder.getAccelerationEnabled()) ? "on " : "off");
      }
      break;

    case ClickEncoder::Held:
      if (systemState != State::Settings) { // enter settings menu

        // disable acceleration, reset in menuExit()
        lastEncoderAccelerationState = Encoder.getAccelerationEnabled();
        Encoder.setAccelerationEnabled(false);

        tft.fillScreen(ST7735_BLACK);
        tft.setTextColor(ST7735_WHITE, ST7735_BLACK);

        engine->navigate(&miSettings);

        systemState = State::Settings;
        previousSystemState = systemState;
        updateMenu = true;
      }
      break;
  }
  */

  if (updateMenu) {
    updateMenu = false;

    if (!encMovement) { // clear menu on child/parent navigation
      u8g2.clearBuffer(); // clear the internal memory
      u8g2.sendBuffer(); // transfer internal memory to the display
    }

    // simple scrollbar
    Menu::Info_t mi = engine->getItemInfo(engine->currentItem);
    uint8_t sbTop = 0, sbWidth = 4, sbLeft = 100;
    uint8_t sbItems = minValue(menuItemsVisible, mi.siblings);
    uint8_t sbHeight = sbItems * menuItemHeight;
    uint8_t sbMarkHeight = sbHeight * sbItems / mi.siblings;
    uint8_t sbMarkTop = ((sbHeight - sbMarkHeight) / mi.siblings) * (mi.position -1);
    u8g2.drawBox(sbLeft, sbTop,     sbWidth, sbHeight);
    u8g2.drawBox(sbLeft, sbMarkTop, sbWidth, sbMarkHeight);
    //tft.fillRect(sbLeft, sbTop,     sbWidth, sbHeight,     ST7735_WHITE);
    //tft.fillRect(sbLeft, sbMarkTop, sbWidth, sbMarkHeight, ST7735_RED);

    // debug scrollbar values
#if 0
    char buf[30];
    sprintf(buf, "itms: %d, h: %d, mh: %d, mt: %d", sbItems, sbHeight, sbMarkHeight, sbMarkTop);
    Serial.println(buf);
#endif

    // render the menu
    {
      //ScopedTimer tm("render menu");
      engine->render(renderMenuItem, menuItemsVisible);
    }
/*
    {
      ScopedTimer tm("helptext");
      tft.setTextSize(1);
      tft.setCursor(10, 110);
      tft.print("Doubleclick to ");
      if (engine->getParent() == &miExit) {
        tft.print("exit. ");
      }
      else {
        tft.print("go up.");
      }
    }
    */
  }

  // dummy "application"
  if (systemState == State::Default) {
    if (systemState != previousSystemState) {
      previousSystemState = systemState;
      encLastAbsolute = -999; // force updateMenu
      //tft.fillScreen(ST7735_WHITE);
      //tft.setTextColor(ST7735_BLACK, ST7735_WHITE);
      u8g2.setCursor(0, 18);
      u8g2.print("Main");
      u8g2.setCursor(0, 40);
      u8g2.print("Hold button for setup");
    }

    if (encAbsolute != encLastAbsolute) {
      encLastAbsolute = encAbsolute;
      u8g2.setCursor(10, 30);
      u8g2.print("Position:");
      //tft.setCursor(10, 30);
      //tft.setTextSize(1);
      //tft.print("Position:");
      u8g2.setCursor(70, 30);
      char tmp[10];
      sprintf(tmp, "%4d", encAbsolute);
      u8g2.print("Position:");
    }
    u8g2.sendBuffer(); // transfer internal memory to the display
  }
}
