#include "DHT.h"
#include <ThreeWire.h>  
#include <RtcDS1302.h>
#include <NewPing.h>
#include <nRF24L01.h>
#include <RF24.h>

#define DHTPIN    6       // Digital pin connected to the DHT sensor
#define DHTTYPE   DHT11   // DHT 11
#define RELAY1    7       // Relay pin for controlling the pump
#define RELAY2    8       // Relay pin for controlling the LED
#define SOIL      A0      // Soil moisture sensor analog pin
#define TRIG      15       // Ultrasonic sensor trigger pin
#define ECHO      16      // Ultrasonic sensor echo pin
#define CE_PIN    9       // CE pin for nrf24
#define CSN_PIN   10       // CNS pin for nrf24
#define DRY       500
#define WET       200
#define W_HEIGHT  13.75

int moist_analog;
float humidity, temperature = 0, moist_percent = 0, water_level = 0;

DHT dht(DHTPIN, DHTTYPE);
ThreeWire myWire(4, 5, 2); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);
NewPing ultrasonic(TRIG, ECHO); // Create an instance of NewPing

RF24 radio(CE_PIN, CSN_PIN);
uint64_t address = 0xF1F1F0F0E0LL;
char s_temp[10], s_moist[10], s_water[10], tx_data[32];

float wetThreshold = 100.0;  // Initial moisture threshold for wet soil
float dryThreshold = 0.0;    // Initial moisture threshold for dry soil

unsigned long previousMillis = 0;
const unsigned long interval = 2000;  // Interval for reading the DHT sensor (2 seconds)

void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);

  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();

  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();

  if (!Rtc.IsDateTimeValid()) 
  {
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing

    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }

  if (Rtc.GetIsWriteProtected())
  {
    Serial.println("RTC was write protected, enabling writing now");
    Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) 
  {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  }
  else if (now > compiled) 
  {
    Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled) 
  {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // Read DHT sensor every 2 seconds
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    humidity = dht.readHumidity();
    temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    // Adjust moisture threshold based on temperature
    if (temperature >= 30) {
      wetThreshold = 100.0;  // Adjust wet threshold for high temperatures
      dryThreshold = 30.0;   // Adjust dry threshold for high temperatures
    } else if (temperature >= 20 && temperature < 30) {
      wetThreshold = 90.0;   // Adjust wet threshold for moderate temperatures
      dryThreshold = 20.0;   // Adjust dry threshold for moderate temperatures
    } else {
      wetThreshold = 80.0;   // Adjust wet threshold for low temperatures
      dryThreshold = 10.0;   // Adjust dry threshold for low temperatures
    }

    Serial.print("Humidity: "); Serial.println(humidity);
    Serial.print("Temperature: "); Serial.println(temperature);
  }
  dtostrf(temperature, 9, 2, s_temp);

  // Soil Moisture
  moist_analog = analogRead(SOIL);
  moist_percent = map(moist_analog, WET, DRY, 100, 0);
  Serial.print("Soil Moisture: "); Serial.println(moist_percent);
  dtostrf(moist_percent, 9, 2, s_moist);

  // Pump control based on moisture percentage
  if (moist_percent < dryThreshold) {  // Pump water if moisture percentage is below threshold
    digitalWrite(RELAY1, LOW);
  } else if (moist_percent > wetThreshold) { // Stop the pump when moisture reaches desired threshold
    digitalWrite(RELAY1, HIGH);
  }

  RtcDateTime now = Rtc.GetDateTime();

  printDateTime(now);
  Serial.println();

  if (!now.IsValid())
  {
    // Common Causes:
    //    1) the battery on the device is low or even missing and the power line was disconnected
    Serial.println("RTC lost confidence in the DateTime!");
  }

  digitalWrite(RELAY2, (now.Hour() > 18 || now.Hour() < 7) ? LOW : HIGH);

  // Ultrasonic Sensor
  unsigned int distance = ultrasonic.ping_cm();
  water_level = 100 * ((W_HEIGHT - (float) distance) / W_HEIGHT);
  Serial.print("Distance: "); Serial.print(distance); Serial.println(" cm");
  Serial.print("Water level: "); Serial.print(water_level);
  dtostrf(water_level, 9, 2, s_water);
//
  sprintf(tx_data, "%s;%s;%s", s_temp, s_moist, s_water);
  radio.write(&tx_data, sizeof(tx_data)) ? Serial.print("\nSuccessfully") : Serial.print("\nUnable to");
  Serial.print(" transmit data: "); Serial.println(tx_data);

  delay(100); // Add a small delay for stability
  Serial.println();
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
  char datestring[20];

  snprintf_P(datestring, 
    countof(datestring),
    PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
    dt.Month(),
    dt.Day(),
    dt.Year(),
    dt.Hour(),
    dt.Minute(),
    dt.Second());
  Serial.print(datestring);
}
