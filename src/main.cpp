#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <MQ135.h>
#include <math.h>

#define BLYNK_TEMPLATE_ID "TMPL6zqrXlz6W"
#define BLYNK_TEMPLATE_NAME "Air Quality"
#define BLYNK_AUTH_TOKEN "2UTofEl7sqo4TeIot3yCgO_I9q9kPKYz"

#include <WiFiClient.h>
#include <BlynkSimpleWifi.h>

#include <arduino_secrets.h>
#include <WiFiS3.h>

#define I2C_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define ARR_SIZE 5

// PIN USED

int measurePin = A2;
int MQ135Pin = A0;
int ledPower = 7;
int dht22_data = 4;
const byte buttonPin = 2;

BlynkTimer timer;

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT_Unified dht(dht22_data, DHT22);
MQ135 mq135_sensor(MQ135Pin);

unsigned int samplingTime = 280;
unsigned int deltaTime = 40;
unsigned int sleepTime = 9680;

float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

float temperature = 0;
float relative_humidity = 0;
int AQI = 0;

unsigned long int startup_timer = 0;

bool blynk_state = false;

unsigned long int button_triggered = 0;


float dust_data[ARR_SIZE] = {0, 0, 0, 0, 0};
float co2_ppm_data[ARR_SIZE] = {0, 0, 0, 0, 0};
 
int i = 0;
float find_avg(float pushed_data, float* arr){
  if(i > ARR_SIZE - 1){
    i = 0;
  }
  arr[i++] = pushed_data;

  float avg = 0;
  for(int j = 0; j < ARR_SIZE; j++){
    avg += arr[j];
  }
  avg /= ARR_SIZE;

  Serial.println(avg);
  return avg; 
}

float co2_ppm = 0;

void getDataDHT(float &temp, float &rh){
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  temp = event.temperature - 4.0;
  rh = dht.humidity().getEvent(&event);
  rh = event.relative_humidity - 4.0;
}

void printText(int x, int y, String text){
  display.setCursor(x, y);
  display.print(text);
}

void printText(int x, int y, float value){
  display.setCursor(x, y);
  display.print(value);
}

void printText(int x, int y, int value){
  display.setCursor(x, y);
  display.print(value);
}

int calc_aqi_epa(float concentration){
  float aqi;
  float c_low[8] = {0, 12.1, 35.5, 55.5, 150.5, 250.5, 350.5, 500.5};
  float c_high[8] = {12, 35.4, 55.4, 150.4, 250.4, 350.4, 500.4, 1000.0};
  float i_low[8] = {0, 51, 101, 151, 201, 301, 401, 501};
  float i_high[8] = {50, 100, 150, 200, 300, 400, 500, 9999};

  for(int i = 0; i < 8; i++){
    if ( concentration >= c_low[i] && concentration <= c_high[i]){
      aqi = ( ( i_high[i] - i_low[i]) / (c_high[i] - c_low[i]) ) * (concentration - c_low[i]) + i_low[i];
      Serial.println(aqi);
      return static_cast<int>(aqi);
    }
  }
  
  if (concentration > c_high[7]){
    aqi = ( ( i_high[7] - i_low[7]) / (c_high[7] - c_low[7]) ) * (concentration - c_low[7]) + i_low[7];
  }
  else{
    aqi = 0.0;
  }

  Serial.println(aqi);
  return static_cast<int>(aqi);
}

void DSDataCollect(float &vo_measured, float &calc_voltage, float &dust_density, int &aqi){
  digitalWrite(ledPower,LOW);
  delayMicroseconds(samplingTime);

  vo_measured = analogRead(measurePin);

  delayMicroseconds(deltaTime);
  digitalWrite(ledPower,HIGH);
  delayMicroseconds(sleepTime);

  calc_voltage = vo_measured*(5.0/1024);
  dust_density = 170*calc_voltage-0.1;

  if ( dust_density < 0)
  {
    dust_density = 0.00;
  }

  //Serial.println("V0");
  //Serial.println(vo_measured);
  //Serial.println("dust density");
  //Serial.println(dust_density);
  //Serial.println("\n");

  aqi = calc_aqi_epa(find_avg(dust_density, dust_data));
}

void get_co2_ppm(float &CO2_ppm){
  float correctedRZero = mq135_sensor.getCorrectedRZero(temperature, relative_humidity);
  float resistance = mq135_sensor.getResistance();
  float cur_co2_ppm = mq135_sensor.getCorrectedPPM(temperature, relative_humidity);

  co2_ppm = find_avg(cur_co2_ppm, co2_ppm_data);
  //Serial.println(analogRead(MQ135Pin));
  //Serial.println(correctedRZero);
  //Serial.println(resistance);
  //Serial.println(co2_ppm);
}

void connect_manage(){
  //if(millis() - button_triggered > 3000){ 
    //blynk_state = !blynk_state;
    //Serial.println("Interrupt triggered");
  //}
  //button_triggered = millis();

  blynk_state = !blynk_state;
}

void status_display(){
  delay(1000);
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  

  if(millis() - startup_timer < 6000 ){
    display.setTextSize(2);
    printText(20, 10, "Waiting");
  }
  else{
    display.setTextSize(1);
  
    display.drawFastHLine(0,32, 128, SH110X_WHITE);
    display.drawFastVLine(90, 0, 64, SH110X_WHITE);

    // PRINT AQI
    printText(16, 8, "PM2.5 AQI:");
    printText(40, 20, AQI);

    printText(16, 36, "CO2 (ppm):");
    if(co2_ppm <= 1500.0){
      printText(40, 48, co2_ppm);
    }
    else{
      printText(10, 48, "unstable data");
    }

    printText(94, 8, "T(oC)");
    printText(94, 20, temperature);
  
    printText(94, 36, "%RH");
    printText(94, 48, relative_humidity);

    if(Blynk.connected()){
      printText(0, 54, "C");
    }
    else{
      printText(0, 54, "U");
    }
  }
}

void send_data_blynk(){
  Blynk.virtualWrite(V1, temperature);
  Blynk.virtualWrite(V2, relative_humidity);
  Blynk.virtualWrite(V4, co2_ppm);
  Blynk.virtualWrite(V5, AQI);
}

void wifiConnect(){
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println("Connection established");
}

void setup(){
  Serial.begin(9600);

  startup_timer = millis(); 

  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), connect_manage, CHANGE);

  timer.setInterval(20000L, send_data_blynk);

  pinMode(ledPower,OUTPUT);

  dht.begin();

  display.begin(I2C_ADDRESS, true);
  display.display();
  display.clearDisplay();
  delay(1000);
}

void loop(){
  DSDataCollect(voMeasured, calcVoltage, dustDensity, AQI);
  getDataDHT(temperature, relative_humidity);
  get_co2_ppm(co2_ppm);

  if(!blynk_state){
    WiFi.disconnect();
    Blynk.disconnect();
    Serial.println("Working with no connection");
  }
  else{
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    Blynk.connect();
    Serial.println("Working with connection");
  }

  Blynk.run();
  timer.run();

  status_display();
  display.display();
  delay(1000);
}