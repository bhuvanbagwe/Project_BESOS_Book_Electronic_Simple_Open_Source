#include <GxEPD2_BW.h>
#include <OneButton.h>
#include <Preferences.h>
#include <SPI.h>
#include "Book.h"

// --- PIN DEFINITIONS (ESP32-C3 Super Mini) ---
#define EPD_SCK   6
#define EPD_MOSI  7
#define EPD_MISO  -1 
#define EPD_CS    10
#define EPD_DC    2
#define EPD_RST   1
#define EPD_BUSY  5
#define BTN_PIN   9  

// Initialize 4.2" Display (Landscape: Width > Height)
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(
  GxEPD2_420_GDEY042T81(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

OneButton button(BTN_PIN, true);
Preferences prefs;

// Navigation State
unsigned int currentPointer = 0; 
unsigned int nextPointer = 0;
unsigned int prevPointers[50]; // Stores start positions of previous pages
int historyIndex = 0;
int pageCounter = 0; 

void saveProgress() {
  prefs.begin("ereader", false);
  prefs.putUInt("lastPos", currentPointer);
  prefs.end();
}

void renderPage(bool fastMode) {
  if (fastMode) {
    display.setPartialWindow(0, 0, display.width(), display.height());
  } else {
    display.setFullWindow();
  }

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    
    // --- TEXT LAYOUT SETTINGS ---
    display.setTextSize(3);     // Large Font
    int16_t x = 8;              // Left Margin
    int16_t y = 28;             // Top Margin
    int16_t maxWidth = display.width() - 8; 
    int16_t lineHeight = 34;    // Spacing for Size 3 font
    
    unsigned int tempPointer = currentPointer; 
    unsigned int bookLength = strlen(myBookContent);

    while (tempPointer < bookLength && y < display.height() - 10) {
      String word = "";
      while (tempPointer < bookLength) {
        char c = pgm_read_byte(&myBookContent[tempPointer]);
        tempPointer++;
        if (c == ' ' || c == '\n') break;
        word += c;
      }

      int16_t tx1, ty1; uint16_t tw, th;
      display.getTextBounds(word, 0, 0, &tx1, &ty1, &tw, &th);

      if (x + tw > maxWidth) {
        x = 8;
        y += lineHeight;
      }

      if (y < display.height() - 10) {
        display.setCursor(x, y);
        display.print(word + " ");
        x += (tw + 12);
      }
      nextPointer = tempPointer;
    }
  } while (display.nextPage());

  display.powerOff(); 
}

void handleNext() {
  if (nextPointer < strlen(myBookContent)) {
    // Store current position in history before moving
    if (historyIndex < 49) {
      prevPointers[historyIndex] = currentPointer;
      historyIndex++;
    }
    
    currentPointer = nextPointer;
    pageCounter++;
    
    if (pageCounter >= 10) {
      renderPage(false); // Clean ghosting every 10 pages
      pageCounter = 0;
    } else {
      renderPage(true); 
    }
    saveProgress();
  }
}

void handleBack() {
  if (historyIndex > 0) {
    historyIndex--;
    currentPointer = prevPointers[historyIndex];
    renderPage(true);
    saveProgress();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  display.epd2.selectSPI(SPI, SPISettings(2000000, MSBFIRST, SPI_MODE0));
  display.init(115200, true, 10, false);
  display.setRotation(1); // Landscape

  // Book ID Logic
  prefs.begin("ereader", false);
  int savedID = prefs.getInt("bookID", 0);
  
  if (savedID != BOOK_ID) {
    currentPointer = 0;
    prefs.putInt("bookID", BOOK_ID);
    prefs.putUInt("lastPos", 0);
    Serial.println("New Book Detected. Starting from Page 1.");
  } else {
    currentPointer = prefs.getUInt("lastPos", 0);
    Serial.println("Resuming Book.");
  }
  prefs.end();

  // Button mapping
  button.attachClick(handleNext);       // Single click = Forward
  button.attachDoubleClick(handleBack); // Double click = Backward

  renderPage(false); 
}

void loop() {
  button.tick();
}
