#include <DHT.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <RTClib.h>
#include <Wire.h>

#define DHTPIN    4         // Digital pin connected to the DHT sensor
#define DHTTYPE   DHT11     // DHT 11
#define TRIGPIN   5         // Ultrasonic sensor trigger pin
#define ECHOPIN   18        // Ultrasonic sensor echo pin
#define RELAY1    19        // Relay pin for controlling the pump
#define RELAY2    13        // Relay pin for controlling the LED
#define SOIL      1         // Soil moisture sensor analog pin
#define CE_PIN    7         // CE pin for nrf24
#define CSN_PIN   8         // CNS pin for nrf24

const int HEIGHT_OF_TUBE = 10;    // Height of the water tank in cm
const float SOUND_SPEED = 0.034;  // Speed of sound in cm/microsecond
const int WET = 210;              // Moisture reading for wet soil
const int DRY = 510;              // Moisture reading for dry soil

// Create a DHT object.
DHT dht(DHTPIN, DHTTYPE);
long duration;
float water_level;

RTC_DS3231 rtc;
char ts[32];

RF24 radio(CE_PIN, CSN_PIN);
uint64_t address = 0xF1F1F0F0E0LL;
char tx_data[32];

float temperature; // Temperature reading from DHT sensor
float wetThreshold = 100.0; // Initial moisture threshold for wet soil
float dryThreshold = 0.0;   // Initial moisture threshold for dry soil

int moist_analog;
float moist_percent;

void setup() {
  Serial.begin(115200);
  dht.begin();          // Initialize the DHT sensor
  pinMode(TRIGPIN, OUTPUT); // Set the trigPin as an Output
  pinMode(ECHOPIN, INPUT);  // Set the echoPin as an Input
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT); // Set the LED pin as an Output

  Wire.begin();
  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Adjust RTC to compile time

  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();
}

void loop() {
  // DHT sensor
  temperature = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  // Print the temperature to the serial port
  Serial.print("Temperature: "); Serial.println(temperature);

  // Adjust moisture threshold based on temperature
  if (temperature >= 30) {
    wetThreshold = 100.0; // Adjust wet threshold for high temperatures
    dryThreshold = 30.0;  // Adjust dry threshold for high temperatures
  } else if (temperature >= 20 && temperature < 30) {
    wetThreshold = 90.0; // Adjust wet threshold for moderate temperatures
    dryThreshold = 20.0; // Adjust dry threshold for moderate temperatures
  } else {
    wetThreshold = 80.0; // Adjust wet threshold for low temperatures
    dryThreshold = 10.0; // Adjust dry threshold for low temperatures
  }

  // Soil Moisture
  moist_analog = analogRead(SOIL);
  moist_percent = map(moist_analog, WET, DRY, 100, 0);
  Serial.print("Soil Moisture: "); Serial.println(moist_percent);

  // Pump control based on moisture percentage
  if (moist_percent < dryThreshold) {  // Pump water if moisture percentage is below threshold
    digitalWrite(RELAY1, LOW);
  } else if (moist_percent > wetThreshold) { // Stop the pump when moisture reaches desired threshold
    digitalWrite(RELAY1, HIGH);
  }

  // Ultrasonic sensor
  // Clears the trigPin
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 microseconds
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(ECHOPIN, HIGH);
  // Calculate the distance
  water_level = duration * SOUND_SPEED / 2;
  // Prints the distance in the Serial Monitor
  Serial.print("Distance (cm): "); Serial.println(water_level);
  if (water_level > HEIGHT_OF_TUBE) {
    Serial.println("Water Level Low!!");
  }

  // Turn LED on/off based on current time
  DateTime now = rtc.now();
  sprintf(ts, "%02d:%02d:%02d %02d/%02d/%02d", now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());
  Serial.print("Date/Time: "); Serial.println(ts);
  digitalWrite(RELAY2, now.hour() >= 18 && now.hour() < 7 ? HIGH : LOW);

  // Attempt to send data to Raspberry Pi gateway
  sprintf(tx_data, "%.3f;%.3f,%.3f", temperature, moist_percent, water_level);
  radio.write(&tx_data, sizeof(tx_data)) ? Serial.print("Successfully") : Serial.print("Unable to");
  Serial.print(" transmit data: "); Serial.println(tx_data);

  delay(2000); // Obtain DHT sensor reading every 2 seconds (limit is 1kHz)
  Serial.println();
}
