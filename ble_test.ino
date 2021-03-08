#include <ArduinoBLE.h>
#include <Arduino_LSM6DS3.h>

BLEService heartRateService("180D"); // BLE Heart Rate Service
// BLE Heart Rate Measurement Characteristic
BLEFloatCharacteristic heartRateChar("2A37",  // standard 16-bit characteristic UUID
    BLERead | BLENotify); // remote clients will be able to get notifications if this characteristic changes                      

void setup() {
  Serial.begin(9600);    // initialize serial communication
  pinMode(LED_BUILTIN, OUTPUT);   // initialize the LED on pin 13 to indicate when a central is connected
 
  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");

    while (1);
  }
  Serial.println("Ardunio BLE address is [" + BLE.address() + "]");
  
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  Serial.print("Accelerometer sample rate = ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println(" Hz");
  Serial.print("Gyroscopr sample rate = ");
  Serial.print(IMU.gyroscopeSampleRate());
  Serial.println(" Hz");
  
  /* Set a local name for the BLE device
     This name will appear in advertising packets
     and can be used by remote devices to identify this BLE device
     The name can be changed but maybe be truncated based on space left in advertisement packet */
  BLE.setLocalName("HeartRateSketch");
  BLE.setAdvertisedService(heartRateService); 
  heartRateService.addCharacteristic(heartRateChar); // add the Heart Rate Measurement characteristic
  BLE.addService(heartRateService);   // Add the BLE Heart Rate service

  // advertise to the world so we can be found
  BLE.advertise();
  Serial.println("Bluetooth heart-rate device active, waiting for connections...");

  // register new connection handler
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  // register disconnect handler
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
}

void loop() {
  // listen for BLE peripherals to connect:
  BLEDevice central = BLE.central();

  // if a central is connected to peripheral:
  if (central) {
    Serial.print("Connected to central at address: ");
    // print the central's MAC address:
    Serial.println(central.address());

    // check the heart rate measurement every 200ms
    // as long as the central is still connected:
    while (central.connected()) {
        Serial.println("Still connected");
        if (IMU.accelerationAvailable()) {
          char buff[32]; 
          float ax, ay, az;     
          IMU.readAcceleration(ax, ay, az);
          snprintf (buff, sizeof(buff), "%f %f %f", ax * 1000, ay * 1000, az * 1000);
          Serial.println(buff);
          heartRateChar.writeValue(ax);
        } 
        updateHeartRate();
    }
  }
}

void blePeripheralConnectHandler(BLEDevice central) {
  // central connected event handler
  Serial.print("Connected event, central address: ");
  Serial.println(central.address());
  digitalWrite(LED_BUILTIN, HIGH);    // indicate that we have a connection
}

void blePeripheralDisconnectHandler(BLEDevice central) {
  // central disconnected event handler
  Serial.print("Disconnected event, central address: ");
  Serial.println(central.address());
  digitalWrite(LED_BUILTIN, LOW);     // indicate that we no longer have a connection
}

void updateHeartRate() {
  /* Read the current voltage level on the A0 analog input pin.
     This is used here to simulate the heart rate's measurement.
  */
  /*int heartRateMeasurement = analogRead(A0);
  int heartRate = map(heartRateMeasurement, 0, 1023, 0, 100);
  if (heartRate != oldHeartRate) {      // if the heart rate has changed
    Serial.print("Heart Rate is now: "); // print it
    Serial.println(heartRate);
    const unsigned char heartRateCharArray[2] = { 0, (char)heartRate };
    heartRateChar.setValue(heartRateCharArray, 2);  // and update the heart rate measurement characteristic
    oldHeartRate = heartRate;           // save the level for next comparison
      }
    */
}
