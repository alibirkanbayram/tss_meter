// ━━━━━━━━━━━━━━━━━━━━━ Library ━━━━━━━━━━━━━━━━━━━━━
#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>    //https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library
#include <OneWire.h>
#include <DallasTemperature.h>

// ━━━━━━━━━━━━━━━━━━━━━ WiFi Variable definition ━━━━━━━━━━━━━━━━━━━━━
#define WIFI_SSID "Birkan"
#define WIFI_PASSWORD "7717C8963D

// ━━━━━━━━━━━━━━━━━━━━━ Firebase Variable definition ━━━━━━━━━━━━━━━━━━━━━
#define API_KEY "REPLACE_WITH_YOUR_FIREBASE_PROJECT_API_KEY"
#define DATABASE_URL "REPLACE_WITH_YOUR_FIREBASE_DATABASE_URL""
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
String path = "";

// ━━━━━━━━━━━━━━━━━━━━━ Variable definition ━━━━━━━━━━━━━━━━━━━━━
LiquidCrystal_I2C lcd(0x27, 16, 2);
int sensorPin = A0;
float volt, ntu,acceptableValue= 0.5;

// ━━━━━━━━━━━━━━━━━━━━━ EC Meter Variable definition ━━━━━━━━━━━━━━━━━━━━━
int R1= 1000, Ra=25, Ecpin= A1, ECPower=A2,ECGround=A3 ;
//*********** Converting to ppm [Learn to use EC it is much better**************//
// Hana      [USA]        PPMconverion:  0.5
// Eutech    [EU]          PPMconversion:  0.64
//Tranchen  [Australia]  PPMconversion:  0.7
// Why didnt anyone standardise this?
float PPMconversion=0.7;
float TemperatureCoef = 0.019; //this changes depending on what chemical we are measuring
float K=2.88; // 2.9

#define ONE_WIRE_BUS 10          // Data wire For Temp Probe is plugged into pin 10 on the Arduino
const int TempProbePossitive =8;  //Temp Probe power connected to pin 9
const int TempProbeNegative=9;    //Temp Probe Negative connected to
OneWire oneWire(ONE_WIRE_BUS);// Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire);// Pass our oneWire reference to Dallas Temperature.

float Temperature=10, EC=0, EC25 =0, ppm =0, raw= 0, Vin= 5, Vdrop= 0, Rc= 0, buffer=0  ;

// ━━━━━━━━━━━━━━━━━━━━━ EC Meter Variable definition ━━━━━━━━━━━━━━━━━━━━━
int pH_Value, phPin=A4;

// ━━━━━━━━━━━━━━━━━━━━━ Setup Variable definition ━━━━━━━━━━━━━━━━━━━━━
unsigned int serialPortBound=115200;

// ━━━━━━━━━━━━━━━━━━━━━ Setup function ━━━━━━━━━━━━━━━━━━━━━
void setup(){
    Serial.begin(9600);
    lcd.begin();
    lcd.backlight();

    // Firebase
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED){
        lcd.print(".");
        delay(300);
    }
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connected with IP: ");
    lcd.setCursor(0,1);
    lcd.print(WiFi.localIP());
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    firebase_init();

    pinMode(doser1, OUTPUT);
    digitalWrite(doser1, LOW);

    // EC Meter
    pinMode(TempProbeNegative , OUTPUT ); //seting ground pin as output for tmp probe
    digitalWrite(TempProbeNegative , LOW );//Seting it to ground so it can sink current
    pinMode(TempProbePossitive , OUTPUT );//ditto but for positive
    digitalWrite(TempProbePossitive , HIGH );
    pinMode(ECPin,INPUT);
    pinMode(ECPower,OUTPUT);//Setting pin for sourcing current
    pinMode(ECGround,OUTPUT);//setting pin for sinking current
    digitalWrite(ECGround,LOW);//We can leave the ground connected permanantly

    delay(100);// gives sensor time to settle
    sensors.begin();
    delay(100);
    R1=(R1+Ra);// Taking into acount Powering Pin Resitance

    // PH Meter
    pinMode(pH_Value, INPUT); 
}

// ━━━━━━━━━━━━━━━━━━━━━ Loop function ━━━━━━━━━━━━━━━━━━━━━
void loop(){
    lcd.clear();
    lcd.setCursor(0,0);
    float values=measurement();
    lcd.print(values[0]);
    lcd.print(" V");
    lcd.setCursor(0,1);
    lcd.print(values[1]);
    lcd.print(" NTU");
    delay(10);

    getEC();
    getPH();
}

// ━━━━━━━━━━━━━━━━━━━━━ Dosing function ━━━━━━━━━━━━━━━━━━━━━
void dosing(){
    while(value[1]<=acceptableValue){
        digitalWrite(doser1, HIGH);
        delay(10);
        float value=measurement();
    }
    digitalWrite(doser1, LOW);
}

// ━━━━━━━━━━━━━━━━━━━━━ Measurement function ━━━━━━━━━━━━━━━━━━━━━
float measurement(){
    volt = 0;
    for(int i=0; i<800; i++)
    {
        volt += ((float)analogRead(sensorPin)/1023)*5;
    }
    volt = volt/800;
    volt = round_to_dp(volt,2);
    if(volt < 2.5){
        ntu = 3000;
    }else{
        ntu = -1120.4*square(volt)+5742.3*volt-4353.8;
    }
    float returnValue[2] = [volt, ntu];
    return returnValue;
}

// ━━━━━━━━━━━━━━━━━━━━━ Raund to DP function ━━━━━━━━━━━━━━━━━━━━━
float round_to_dp( float in_value, int decimal_place ){
  float multiplier = powf( 10.0f, decimal_place );
  in_value = roundf( in_value * multiplier ) / multiplier;
  return in_value;
}
void getEC(){
    sensors.requestTemperatures();// Send the command to get temperatures
    Temperature=sensors.getTempCByIndex(0); //Stores Value in Variable
    digitalWrite(ECPower,HIGH);
    raw= analogRead(ECPin);
    raw= analogRead(ECPin);// This is not a mistake, First reading will be low beause if charged a capacitor
    digitalWrite(ECPower,LOW);
    Vdrop= (Vin*raw)/1024.0;
    Rc=(Vdrop*R1)/(Vin-Vdrop);
    Rc=Rc-Ra; //acounting for Digital Pin Resitance
    EC = 1000/(Rc*K);
    EC25  =  EC/ (1+ TemperatureCoef*(Temperature-25.0));
    ppm=(EC25)*(PPMconversion*1000);
    }

void getPH(){
    pH_Value = analogRead(phPin);
    Voltage = pH_Value * (5.0 / 1023.0);
    Serial.println(Voltage);
    delay(500);
}

void firebaseSetValue(float value){
    Firebase.set(fbdo, node.c_str(), value),
}