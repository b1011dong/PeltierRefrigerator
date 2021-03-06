#include <TemperatureSensor.h>
#include <DHT11.h>

#define GLOBAL_DELAY 50

#define PIN_TEMPERATURE 0 // analog
#define PIN_RELAY 7
#define PIN_DHT11 2
#define PIN_BUTTON 12
#define PIN_RED 3
#define PIN_BLUE 4
#define PIN_GREEN 5

// 온도 설정 -> 값/10 (도씨) 예) 370 -> 37.0 도씨
#define LIMIT_NORMAL 370 // 과열 온도 (꺼짐)
#define LIMIT_UNLOCK 900 // 언락 온도 (꺼짐, 다시 켜지는 온도 = 언락 온도 - 100)
#define LIMIT_COOLDOWN 300 // 쿨링 온도 (켜짐)

#define STATE_RUN 1
#define STATE_COOLDOWN 2
#define STATE_STOP 3

#define MODE_NORMAL 1
#define MODE_UNLOCK 2
#define MODE_STOP 3

#define BUTTON_PRESSED 0
#define BUTTON_RELEASED 1

#define TRUE 1
#define FALSE 0

// Custom Structures
typedef struct _myTime
{
  int s;
  int m;
  int h;
  int d;
}myTime;

// Custom global variables
dht11 DHT11;

const int modeList[3] = {MODE_NORMAL, MODE_UNLOCK, MODE_STOP};
const int ledList[3] = {PIN_GREEN, PIN_BLUE, PIN_RED};

int state;
int currentIndex;
int overheated;
int prevButton;
int curButton;

int count;

myTime elapsedTime;

// Custom Functions
void setupRelay()
{
  // Relay Setup
  pinMode(PIN_RELAY, OUTPUT);
}

void relayOn()
{
  digitalWrite(PIN_RELAY, HIGH);
}

void relayOff()
{
  digitalWrite(PIN_RELAY, LOW);
}

void setupTemperatureSensor()
{
  pinMode(PIN_TEMPERATURE, OUTPUT);
}

int getTemperature()
{
  int temp; 
  
  temp = analogRead(PIN_TEMPERATURE) - 238;
  temp = pgm_read_word(&temperatureLookup[temp]);
  
  return temp;
}

void printTemperatureSensor()
{
  int temp = getTemperature();
  
  Serial.print("Temperature = ");
  Serial.print(temp / 10, DEC);
  Serial.print(".");
  Serial.print(temp % 10);
  Serial.println(" 'C");
}

void printDHT()
{
  int check = DHT11.read(PIN_DHT11);

  switch (check)
  {
    case 0:
      Serial.print("Temperature = ");
      Serial.print((float)DHT11.temperature, 1);
      Serial.print(" 'C");
      Serial.print(", Humidity = ");
      Serial.print((float)DHT11.humidity, 1);
      Serial.println(" %");
      break;
    case -1: Serial.println("Checksum error"); break;
    case -2: Serial.println("Time out error"); break;
    default: Serial.println("Unknown error"); break;
  }
}

void setupLED()
{
  // LED Setup
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);

  currentIndex = 0;
}

void ledOn(int ledPin)
{
  digitalWrite(ledPin, HIGH);
  switch(ledPin)
  {
    case PIN_RED: Serial.println("Red LED On"); break;
    case PIN_BLUE: Serial.println("Blue LED On"); break;
    case PIN_GREEN: Serial.println("Green LED On"); break;
    default: Serial.println("LED On Error!"); break;
  }
}

void ledOff(int ledPin)
{
  digitalWrite(ledPin, LOW);
  switch(ledPin)
  {
    case PIN_RED: Serial.println("Red LED Off"); break;
    case PIN_BLUE: Serial.println("Blue LED Off"); break;
    case PIN_GREEN: Serial.println("Green LED Off"); break;
    default: Serial.println("LED Off Error!"); break;
  }
}

void setupButton()
{
  // Button Setup
  pinMode(PIN_BUTTON, INPUT);

  prevButton = BUTTON_RELEASED;
  curButton = BUTTON_RELEASED;
}

int getButtonState()
{
  return digitalRead(PIN_BUTTON);
}

void addElapsedTime()
{
  elapsedTime.s += 1;

  if(elapsedTime.s == 60)
  {
    elapsedTime.s = 0;
    elapsedTime.m += 1;
  }
  if(elapsedTime.m == 60)
  {
    elapsedTime.m = 0;
    elapsedTime.h += 1;
  }
  if(elapsedTime.h == 24)
  {
    elapsedTime.h = 0;
    elapsedTime.d += 1;
  }
}

void resetElapsedTime()
{
  elapsedTime.s = 0;
  elapsedTime.m = 0;
  elapsedTime.h = 0;
  elapsedTime.d = 0;
}

void printElapsedTime()
{
  if(state == STATE_RUN)
    Serial.print("Run  ");
  else
    Serial.print("Stop ");

   Serial.print(elapsedTime.d);
   Serial.print("d, ");
   Serial.print(elapsedTime.h);
   Serial.print("h, ");
   Serial.print(elapsedTime.m);
   Serial.print("m, ");
   Serial.print(elapsedTime.s);
   Serial.println("s");
}

void setNextMode()
{
  int prev = currentIndex;
  
  currentIndex = (currentIndex + 1) % 3;
  ledOff(ledList[prev]);
  ledOn(ledList[currentIndex]);

  if(ledList[prev] == MODE_NORMAL && state == STATE_COOLDOWN)
    resetElapsedTime();
  else if(ledList[prev] == MODE_UNLOCK && state != STATE_COOLDOWN)
    resetElapsedTime();
  else if(ledList[prev] == MODE_STOP)
    resetElapsedTime();
}

void overheat()
{
  overheated = TRUE;
  if(state == STATE_RUN || state == STATE_STOP)
    resetElapsedTime();
  state = STATE_COOLDOWN;
  relayOff();
}

void cooldown()
{
  overheated = TRUE;
  if(state == STATE_COOLDOWN || state == STATE_STOP)
    resetElapsedTime();
  state = STATE_RUN;
  relayOn();
}

int getMode()
{
  curButton = getButtonState();
  
  if(curButton == BUTTON_PRESSED)
  {
    if(prevButton == BUTTON_RELEASED)
    {
      setNextMode();
      prevButton = BUTTON_PRESSED;
    }
  }
  else
  {
    prevButton = BUTTON_RELEASED;
  }

  return modeList[currentIndex];
}

void normalProcedure()
{
  int temp = getTemperature();
  
  if(temp > LIMIT_NORMAL)
  {
    overheat();
  }
  else
  {
    if(overheated == TRUE)
    {
      if(temp < LIMIT_COOLDOWN)
        overheated = FALSE;
    }
    else
    {
      cooldown();
    }
  }
}

void unlockProcedure()
{
  int temp = getTemperature();
  
  if(temp > LIMIT_UNLOCK)
  {
    overheated = TRUE;
    state = STATE_COOLDOWN;
    relayOff();
  }
  else
  {
    if(overheated == TRUE)
    {
      if(temp < (LIMIT_UNLOCK - 100))
        overheated = FALSE;
    }
    else
    {
      relayOn();
      state = STATE_RUN;
      overheated = FALSE;
    }
  }
}

void stopProcedure()
{
  state = STATE_STOP;
  relayOff();
}

void printState()
{
  count = (count + 1) % (1000 / GLOBAL_DELAY);
  if(count != 0)
    return;
  
  addElapsedTime();
  
  Serial.print("Current Mode               : ");
  switch(getMode())
  {
  case MODE_NORMAL:
    Serial.println("Normal Mode");
    break;
  case MODE_UNLOCK:
    Serial.println("Unlock Mode");
    break;
  case MODE_STOP:
    Serial.println("Stop Mode");
    break;
  default:
    Serial.println("Error Mode");
    break;
  }
  Serial.print("Current State              : ");
  switch(state)
  {
  case STATE_RUN:
    Serial.println("Running");
    break;
  case STATE_COOLDOWN:
    Serial.println("Cooling Down");
    break;
  case STATE_STOP:
    Serial.println("Stopped");
    break;
  default:
    break;
  }
  Serial.print("Hot Side State             : ");
  printTemperatureSensor();
  Serial.print("Cool Side State(Estimated) : ");
  Serial.print("Temperature = ");
  if(state == STATE_COOLDOWN)
  {
    Serial.println("N/A");
  }
  else
  {
    Serial.print((float)((getTemperature() - 220) / 10), 1);
    Serial.println(" 'C");
  }
  Serial.print("Weather State              : ");
  printDHT();
  Serial.print("Elapsed Time               : ");
  printElapsedTime();
  
  Serial.println();
}

// Default Functions
void setup() 
{
  Serial.begin(9600);
  setupTemperatureSensor();
  setupRelay();
  setupLED();
  ledOn(PIN_GREEN);
  setupButton();
  resetElapsedTime();
  Serial.println("Ready");
}

void loop() 
{
  switch(getMode())
  {
   case MODE_NORMAL:
    normalProcedure();
    break;
   case MODE_UNLOCK:
    unlockProcedure();
    break;
   case MODE_STOP:
    stopProcedure();
    break;
   default: Serial.println("Error!"); break;
  }
  
  printState();
  delay(GLOBAL_DELAY);
}
