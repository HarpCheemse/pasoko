// CHANGE LCD TO 0X27 BEFORE UPLOAD
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

// Hardware

const byte RED_LED = 13;
const byte GREEN_LED = 12;

byte KEYPAD_ROW[4] = { 9, 8, 7, 6 };
byte KEYPAD_COL[4] = { 5, 4, 3, 2 };
const char hexaKeys[4][4] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), KEYPAD_ROW, KEYPAD_COL, 4, 4);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// A, B, C, D, E, F, G
const byte segPins[] = { 11, A3, A0, A1, A2, 10, 1 };
const byte digits[10] = {
  0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110,
  0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111
};

// Const
const char* diffNames[] = {
  "Easy",
  "Medium",
  "Hard"
};

// ENUM
enum Screen {
  PASOKO,
  PASOKO_INFO,

  GAME_SELECTOR,

  MATH_MASTER,
  MATH_MASTER_INFO,
  MATH_MASTER_DIFF,
  MATH_MASTER_PLAYING,
  MATH_MASTER_RESULT,

  NUM_MEMORY,
  NUM_MEMORY_INFO,
  NUM_MEMORY_PLAYING,
  NUM_MEMORY_RESULT,
};

enum Difficulty {
  EASY,
  MEDIUM,
  HARD
};

enum GameType {
  ENDLESS,
  TEST
};

// STRUCT
struct Question {
  String expression;
  String answer;
  int timer;
};

struct MathMasterContext {
  GameType gameType;
  Difficulty diff;
  byte questionCount;
  Question question;
  byte questionProgress;
  byte correctAnswers;
  String userInput;
  int totalSessionTime;
  unsigned long sessionStartTime;
  MathMasterContext()
    : gameType(TEST),
      diff(EASY),
      questionCount(10),
      questionProgress(0),
      correctAnswers(0),
      totalSessionTime(0),
      sessionStartTime(0) {}
};

struct NumberContext {
  String number;
  String userInput;
  byte progress;
};

// FORWARD DECLARATIONS
void mainMenu(char key);
void mainMenuInfo(char key);
void gameSelectorMenu(char key);
void mathMasterMenu(char key);
void mathMasterDiffSelector(char key);
void mathMasterInfoMenu(char key);
void mathMasterPlaying(char key);
void mathMasterResult(char key);
void numMemoryMenu(char key);
void numMemoryInfoMenu(char key);
void numMemoryPlaying(char key);
void numMemoryResult(char key);

Question generateQuestion(Difficulty diff);

class ScreenController;
class TestController;

extern ScreenController ui;
extern TestController testCtrl;
extern MathMasterContext mathContext;
extern NumberContext numberContext;

// CONTROLLERS

// ------------------------------------------------------------
// TestController
// ------------------------------------------------------------
// Handles the countdown timer, 7-segment display animation,
// and LED feedback for correct/incorrect answers.
//
// Responsibilities:
// 1. Manage a countdown timer for each question
// 2. Display remaining time on the 7-segment display
// 3. Animate a spinning clock while time > 9 seconds
// 4. Flash LEDs for correct / incorrect answers
//
// This controller is updated every loop() frame via update().
// ------------------------------------------------------------
class TestController {
public:
  TestController() {
    active = false;
    ledStartTime = 0;
  }

  void start(byte seconds) {
    duration = seconds;
    startTime = millis();
    active = true;
  }

  void correctIndicator(bool isCorrect) {
    flashCorrectLed(isCorrect);
  }

  void stopTimer() {
    active = false;
  }

  bool isExpired() {
    return !active;
  }

  void update() {
    unsigned long elapsed = (millis() - startTime) / 1000;
    unsigned long ledElapsed = millis() - ledStartTime;
    bool shouldLedActive = ledElapsed <= ledFlashDuration;

    if (!shouldLedActive) {
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, LOW);
    }
    if (!active) {
      clearDisplay();
      return;
    }
    int remaining = duration - elapsed;
    if (remaining > 9) {
      animateClock();
    } else if (remaining >= 0) {
      showDigit(remaining);
    } else {
      clearDisplay();
      active = false;
    }
  }

private:
  unsigned long startTime;
  unsigned long ledStartTime;
  byte duration;
  bool active;

  const int ledFlashDuration = 500;

  void flashCorrectLed(bool isCorrect) {
    if (isCorrect) {
      digitalWrite(GREEN_LED, HIGH);
    } else {
      digitalWrite(RED_LED, HIGH);
    }
    ledStartTime = millis();
  }

  void animateClock() {
    byte frame = (millis() / 150) % 6;
    for (byte i = 0; i < 7; i++) {
      digitalWrite(segPins[i], i == frame ? LOW : HIGH);
    }
  }

  void showDigit(byte digit) {
    for (byte i = 0; i < 7; i++) {
      digitalWrite(segPins[i], !bitRead(digits[digit], i));
    }
  }

  void clearDisplay() {
    for (byte i = 0; i < 7; i++) {
      digitalWrite(segPins[i], HIGH);
    }
  }
};

/*
ScreenController

PASOKO ENGINE
Lightweight UI state machine that avoids unnecessary LCD redraws.

Flow:
1. Normal frame → currScreen == prevScreen → no redraw
2. setScreen() → currScreen changes but old screen still visible
3. Next frame → shouldRedraw() == true → new screen draws
4. prevScreen updated → system stabilizes

This ensures screens only redraw once after a transition.

Use:
- setScreen(screen) → navigate to another screen
- setState() → force redraw of current screen (like Flutter setState)
- shouldRedraw() → check if UI should draw this frame
*/
class ScreenController {
public:
  Screen currScreen, prevScreen;
  ScreenController()
    : currScreen(PASOKO), prevScreen(PASOKO_INFO) {}
  //prev screen must be different in first frame
  //so shouldRedraw eval to true in first frame of the program
  void transitionHelper(byte column) {
    for (byte row = 0; row < 2; row++) {
      for (byte col = column; col < 16; col += 2) {
        lcd.setCursor(col, row);
        lcd.print(" ");
        delay(10);
      }
    }
  }

  void transition() {
    transitionHelper(0);
    transitionHelper(1);
  }

  void update(char key) {
    switch (currScreen) {
      case PASOKO: mainMenu(key); break;
      case PASOKO_INFO: mainMenuInfo(key); break;

      case GAME_SELECTOR: gameSelectorMenu(key); break;

      case MATH_MASTER: mathMasterMenu(key); break;
      case MATH_MASTER_INFO: mathMasterInfoMenu(key); break;
      case MATH_MASTER_DIFF: mathMasterDiffSelector(key); break;
      case MATH_MASTER_PLAYING: mathMasterPlaying(key); break;
      case MATH_MASTER_RESULT: mathMasterResult(key); break;

      case NUM_MEMORY: numMemoryMenu(key); break;
      case NUM_MEMORY_INFO: numMemoryInfoMenu(key); break;
      case NUM_MEMORY_PLAYING: numMemoryPlaying(key); break;
      case NUM_MEMORY_RESULT: numMemoryResult(key); break;
    }
  }

  void setScreen(byte screen) {
    if (screen != currScreen) {
      prevScreen = currScreen;
      currScreen = (Screen)screen;  //CAST TO SCREEN SO TINKERCAD WONT VOMIT
      transition();
    }
  }

  void write(byte c, byte r, const __FlashStringHelper* s) {
    lcd.setCursor(c, r);
    byte pos = c + lcd.print(s);
    while (pos < 16) {
      lcd.print(' ');
      pos++;
    }
  }

  void write(byte c, byte r, const String& s) {
    lcd.setCursor(c, r);
    byte pos = c + lcd.print(s);
    while (pos < 16) {
      lcd.print(' ');
      pos++;
    }
  }

  void clear() {
    lcd.clear();
  }

  bool shouldRedraw() {
    if (currScreen != prevScreen) {
      //Move to the next frame
      prevScreen = currScreen;
      return true;
    }
    return false;
  }

  void setState() {
    prevScreen = (Screen)-1;
  }
};

class MathMasterGame {
public:
  void start() {
    ctx.question = generateQuestion(ctx.diff);
    ctx.sessionStartTime = millis();
    ctx.userInput = "";
    testCtrl.start(ctx.question.timer);
    ui.write(0, 0, ctx.question.expression);
    ui.write(0, 1, "> ");
  }

  void update(char key) {

    if (testCtrl.isExpired()) {
      handleTimeout();
      return;
    }

    if (isDigit(key)) {
      handleDigit(key);
    } else if (key == 'D') {
      handleDelete();
    } else if (key == '#') {
      handleSubmit();
    } else if (key == '*') {
      ui.setScreen(MATH_MASTER_RESULT);
    }
  }

  void result() {
    if (mathContext.gameType == ENDLESS) {
      ui.write(0, 0, F("GAME OVER!"));
      ui.write(11, 0, "S:" + String(mathContext.correctAnswers));
    } else {
      ui.write(0, 0, "TEST: " + String(mathContext.correctAnswers) + "/" + String(mathContext.questionCount));
    }
    float avg = 0;
    if (mathContext.questionProgress > 0) {
      avg = (mathContext.totalSessionTime / mathContext.questionProgress) / 1000.0;
    }
    ui.write(0, 1, "Avg: " + String(avg, 2) + "s");
  }

  void reset() {
    mathContext.questionProgress = 0;
    mathContext.correctAnswers = 0;
    mathContext.totalSessionTime = 0;
  }
private:
  MathMasterContext& ctx = mathContext;

  Question generateQuestion(Difficulty diff) {
    Question q;

    if (diff == EASY) q.timer = 10;
    else if (diff == MEDIUM) q.timer = 15;
    else q.timer = 20;

    byte a, b;
    int result;
    char op;

    byte opCount = 0;
    char ops[4];

    switch (diff) {
      case HARD:
        ops[opCount++] = '/';
      case MEDIUM:
        ops[opCount++] = '*';
      case EASY:
        ops[opCount++] = '+';
        ops[opCount++] = '-';
        break;
    }

    op = ops[random(opCount)];

    a = random(1, 21);
    b = random(1, 21);

    switch (op) {
      case '+':
        result = a + b;
        break;

      case '-':
        if (b > a) {
          byte t = a;
          a = b;
          b = t;
        }
        result = a - b;
        break;

      case '*':
        result = a * b;
        break;

      case '/':
        result = random(2, 13);
        a = result * b;
        break;
    }

    q.expression = String(a) + op + String(b) + "=?";
    q.answer = String(result);

    return q;
  }

  void handleDigit(char key) {
    if (ctx.userInput.length() >= 14) return;

    ctx.userInput += key;
    ui.write(2, 1, ctx.userInput);
  }

  void handleDelete() {
    if (ctx.userInput.length() == 0) return;

    ctx.userInput.remove(ctx.userInput.length() - 1);
    ui.write(2 + ctx.userInput.length(), 1, " ");
  }

  void handleSubmit() {

    addSessionTime();

    if (ctx.userInput == ctx.question.answer) {
      ctx.correctAnswers++;

      testCtrl.correctIndicator(true);

      ui.write(2, 1, F("Correct!"));
    } else {
      testCtrl.correctIndicator(false);

      ui.write(2, 1, F("Wrong!"));

      if (ctx.gameType == ENDLESS) {
        delay(500);
        ui.setScreen(MATH_MASTER_RESULT);
        return;
      }
    }

    delay(500);
    nextQuestionOrEnd();
  }

  void handleTimeout() {
    addSessionTime();

    ui.write(0, 1, "> TimeOut!");
    delay(500);

    if (ctx.gameType == ENDLESS) {
      ui.setScreen(MATH_MASTER_RESULT);
      return;
    }

    nextQuestionOrEnd();
  }

  void addSessionTime() {
    ctx.totalSessionTime += millis() - ctx.sessionStartTime;
  }

  void nextQuestionOrEnd() {
    if (ctx.questionProgress == ctx.questionCount - 1) {
      ui.setScreen(MATH_MASTER_RESULT);

    } else {
      ctx.questionProgress++;

      //Preparing next question
      ui.setState();
    }
  }
};

class NumberMemoryGame {
public:
  void start() {
    inputPhase = false;
    isOver = false;
    lastBlink = false;
    byte digits = numberContext.progress + 1;

    if (digits > 14) digits = 14;
    // First digit not 0
    numberContext.number = String(random(1, 10));
    for (byte i = 1; i < digits; i++) {
      numberContext.number += char('0' + random(10));
    }

    numberContext.userInput = "";

    testCtrl.start(numberContext.progress * 3 + 2);

    ui.write(0, 0, numberContext.number);
    ui.write(0, 1, F("Remember"));

    inputPhase = false;
  }

  void update(char key) {
    if (!inputPhase && (key || testCtrl.isExpired())) {
      beginInputPhase();
    }

    if (!inputPhase) return;

    if (isOver) {
      handleGameOver(key);
      return;
    }

    handleInput(key);
  }

  void result() {
    ui.write(1, 0, F("GAME OVER!"));
    ui.write(2, 1, "Score: " + String(numberContext.progress));
  }

  void reset() {
    numberContext.progress = 0;
  }

private:
  bool inputPhase = false;
  bool isOver = false;
  bool lastBlink = false;

  void beginInputPhase() {
    inputPhase = true;

    testCtrl.stopTimer();

    ui.write(0, 0, "What Number?    ");
    ui.write(0, 1, F("> "));
  }

  void handleInput(char key) {

    String& userInput = numberContext.userInput;
    String& number = numberContext.number;

    if (isDigit(key) && userInput.length() < number.length()) {
      userInput += key;
      ui.write(2, 1, userInput);
    }

    else if (key == 'D' && userInput.length() > 0) {
      userInput.remove(userInput.length() - 1);
      ui.write(2 + userInput.length(), 1, " ");
    }

    else if (key == '#') {
      if (userInput == number) {
        numberContext.progress++;
        testCtrl.correctIndicator(true);
        ui.write(0, 1, "> Correct!");
        delay(500);

        ui.setState();
      } else {
        testCtrl.correctIndicator(false);
        isOver = true;
      }
    }
  }

  void handleGameOver(char key) {

    bool blink = (millis() / 250) % 2;

    if (blink != lastBlink) {
      lastBlink = blink;

      String userInput = numberContext.userInput;
      String& number = numberContext.number;

      if (blink) {
        for (byte i = 0; i < number.length(); i++) {
          if (i >= userInput.length() || userInput[i] != number[i]) {
            userInput[i] = '_';
          }
        }
      }

      ui.write(0, 0, "A:" + number);
      ui.write(0, 1, "> " + userInput);
    }

    if (key) {
      isOver = false;
      ui.setScreen(NUM_MEMORY_RESULT);
    }
  }
};

// GLOBAL VARIABLES
ScreenController ui;
MathMasterContext mathContext;
NumberContext numberContext;
TestController testCtrl;
MathMasterGame mathGame;
NumberMemoryGame numberGame;

// CORE
void setup() {
  lcd.init();
  lcd.backlight();
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  for (byte i = 0; i < 7; i++) {
    pinMode(segPins[i], OUTPUT);
    digitalWrite(segPins[i], HIGH);
  }
}

// MAIN
void loop() {
  delay(10);
  char key = customKeypad.getKey();
  ui.update(key);
  testCtrl.update();
}

// SCREENS
void mainMenu(char key) {
  if (ui.shouldRedraw()) {
    lcd.clear();
    ui.write(5, 0, F("PASOKO"));
    ui.write(2, 1, F("Info Press C"));
  }
  if (key == 'C') {
    ui.setScreen(PASOKO_INFO);
  } else if (key == '#') {
    randomSeed(millis());
    ui.setScreen(GAME_SELECTOR);
  }
}

void mainMenuInfo(char key) {
  static byte page = 0;
  if (ui.shouldRedraw()) {
    ui.clear();
    if (page == 0) {
      ui.write(4, 0, F("GROUP 1"));
      ui.write(4, 1, F("MEMBERS"));
    } else if (page == 1) {
      ui.write(0, 0, F("1.HarpCheemse"));
      ui.write(0, 1, F("2.---------"));
    } else if (page == 2) {
      ui.write(0, 0, F("3.----------"));
      ui.write(0, 1, F("4.----------"));
    } else if (page == 3) {
      ui.write(0, 0, F("*Back  #Next"));
      ui.write(0, 1, F("D Delete"));
    }
  }
  if (key && key != '*') {
    page = (page + 1) % 4;
    ui.setState();
  } else if (key == '*') {
    page = 0;
    ui.setScreen(PASOKO);
  }
}

void gameSelectorMenu(char key) {
  if (ui.shouldRedraw()) {
    ui.clear();
    ui.write(0, 0, F("A: Math Master"));
    ui.write(0, 1, F("B: Number Memory"));
  }
  if (key == '*') {
    ui.setScreen(PASOKO);
  } else if (key == 'A') {
    ui.setScreen(MATH_MASTER);
  } else if (key == 'B') {
    ui.setScreen(NUM_MEMORY);
  }
}

void mathMasterMenu(char key) {
  if (ui.shouldRedraw()) {
    ui.clear();
    ui.write(3, 0, F("MATH MASTER"));
    ui.write(2, 1, F("Info Press C"));
  }
  if (key == 'C') {
    ui.setScreen(MATH_MASTER_INFO);
  } else if (key == '#') {
    ui.setScreen(MATH_MASTER_DIFF);
  } else if (key == '*') {
    ui.setScreen(GAME_SELECTOR);
  }
}

void mathMasterInfoMenu(char key) {
  if (ui.shouldRedraw()) {
    ui.write(0, 0, "Test: " + String(mathContext.questionCount) + "q");
    ui.write(0, 1, F("Endless: NOLIMIT"));
  }
  if (key == '#' || key == '*') {
    ui.setScreen(MATH_MASTER);
  }
}

void mathMasterDiffSelector(char key) {
  if (ui.shouldRedraw()) {
    ui.clear();
    String config = diffNames[mathContext.diff];
    config += (mathContext.gameType == TEST) ? " Test" : " Endless";
    ui.write(0, 0, config);
    ui.write(0, 1, "A: Diff  B: Type");
  }
  if (key == 'A') {
    mathContext.diff = (Difficulty)((mathContext.diff + 1) % 3);
    ui.setState();
  } else if (key == 'B') {
    mathContext.gameType = (mathContext.gameType == TEST) ? ENDLESS : TEST;
    ui.setState();
  } else if (key == '*') {
    ui.setScreen(MATH_MASTER);
  } else if (key == '#') {
    ui.setScreen(MATH_MASTER_PLAYING);
  }
}

void mathMasterPlaying(char key) {
  if (ui.shouldRedraw()) {
    mathGame.start();
  }
  mathGame.update(key);
}

void mathMasterResult(char key) {
  if (ui.shouldRedraw()) {
    testCtrl.stopTimer();
    mathGame.result();
  }
  if (key) {
    mathGame.reset();
    ui.setScreen(MATH_MASTER);
  }
}

void numMemoryMenu(char key) {
  if (ui.shouldRedraw()) {
    ui.clear();
    ui.write(1, 0, F("NUMBER MEMORY"));
    ui.write(2, 1, F("Info Press C"));
  }
  if (key == 'C') {
    ui.setScreen(NUM_MEMORY_INFO);
  } else if (key == '#') {
    ui.setScreen(NUM_MEMORY_PLAYING);
  } else if (key == '*') {
    ui.setScreen(GAME_SELECTOR);
  }
}

void numMemoryInfoMenu(char key) {
  if (ui.shouldRedraw()) {
    ui.clear();
    ui.write(1, 0, F("Remeber number"));
    ui.write(1, 1, F("And win"));
  }
  if (key == '#' || key == '*') {
    ui.setScreen(NUM_MEMORY);
  }
}

void numMemoryPlaying(char key) {
  if (ui.shouldRedraw()) {
    numberGame.start();
  }
  numberGame.update(key);
}

void numMemoryResult(char key) {
  if (ui.shouldRedraw()) {
    numberGame.result();
  }
  if (key) {
    numberGame.reset();
    ui.setScreen(NUM_MEMORY);
  }
}