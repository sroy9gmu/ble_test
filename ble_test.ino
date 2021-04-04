#include <ArduinoBLE.h>
#include <Arduino_LSM6DS3.h>

BLEService heartRateService("8da11f6d-0a78-4c3a-8a83-941f7c1d064b"); // BLE Heart Rate Service
// BLE Heart Rate Measurement Characteristic
BLEIntCharacteristic heartRateChar("3b0ef782-9b04-4fef-a3e8-c44f10f0f661",  // standard 16-bit characteristic UUID
    BLERead | BLENotify);  // remote clients will be able to get notifications if this characteristic changes

int oldHeartRate = 0;  // last heart rate reading from analog input

void setup() {
  Serial.begin(9600);    // initialize serial communication
  pinMode(LED_BUILTIN, OUTPUT);   // initialize the LED on pin 13 to indicate when a central is connected
 
  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");

    while (1);
  }
    
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  Serial.print("Accelerometer sample rate = ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println(" Hz");
  Serial.print("Gyroscope sample rate = ");
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
    // check the heart rate measurement every 200ms
    // as long as the central is still connected:
    while (central.connected()) {
        float ax = 0, ay, az;

        Serial.println("Arduino Peripheral address is [" + BLE.address() + "]");
        Serial.println("Smartphone Central address is [" + central.address() + "]");
        
        if (IMU.accelerationAvailable()) {               
          IMU.readAcceleration(ax, ay, az);          
        }
        updateHeartRate((int)(ax * 30000));
        delay(1000);         
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

void updateHeartRate(int heartRate) {
  /* Read the current voltage level on the A0 analog input pin.
     This is used here to simulate the heart rate's measurement.
  */
  Serial.print("Heart Rate is now: "); // print it
  Serial.println(heartRate);
  if (heartRate != oldHeartRate) {      // if the heart rate has changed    
    heartRateChar.writeValue(heartRate);
    oldHeartRate = heartRate;           // save the level for next comparison
    Serial.println("Heart Rate updated ");
  }
}
