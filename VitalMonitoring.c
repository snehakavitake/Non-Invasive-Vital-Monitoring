#include <Wire.h> //enable i2c communication
#include <LiquidCrystal_I2C.h> //to operate lcd
#include <DHT.h> //library for temp sensor

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  //initialize lcd 16x2 at add 0x27

// Pulse sensor
int pulsePin = A0; //s connected to analog pin
int blinkPin = 13; //led will blink with heatbeat

// DHT11 sensor setup //    
#define DHTPIN 2       // DHT11 data pin connected to D2
#define DHTTYPE DHT11  //type for temp sensor DHT11 
DHT dht(DHTPIN, DHTTYPE);  //initialise

// Pulse sensor variables
volatile int BPM; //beats per min 
volatile int Signal; //pulse sensor signal stored
volatile int IBI = 600; //time btw detected beats
volatile boolean Pulse = false; //true when heart beat detected
volatile boolean QS = false; // t…
[12:21 pm, 16/4/2025] pranjal: #include <Wire.h> //enable i2c communication
#include <LiquidCrystal_I2C.h> //to operate lcd
#include <DHT.h> //library for temp sensor

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  //initialize lcd 16x2 at add 0x27

// Pulse sensor
int pulsePin = A0; //s connected to analog pin
int blinkPin = 13; //led will blink with heatbeat

// DHT11 sensor setup //    
#define DHTPIN 2       // DHT11 data pin connected to D2
#define DHTTYPE DHT11  //type for temp sensor DHT11 
DHT dht(DHTPIN, DHTTYPE);  //initialise

// Pulse sensor variables
volatile int BPM; //beats per min 
volatile int Signal; //pulse sensor signal stored
volatile int IBI = 600; //time btw detected beats
volatile boolean Pulse = false; //true when heart beat detected
volatile boolean QS = false; // true when heart beat is found(trigger serial output)

static boolean serialVisual = true;  //serial output

volatile int rate[10]; //store last 10 ibi value
volatile unsigned long sampleCounter = 0; //timer count
volatile unsigned long lastBeatTime = 0; //timer since last beat
volatile int P = 512; //peak for wave
volatile int T = 512; //trough
volatile int thresh = 525; //beat detection treshold
volatile int amp = 100; //amplitude of wave
volatile boolean firstBeat = true; //first beat flag
volatile boolean secondBeat = false; //second beat flag

unsigned long lastDisplayChange = 0;  //time since last lcd switch
bool showHeartbeat = true; //alt btw heartbeat and temp

void setup() { 
  pinMode(blinkPin, OUTPUT); //13 as output for led blink
  Serial.begin(115200); //start serial communication
  interruptSetup(); //configure timer 2 for periodic intervals

  lcd.init(); //initialize lcd
  lcd.backlight();  //start backlight

  dht.begin();  // Start DHT11
}

void loop() {  


  if (QS == true) {  //if heart beat detected recently
    serialOutputWhenBeatHappens(); //send heartbeat to SM
    QS = false; //reset flag
  }

  // Alternate LCD view every 2.5 seconds
  if (millis() - lastDisplayChange > 2500) {
    lcd.clear();
    if (showHeartbeat) {
      lcd.setCursor(0, 0);
      lcd.print("Heartbeat: ");
      lcd.print(BPM);
      lcd.print(" BPM");
    } else {
      float temp = dht.readTemperature();

      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      lcd.print(temp);
      lcd.print(" C");
      
    }
    showHeartbeat = !showHeartbeat;
    lastDisplayChange = millis(); //update timestamp for lcd change
  }

  delay(20);
}

void interruptSetup() { //timer2 used to generate interrupt
  TCCR2A = 0x02;
  TCCR2B = 0x06;
  OCR2A = 0X7C;
  TIMSK2 = 0x02;
  sei(); //allow all(global) interrupt to happen
}



void serialOutputWhenBeatHappens() { //serial monitor display for heartbeat
  if (serialVisual) {
    Serial.print(" Heart-Beat Found ");
    Serial.print("BPM: ");
    Serial.println(BPM);
  } else {
    sendDataToSerial('B', BPM);
    sendDataToSerial('Q', IBI);
  }
}
void sendDataToSerial(char symbol, int data) {
  Serial.print(symbol);
  Serial.println(data);
}

ISR(TIMER2_COMPA_vect) {
  cli(); //disbale interrupt during critical section
  Signal = analogRead(pulsePin); //read from sensor from A0
  sampleCounter += 2; 
  int N = sampleCounter - lastBeatTime; //calculate time since last beat

  if (Signal < thresh && N > (IBI / 5) * 3) {
    if (Signal < T) {
      T = Signal; //is signal dips below threshold and enough time has passed update T
    }
  }

  if (Signal > thresh && Signal > P) {
    P = Signal;  //if signal goes above threshold update P
  }  

  if (N > 250) {  //check for next beat only if at least 250ms has passed
    if ((Signal > thresh) && (Pulse == false) && (N > (IBI / 5) * 3)) {  //signal cross threshold and beat isnt ongoing
      Pulse = true;  //pulse detected
      digitalWrite(blinkPin, HIGH);  //diplay in led
      IBI = sampleCounter - lastBeatTime; //time btw this and previous beat
      lastBeatTime = sampleCounter; 

      if (secondBeat) {
        secondBeat = false;
        for (int i = 0; i <= 9; i++) rate[i] = IBI;
      }

      if (firstBeat) {
        firstBeat = false;
        secondBeat = true;
        sei();
        return; //skip ibm calculation on first beat
      }

      word runningTotal = 0;
      for (int i = 0; i <= 8; i++) {
        rate[i] = rate[i + 1]; //shift data left
        runningTotal += rate[i];
      }

      rate[9] = IBI; //add new ibi
      runningTotal += rate[9]; 
      runningTotal /= 10; //avg ibi
      BPM = 60000 / runningTotal; //calculate bmp 
      QS = true; //beat detected; signal to loop
    }
  }

  if (Signal < thresh && Pulse == true) {
    digitalWrite(blinkPin, LOW);  //turn off led
    Pulse = false;
    amp = P - T;
    thresh = amp / 2 + T; //calculate new threshold dynamically
    P = thresh;  
    T = thresh;
  }

  if (N > 2500) {  //no deat detected for 2.5 sec
    thresh = 512; //reset to default
    P = 512;
    T = 512;
    lastBeatTime = sampleCounter;
    firstBeat = true;
    secondBeat = false;
  }

  sei();
}
