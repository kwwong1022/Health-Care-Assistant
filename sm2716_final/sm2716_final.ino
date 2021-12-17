#include <LCD.h>
#include <Wire.h>
#include <Servo.h>
#include <IRremote.h>
#include <LedControl.h>
#include <LiquidCrystal_I2C.h>

const String emptyLCD = "                ";
const int MODE_SLEEP = -1;
const int MODE_INIT_CLOCK = 0;
const int MODE_DETECT_HEART_BEAT = 1;
const int MODE_DETECT_DETECT_USER = 2;
const int MODE_MENU = 3;
const int MODE_IDLE = 4;
const int MODE_GAME = 5;
const int MODE_HEATMAP = 6;

const int RECV_PIN = 3;
const int FACE_PAT = 6;
const int buzzerPin = 2;
const int motionPin = 4;
const int lightSensorPin = A0;
const int pulseSensorPin = A3;
const int echoPin = 9, trigPin = 8;
const int din2 = 7, cs2 = 6, clk2 = 5;
const int din1 = 12, cs1 = 11, clk1 = 10;
const int panServoPin = A1, tiltServoPin = A2;

Servo panServo;
decode_results results;
IRrecv irrecv(RECV_PIN);
LedControl lc1 = LedControl(din1, clk1, cs1, 0);
LedControl lc2 = LedControl(din2, clk2, cs2, 0);
LiquidCrystal_I2C lcd(0x20, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

int mode;
long prevSec = 0;
long prevTSec = 0;
long timeSecond = 0;
int clockInput[] = {-1, -1, -1, -1};

int hr;
//int hrDay = -1;
int hrDay = 17;
int hrBPM = -1;
int stayedMin = 0;
int hrNotiTime = 0;
int THRESHOLD = 338;
int hrHeatmapTime = 0;
int avgRecordedHr = 0;
int ACCEPTANCE_TIME = 100;
bool needHRCheck = false;
bool isStayedLong = false;
bool isMovedInTenMins = false;
bool isHeartBeatChecked = false;
int avgBPMs[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
long heartBeatTime[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
//int recordedBPMs[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
int recordedBPMs[] = {70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70};
//int recordedBPMs[] = {140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140};
//int recordedBPMs[] = {40, 50, 60, 70, 100, 80, 50, 40, 80, 100, 90, 50, 40, 60, 70, 80};

int lightLevel = 0;
int pirState = LOW;
int time, distance = 999;

int patternI = 0;
int currPattern = 6;
int actionSelected = 0;

int panAngle = 84;
int userAngle = -1;
int closestDist = 999;
int closestAngle = 999;
int minPanAngle = 44, maxPanAngle = 124;
bool isMovingDirLeft = true;
int panScanTime = 0;
int SCAN_TIMES = 2;
bool isScaned = false;

int DEFAULT_COUNT_DOWN_TIME = 30;
int countDownTime = DEFAULT_COUNT_DOWN_TIME;
int score = 0, countingTime = 500;
int prevGSec = 0, nextCountingTime = -1;
bool isCounting = false, isCounted = false, gameOver = false;

void setup() {
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(motionPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(panServoPin, OUTPUT);
  pinMode(lightSensorPin, INPUT);
  pinMode(pulseSensorPin, INPUT);

  panServo.attach(panServoPin);
  
  Serial.begin(9600);
  
  irrecv.enableIRIn();
  
  lcd.begin(16, 2);
  lcd.backlight();

  mode = MODE_INIT_CLOCK;
  beep();

  panServo.write(panAngle);

  lc1.shutdown(0,false);
  lc1.setIntensity(0,1);
  lc1.clearDisplay(0); 
  lc2.shutdown(0,false);
  lc2.setIntensity(0,1);
  lc2.clearDisplay(0);

  initAnimation();
  nextCountingTime = random(2500, 3000);
}

void initAnimation() {
  printMatrixPattern(0);
  beep();
  delay(1000);
  lcd.setCursor(0, 1);
  printMatrixPattern(1);
  beep();
  delay(1000);
  lcd.setCursor(0, 1);
  printMatrixPattern(2);
  beep();
  delay(1000);
}

void loop() {
  long ms = millis()%86400000;

  if (mode != MODE_INIT_CLOCK) { findUser(); }

  if (mode != MODE_DETECT_HEART_BEAT) {
    detectDist();
    detectUserHr(ms);
    //detectLightLevel();
  }
  
  getRemoteAction();

  if (ms-prevSec > 1000) {
    timeSecond ++;
    prevSec = ms;
    
    if (timeSecond>86400){
      timeSecond = 0;
      hrNotiTime = 0;
      isHeartBeatChecked = false;
      if (recordedBPMs[15]!=-1) { for (int i=0; i<14; i++) { recordedBPMs[i] = recordedBPMs[i+1]; } }
    }

    switch (mode) {
      case MODE_IDLE:
        if (timeSecond>79200 && !isHeartBeatChecked && hrNotiTime<180) {
          updateClock();
          lcd.setCursor(6, 0);
          lcd.print("       >OK");
          lcd.setCursor(0, 1);
          lcd.print("Check HR BPM?   ");
          needHRCheck = true;
          beep();
          hrNotiTime++;
        } else if (isStayedLong) {
          lcd.setCursor(0, 0);
          lcd.print("Stayed too long!");
          lcd.setCursor(0, 1);
          lcd.print("Take a rest  >OK");
        } else {
          updateClock();
          lcd.setCursor(6, 0);
          lcd.print("   OK:MENU");
          lcd.setCursor(0, 1);
          lcd.print("================");
        }
        break;
      case MODE_DETECT_HEART_BEAT:
        lcd.setCursor(0, 0);
        lcd.print("HR-BPM    OK:END");
        lcd.setCursor(0, 1);
        lcd.print("AVG-BPM:");
        if (hrBPM>30 && hrBPM<140) {
          lcd.setCursor(8, 1);           
          lcd.print(hrBPM);
          lcd.print(" ");
        }
        lcd.setCursor(11, 1);
        lcd.print("     ");
        if (timeSecond%2==0) {
          printMatrixPattern(3);
        } else { printMatrixPattern(4); }
        break;
      case MODE_MENU:
        lcd.setCursor(0, 0);
        lcd.print("Menu   OK:ACTION");
        break;
      case MODE_HEATMAP:
        if (hrHeatmapTime<=1) {
          printMatrixPattern(-1);
          lcd.setCursor(0, 0);
          lcd.print("HR BPM Recorded ");
          lcd.setCursor(0, 0);
          lcd.print("================");
        } else if (hrHeatmapTime>1 && hrHeatmapTime<7) {
          printMatrixPattern(5);
          lcd.setCursor(0, 0);
          lcd.print("= HR  RECORDED =");
          lcd.setCursor(0, 1);
          lcd.print("Avg HR:");
          lcd.setCursor(7, 1);
          lcd.print(avgRecordedHr);
          lcd.print("      ");
          if (hrHeatmapTime==6) { mode=MODE_IDLE; }
        }
        break;
      case MODE_GAME:
        if (countDownTime>0) {
          countDownTime--;
          lcd.setCursor(0, 0);
          lcd.print("Time:    OK:EXIT");
          lcd.setCursor(5, 0);
          lcd.print(countDownTime);
          lcd.setCursor(0, 1);
          lcd.print(emptyLCD);
        } else {
          lcd.setCursor(0, 0);
          lcd.print("GAMEOVER OK:EXIT");
          lcd.setCursor(0, 1);
          lcd.print("Score:");
          lcd.print(score);
        }
        break;
    }
    
    if (mode!=MODE_DETECT_HEART_BEAT) {
      if (mode == MODE_HEATMAP) { 
        hrHeatmapTime++; 
      } else if (mode == MODE_GAME) { 
        printMatrixPattern(9 + patternI); 
      } else {
        printMatrixPattern(currPattern + patternI); 
      }
      
      patternI++;
      patternI%=3;
    }
  }

  if (ms-prevTSec > 600000) {
    if (isMovedInTenMins) {
      stayedMin++;
      Serial.print("stayedMin++");
    } else { stayedMin = 0; }

    if (stayedMin > 9) {
      isStayedLong = true;
      beep();
      beep();
      beep();
    }

    isMovedInTenMins = false;
    prevTSec = ms;
  }

  switch(mode) {
    case MODE_INIT_CLOCK:
      initClock();
      break;
    case MODE_DETECT_HEART_BEAT:
      detectUserHr(ms);
      break;
  }

  if (mode == MODE_GAME) {
    if (ms-prevGSec > nextCountingTime+700 && countDownTime > 0 && patternI == 2) {
        prevGSec = ms;
        nextCountingTime = random(2500, 5000);
        countingTime = 300;
        isCounted = false;
        printMatrixPattern(15);
    }
      
    if (countDownTime > 0) {
      if (countingTime > 0) {
        isCounting = true;
        countingTime--;
      } else { isCounting = false; }

      if (isCounting && !isCounted && distance<12) {
          score += 10;
          isCounted = true;
          beep();
      }
    } else { gameOver = true; }
  }

  if (mode != MODE_DETECT_HEART_BEAT) {
    if (digitalRead(motionPin)==HIGH) {
      if (pirState == LOW) {
        isMovedInTenMins = true;
        pirState = HIGH;
      }
    } else {
      if (pirState == HIGH) {
        pirState = LOW;
      }
    }
  }
}

void printMatrixPattern(int patternNo) {
  byte leftloading1[8]= {};
  byte leftloading2[8]= {B00000000,B00000000,B00000000,B00000000,B00000000,B00000000,B00011000,B00100100};
  byte leftloading3[8]= {B00011000,B00100100,B00100100,B00011000,B00000000,B00000000,B00011000,B00100100};
  byte rightloading1[8]= {B00000000,B00000000,B00000000,B00000000,B00011000,B00100100,B00100100,B00011000};
  byte rightloading2[8]= {B00100100,B00011000,B00000000,B00000000,B00011000,B00100100,B00100100,B00011000};
  byte rightloading3[8]= {B00100100,B00011000,B00000000,B00000000,B00011000,B00100100,B00100100,B00011000};

  byte leftHeart1[8]= {B00000000,B00000000,B00000000,B01110000,B11111000,B11111100,B11111110,B01111111};
  byte leftHeart2[8]= {B00000000,B00000000,B00000000,B00000000,B00110000,B01111000,B01111100,B00111110};
  byte rightHeart1[8]= {B01111111,B11111110,B11111100,B11111000,B01110000,B00000000,B00000000,B00000000};
  byte rightHeart2[8]= {B00111110,B01111100,B01111000,B00110000,B00000000,B00000000,B00000000,B00000000};

  byte leftHeatMap[8];
  byte rightHeatMap[8];

  if (mode == MODE_HEATMAP) {
    int barLength = ((recordedBPMs[0] - 30.0) / (140.0 - 30.0) * (8.0 - 0.0));
    for (int i=0; i<8; i++) { leftHeatMap[i] = B00000000; }
    for (int i=0; i<8; i++) { rightHeatMap[i] = B00000000; }

    for (int i=7; i>=0; i--) {
      int j = 7-i;
      if (recordedBPMs[7-i] != -1) {
        int barLength = ((recordedBPMs[7-i] - 30.0) / (140.0 - 30.0) * (8.0 - 0.0));

        switch(barLength) {
          case 0:
            rightHeatMap[i] = B00000000;
            break;
          case 1:
            rightHeatMap[i] = B00000001;
            break;
          case 2:
            rightHeatMap[i] = B00000011;
            break;
          case 3:
            rightHeatMap[i] = B00000111;
            break;
          case 4:
            rightHeatMap[i] = B00001111;
            break;
          case 5:
            rightHeatMap[i] = B00011111;
            break;
          case 6:
            rightHeatMap[i] = B00111111;
            break;
          case 7:
            rightHeatMap[i] = B01111111;
            break;
          case 8:
            rightHeatMap[i] = B11111111;
            break;
        }
      } else { rightHeatMap[i] = B00000000; }
    }
    for (int i=7; i>=0; i--) {
      if (recordedBPMs[7-i+8] != -1) {
        int barLength = ((recordedBPMs[7-i+8] - 30.0) / (140.0 - 30.0) * (8.0 - 0.0));
        
        switch(barLength) {
          case 0:
            leftHeatMap[i] = B00000000;
            break;
          case 1:
            leftHeatMap[i] = B00000001;
            break;
          case 2:
            leftHeatMap[i] = B00000011;
            break;
          case 3:
            leftHeatMap[i] = B00000111;
            break;
          case 4:
            leftHeatMap[i] = B00001111;
            break;
          case 5:
            leftHeatMap[i] = B00011111;
            break;
          case 6:
            leftHeatMap[i] = B00111111;
            break;
          case 7:
            leftHeatMap[i] = B01111111;
            break;
          case 8:
            leftHeatMap[i] = B11111111;
            break;
        }
      } else { leftHeatMap[i] = B00000000; }
    }
    
    if (hrDay < 8) { for (int i=0; i<8; i++) { leftHeatMap[i] = B00000000; } }
  }
  
  byte leftSmile1[8]= {B01000000,B01001100,B11010010,B11010010,B11010010,B01001100,B01000000,B00000000};
  byte leftSmile2[8]= {B01000000,B01000100,B11001010,B11001010,B11001010,B01000100,B01000000,B00000000};
  byte leftSmile3[8]= {B01000000,B01000100,B11000110,B11000110,B11000110,B01000100,B01000000,B00000000};
  byte rightSmile1[8]= {B00000000,B01000000,B01001100,B11010010,B11010010,B11010010,B01001100,B01000000};
  byte rightSmile2[8]= {B00000000,B01000000,B01000100,B11001010,B11001010,B11001010,B01000100,B01000000};
  byte rightSmile3[8]= {B00000000,B01000000,B01000100,B11000110,B11000110,B11000110,B01000100,B01000000};

  byte leftHappy1[8]= {B00100000,B01101110,B01011111,B11011111,B11011111,B11000111,B01000110,B00000000};
  byte leftHappy2[8]= {B00100000,B01100100,B01001110,B11001110,B11001110,B11000110,B01000100,B00000000};
  byte leftHappy3[8]= {B00100000,B01100100,B01000100,B11000110,B11000110,B11000100,B01000100,B00000000};
  byte rightHappy1[8]= {B00000000,B01001110,B11011111,B11011111,B11011111,B01000111,B01100110,B00100000};
  byte rightHappy2[8]= {B00000000,B01000100,B11001110,B11001110,B11001110,B01000110,B01100100,B00100000};
  byte rightHappy3[8]= {B00000000,B01000100,B11000100,B11000110,B11000110,B01000100,B01100100,B00100000};

  byte leftSad1[8]= {B00100000,B00100110,B01101111,B01101111,B01001111,B11000111,B10000110,B00000000};
  byte leftSad2[8]= {B00100000,B00100100,B01101110,B01101110,B01001110,B11000110,B10000100,B00000000};
  byte leftSad3[8]= {B00100000,B00100101,B01100110,B01100110,B01000110,B11000110,B10000100,B00000000};
  byte rightSad1[8]= {B00000000,B10000110,B11001111,B01001111,B01101111,B01100111,B00100110,B00100000};
  byte rightSad2[8]= {B00000000,B10000100,B11001110,B01001110,B01101110,B01100110,B00100100,B00100000};
  byte rightSad3[8]= {B00000000,B10000100,B11000110,B01000110,B01100110,B01100110,B00100101,B00100000};

  byte leftIdle1[8]= {B01000010,B01100110,B01100110,B00110110,B00010100,B00011100,B00001000,B00000000};
  byte rightIdle1[8]= {B00000000,B00001000,B00011100,B00010110,B00110110,B01100110,B01100110,B01000010};
  
  switch (patternNo) {
    case -1:
      printByte(leftloading1, leftloading1);
      break;
    case 0:
      printByte(leftloading1, rightloading1);
      break;
    case 1:
      printByte(leftloading2, rightloading2);
      break;
    case 2:
      printByte(leftloading3, rightloading3);
      break;
    case 3:
      printByte(leftHeart1, rightHeart1);
      break;
    case 4:
      printByte(leftHeart2, rightHeart2);
      break;
    case 5:
      printByte(leftHeatMap, rightHeatMap);
      break;
    case 6:
      printByte(leftSmile1, rightSmile1);
      break;
    case 7:
      printByte(leftSmile2, rightSmile2);
      break;
    case 8:
      printByte(leftSmile3, rightSmile3);
      break;
    case 9:
      printByte(leftHappy1, rightHappy1);
      break;
    case 10:
      printByte(leftHappy2, rightHappy2);
      break;
    case 11:
      printByte(leftHappy3, rightHappy3);
      break;
    case 12:
      printByte(leftSad1, rightSad1);
      break;
    case 13:
      printByte(leftSad2, rightSad2);
      break;
    case 14:
      printByte(leftSad3, rightSad3);
      break;
    case 15:
      printByte(leftIdle1, rightIdle1);
      break;
  }
}

void printByte(byte character1[], byte character2[]) {
  int i = 0;
  for(i=0; i<8; i++) {
    lc1.setRow(0,i,character1[i]);
    lc2.setRow(0,i,character2[i]);
  }
}

void initClock() {
  String h1, h2, m1, m2;
  int clockArrLength = 0;
  
  for (int i=0; i<4; i++) { if (clockInput[i] != -1) clockArrLength++; }
  
  h1 = clockInput[0]==-1? "_":String(clockInput[0]);
  h2 = clockInput[1]==-1? "_":String(clockInput[1]);
  m1 = clockInput[2]==-1? "_":String(clockInput[2]);
  m2 = clockInput[3]==-1? "_":String(clockInput[3]);
  
  lcd.setCursor(0, 0);
  lcd.print("Clock Time:  >OK");
  lcd.setCursor(2, 1);
  lcd.print(":");
  lcd.setCursor(5, 1);
  lcd.print("      hh:mm");
  lcd.setCursor(0, 1);
  lcd.print(h1);
  lcd.setCursor(1, 1);
  lcd.print(h2);
  lcd.setCursor(3, 1);
  lcd.print(m1);
  lcd.setCursor(4, 1);
  lcd.print(m2);
}

void findUser() {
  // put inside timer 1s
  if (panScanTime < SCAN_TIMES) {
    
    if (userAngle!=-1 && mode!=MODE_DETECT_HEART_BEAT) {
      if (isMovingDirLeft) {
        panAngle--;
        
        if (panAngle < minPanAngle+1) {
          isMovingDirLeft = false; 
        }
        
      } else {
        panAngle++;
        
        if (panAngle > maxPanAngle-1) {
          isMovingDirLeft = true; 
          panScanTime++;
        }
      }
    }
    panServo.write(panAngle);
  }
  
  if (panScanTime<SCAN_TIMES && closestDist>distance) { 
    closestDist = distance;
    closestAngle = panAngle;
  } else { userAngle = closestAngle; }

  // pan to user
  if (panScanTime==SCAN_TIMES && panAngle>userAngle) {
    panAngle--;
    if (panAngle < minPanAngle) { panAngle = minPanAngle; }
    if (!isScaned) { panServo.write(panAngle); }
    if (panAngle==userAngle) { isScaned = true; }
  }
}

void updateClock() {   
  int hh = timeSecond/3600;
  int mm = timeSecond%3600/60;
  
  lcd.setCursor(0, 0);
  if (hh < 10) {
    lcd.print(0);
    lcd.print(hh);   } 
  else { lcd.print(hh); }
    lcd.print(":");
  if (mm < 10) {
    lcd.print(0);
    lcd.print(mm);
  } else { lcd.print(mm); }
    lcd.print(" ");
}

void getRemoteAction() {
  if (irrecv.decode(&results)) {
    int action = -1;
    irrecv.resume();

    switch(results.value) {
      case 16753245:
        action = 1;
        break;
      case 16736925:
        action = 2;
        break;
      case 16769565:
        action = 3;
        break;
      case 16720605:
        action = 4;
        break;
      case 16712445:
        action = 5;
        break;
      case 16761405:
        action = 6;
        break;
      case 16769055:
        action = 7;
        break;
      case 16754775:
        action = 8;
        break;
      case 16748655:
        action = 9;
        break;
      case 16750695:
        action = 0;
        break;
      case 16738455:
        action = 11;
        break;
      case 16756815:
        action = 12;
        break;
      case 16718055:
        action = 13;
        break;
      case 16730805:
        action = 14;
        break;
      case 16716015:
        action = 15;
        break;
      case 16734885:
        action = 16;
        break;
      case 16726215:
        action = 17;
        break;
    }

    // MODE_INIT_CLOCK
    if (mode == MODE_INIT_CLOCK) {
      int clockArrLength = 0;
      for (int i=0; i<4; i++) { if (clockInput[i] != -1) clockArrLength++; }
      if (clockArrLength==0 && action>=0 && action<=2) {
        clockInput[0] = action;
      } else if (clockArrLength==1 && action>=0 && action<=10) {
        clockInput[1] = action;
      } else if (clockArrLength==2 && action>=0 && action<=5) {
        clockInput[2] = action;
      } else if (clockArrLength==3 && action>=0 && action<=10) {
        clockInput[3] = action;
      } else if (clockArrLength==4 && action==17) {
        int hh = (clockInput[0]*10 + clockInput[1]);
        int mm = (clockInput[2]*10 + clockInput[3]);
        if ((hh*3600L) + (mm*60L) > 86400) {
          for (int i=0; i<4; i++) { clockInput[i] = -1; }
        } else {
          mode = 4;
          timeSecond = (hh*3600L) + (mm*60L);
          emptyLcd();
          updateClock();
          beep();

          userAngle = -1;
          closestDist = 999;
          closestAngle = 999;
          isMovingDirLeft = true;
          panScanTime = 0;
          isScaned = false;
          
          delay(100);
          return;
        }
      }
      delay(10);
      
    // MODE_IDLE
    } else if (mode == MODE_IDLE) { 
      if (action==17 && !needHRCheck && !isStayedLong) {
        mode = 3; 
        switchMenuAction();
      } else if (action==17 && needHRCheck) {
        mode = 1;
      } else if (action==17 && isStayedLong) {
        isStayedLong = false;
      }
      beep();
      return; 
      
    // MODE_DETECT_HEART_BEAT
    } else if (mode == MODE_DETECT_HEART_BEAT) {
      if (action==17) { 
        mode=4;
        if (!isHeartBeatChecked) {
          hrDay++;
          isHeartBeatChecked = true;
        }
        beep();

        // record hr here
        int tempTotal = 0;
        if (recordedBPMs[15]!=-1) {
          recordedBPMs[15] = hrBPM;
          for (int i=0; i<16; i++) { tempTotal+=recordedBPMs[i]; }
          avgRecordedHr = tempTotal/16;

            if (avgRecordedHr>100) {
              currPattern = 12;
            } else { currPattern = 9; }
          
        } else {
          recordedBPMs[hrDay] = hrBPM;
          for (int i=0; i<hrDay+1; i++) { tempTotal+=recordedBPMs[i]; }
          avgRecordedHr = tempTotal/(hrDay+1);
        }
        
        mode = MODE_HEATMAP;
        hrHeatmapTime = 0;
        return; 
      } 
      
    // MODE_MENU
    } else if (mode == MODE_MENU) { 
      if (action==15) {
        actionSelected--;
        if (actionSelected<0) actionSelected = 3;
        switchMenuAction();
      } else if (action==16) {
        actionSelected++;
        if (actionSelected>3) actionSelected = 0;
        switchMenuAction();
      } else if (action==17) {
        if (actionSelected==0) {
          mode = 4;
        } else if (actionSelected==1) {
          int avgBPMs[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
          long heartBeatTime[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
          lcd.setCursor(8, 1);
          lcd.print("   ");
          mode = 1;
        } else if (actionSelected==2) {
          mode = 5;
          countDownTime = DEFAULT_COUNT_DOWN_TIME;
          score = 0;
          isCounting = false;
          isCounted = false;
          gameOver = false;
          prevGSec = millis()%86400000;
          nextCountingTime = random(1500, 4000);
          
        } else if (actionSelected==3) {
          mode = 0;
          for (int i=0; i<4; i++) { clockInput[i]=-1; }
        }
        beep();
      }
    
    // MODE_GAME
    } else if (mode == MODE_GAME) {
      if (action==17) {
        isStayedLong = false;
        mode = 4;
        beep();
        return; 
      }
    }
  }
}

void switchMenuAction() {
  switch (actionSelected) {
    case 0:
      lcd.setCursor(0, 1);
      lcd.print("<     Exit     >");
      break;
    case 1:
      lcd.setCursor(0, 1);
      lcd.print("<  Heart Beat  >");
      break;
    case 2:
      lcd.setCursor(0, 1);
      lcd.print("<  Play  Game  >");
      break;
    case 3:
      lcd.setCursor(0, 1);
      lcd.print("< Reset  Clock >");
      break;
  }
}

void beep() {
  digitalWrite(buzzerPin, 1);
  delay(5);
  digitalWrite(buzzerPin, 0);
}

void emptyLcd() {
  lcd.setCursor(0, 0);
  lcd.print(emptyLCD);
  lcd.setCursor(0, 1);
  lcd.print(emptyLCD);
}

void detectUserHr(long ms) {
  bool newHrDetected = detectedHr();
  if (newHrDetected) {
    
    if (heartBeatTime[9] != -1) {
      for (int i=0; i<9; i++) { heartBeatTime[i] = heartBeatTime[i+1]; }
      heartBeatTime[9] = ms;
      long temp = heartBeatTime[9]-heartBeatTime[0];
      float tempf = 1000.0/(temp/10.0)*60.0;
      
      int BPMTotal = 0;
      if (avgBPMs[9] != -1) {
        for (int i=0; i<9; i++) { avgBPMs[i] = avgBPMs[i+1]; }
        avgBPMs[9] = tempf;
        for (int i=0; i<10; i++) { BPMTotal += tempf; }
        
        hrBPM = BPMTotal/10;
      } else {
        for (int i=0; i<10; i++) {
          if (avgBPMs[i] == -1) {
            avgBPMs[i] = tempf;
            BPMTotal += tempf;
            break;
          }
        }
      }
    } else {
      for (int i=0; i<10; i++) {
        if (heartBeatTime[i] == -1) {
          heartBeatTime[i] = ms;
          break;
        }
      }
    }
  }
}

bool detectedHr() {
  static long last_time_beat = 0;

  bool isOutputBeated = false;
  int hrValue = analogRead(pulseSensorPin);
  
  if (hrValue>THRESHOLD && hrValue<345) {
    unsigned long present_time = millis();

    if (present_time - last_time_beat > ACCEPTANCE_TIME) {
      isOutputBeated = true;
      last_time_beat = present_time;
    }
  }
  delay(10);
  
  return(isOutputBeated);
}

//void detectLightLevel() {
//  lightLevel = analogRead(lightSensorPin)>100? 100:analogRead(lightSensorPin);
//}

void detectDist() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  time = pulseIn(echoPin, HIGH);
  distance = 0.034/2*time;
}
