const int sensorPin = 7; // PWM input pin connected to the sensor
const int relayPin = 4;  // Digital pin connected to the relay module

void setup() {
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  Serial.begin(9600);
}

void loop() {
  // Read the PWM signal from the sensor
  int sensorValue = pulseIn(sensorPin, HIGH);

  // Set a threshold value (adjust as needed)
  int threshold = 1500; // You may need to calibrate this value

  // Control the relay based on the sensor reading
  if (sensorValue < threshold) {
    digitalWrite(relayPin, HIGH); // Turn on the pump/mixer
    Serial.println("Milk level low, mixing ON");
  } else {
    digitalWrite(relayPin, LOW); // Turn off the pump/mixer
    Serial.println("Milk level sufficient, mixing OFF");
  }

  delay(1000); // Delay for stability
}
