/**************************************************************************
 Date: 04/03/2021
 This is an example for Serial console reading from TMP36 and Pulse sensors.
 Pulse SIG pin connects to Arduino Nano 33 IoT pin A1.
 TMP36 SIG pin connects to Arduino Nano 33 IoT pin A0. 
 **************************************************************************/
#include <SPI.h>
#include <Wire.h>
#include <FreeRTOS_SAMD21.h>
#include <ArduinoBLE.h>
/* Alternate implemention of temperature sensor reading
    #include <TMP36.h> 
  
    TMP36 myTMP36(A0, 3.3); //Create an instance of the TMP36 class
 */ 

BLEService heartRateService("8da11f6d-0a78-4c3a-8a83-941f7c1d064b"); // BLE Heart Rate Service
// BLE Heart Rate Measurement Characteristic
BLEIntCharacteristic heartRateChar("3b0ef782-9b04-4fef-a3e8-c44f10f0f661",  // standard 16-bit characteristic UUID
    BLERead | BLENotify);  // remote clients will be able to get notifications if this characteristic changes
// BLE Body Temperature Measurement Characteristic
BLEIntCharacteristic bodyTempChar("6fcf1abb-a08e-4dae-a0ad-8b6d65ba27cb",  // standard 16-bit characteristic UUID
    BLERead | BLENotify);  // remote clients will be able to get notifications if this characteristic changes    

TaskHandle_t Handle_aTask;
TaskHandle_t Handle_bTask;
TaskHandle_t Handle_monitorTask;

int bleLED = 20;
int pulsePin = A1;
int pulseLED = 16;
int tempPin = A0;
int tempLED = LED_BUILTIN;
int oldSignal = 0;  // last heart rate reading from analog input
int oldtemperatureF = 0;

int startMillis, count, Signal, temperatureF;

void setup() { 
  pinMode(bleLED,OUTPUT);
  pinMode(tempLED,OUTPUT);
  pinMode(pulseLED,OUTPUT);
  startMillis = millis();

  Wire.begin();
  Serial.begin(9600);

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");

    while (1);
  }

   /* Set a local name for the BLE device
     This name will appear in advertising packets
     and can be used by remote devices to identify this BLE device
     The name can be changed but maybe be truncated based on space left in advertisement packet */
  BLE.setLocalName("HeartRateSketch");
  BLE.setAdvertisedService(heartRateService); 
  heartRateService.addCharacteristic(heartRateChar); // add the Heart Rate Measurement characteristic
  heartRateService.addCharacteristic(bodyTempChar); // add the Body Temperature Measurement characteristic
  BLE.addService(heartRateService);   // Add the BLE Heart Rate service

  // advertise to the world so we can be found
  BLE.advertise();
  Serial.println("Bluetooth heart-rate device active, waiting for connections...");

  // register new connection handler
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  // register disconnect handler
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  delay(1000); // prevents usb driver crash on startup, do not omit this
  while (!Serial) ;  // Wait for serial terminal to open port before starting program 

  // Create the threads that will be managed by the rtos
  // Sets the stack size and priority of each task
  // Also initializes a handler pointer to each task, which are important to communicate with and retrieve info from tasks
  xTaskCreate(threadA,     "Task Pulse",       256, NULL, tskIDLE_PRIORITY + 3, &Handle_aTask);  
  xTaskCreate(threadB,     "Task Temperature", 256, NULL, tskIDLE_PRIORITY + 2, &Handle_bTask); 
  xTaskCreate(taskMonitor, "Task Monitor", 256, NULL, tskIDLE_PRIORITY + 1, &Handle_monitorTask);
   
  // Start the RTOS, this function will never return and will schedule the tasks.
  vTaskStartScheduler();

  // error scheduler failed to start
  // should never get here
  while(1)
  {
    Serial.println("Scheduler Failed!");
    Serial.flush();
    delay(1000);
  } 
}

void myDelayMs(int ms)
{
  vTaskDelay( (ms * 1000) / portTICK_PERIOD_US );  
}

//*****************************************************************
// Task will periodically print out useful information about the tasks running
// Is a useful tool to help figure out stack sizes being used
// Run time stats are generated from all task timing collected since startup
// No easy way yet to clear the run time stats yet
//*****************************************************************
static char ptrTaskList[400]; //temporary string buffer for task stats

void taskMonitor(void *pvParameters)
{
    int x;
    int measurement;
    
    Serial.println("Task Monitor: Started");

    // run this task afew times before exiting forever
    while(1)
    {
      myDelayMs(10000); // print every 10 seconds

      Serial.flush();
      Serial.println("");
      Serial.println("****************************************************");
      Serial.print("Free Heap: ");
      Serial.print(xPortGetFreeHeapSize());
      Serial.println(" bytes");

      Serial.print("Min Heap: ");
      Serial.print(xPortGetMinimumEverFreeHeapSize());
      Serial.println(" bytes");
      Serial.flush();

      Serial.println("****************************************************");
      Serial.println("Task            ABS             %Util");
      Serial.println("****************************************************");

      vTaskGetRunTimeStats(ptrTaskList); //save stats to char array
      Serial.println(ptrTaskList); //prints out already formatted stats
      Serial.flush();

      Serial.println("****************************************************");
      Serial.println("Task            State   Prio    Stack   Num     Core" );
      Serial.println("****************************************************");

      vTaskList(ptrTaskList); //save stats to char array
      Serial.println(ptrTaskList); //prints out already formatted stats
      Serial.flush();

      Serial.println("****************************************************");
      Serial.println("[Stacks Free Bytes Remaining] ");

      measurement = uxTaskGetStackHighWaterMark( Handle_aTask );
      Serial.print("Thread Pulse: ");
      Serial.println(measurement);

      measurement = uxTaskGetStackHighWaterMark( Handle_bTask );
      Serial.print("Thread Temperature: ");
      Serial.println(measurement);

      measurement = uxTaskGetStackHighWaterMark( Handle_monitorTask );
      Serial.print("Monitor Stack: ");
      Serial.println(measurement);

      Serial.println("****************************************************");
      Serial.flush();

    }

    // delete ourselves.
    // Have to call this or the system crashes when you reach the end bracket and then get scheduled.
    Serial.println("Task Monitor: Deleting");
    vTaskDelete( NULL );
}

static void threadA( void *pvParameters ) 
{
  Serial.println("Thread Pulse: Starting");
  
  while(1){
    read_pulse();
  }
  
  // delete ourselves.
  // Have to call this or the system crashes when you reach the end bracket and then get scheduled.
  Serial.println("Thread Pulse: Deleting");
  vTaskDelete( NULL );
}

static void threadB( void *pvParameters ) 
{
  Serial.println("Thread Temperature: Starting");
  
  while(1){
    read_temperature();
  }
  
  // delete ourselves.
  // Have to call this or the system crashes when you reach the end bracket and then get scheduled.
  Serial.println("Thread Temperature: Deleting");
  vTaskDelete( NULL );
}

void read_temperature() {  
  // NOTE: There exists scope for utilizing more stable and accurate sensors
  // getting the voltage reading from the temperature sensor
  int reading = analogRead(tempPin);  
 
  // converting that reading to voltage, for 3.3v arduino use 3.3
  float voltage = reading * 3.3;
  voltage /= 1024.0; 
 
  // now print out the temperature
  float temperatureC = (voltage - 0.5) * 100 ;  //converting from 10 mv per degree wit 500 mV offset
                                               //to degrees ((voltage - 500mV) times 100)
 
  // now convert to Fahrenheit
  temperatureF = int((temperatureC * 9.0 / 5.0) + 32.0);
  Serial.print(temperatureF); Serial.println(" degrees F");

  /* Alternate implemention of temperature sensor reading  
     temperatureF = myTMP36.getTempF();
   */
  
  digitalWrite(tempLED, HIGH);
  myDelayMs(500);   
  digitalWrite(tempLED, LOW); 
  myDelayMs(500);

  // listen for BLE peripherals to connect:
  BLEDevice central = BLE.central();
    
  // if a central is connected to peripheral:
  if (central.connected()) {   
    Serial.print("Body temperature is now: "); // print it
    Serial.println(temperatureF);
    if (oldtemperatureF != temperatureF) {      // if the heart rate has changed    
      bodyTempChar.writeValue(temperatureF);
      oldtemperatureF = temperatureF;           // save the level for next comparison
      Serial.println("Body temperature updated ");
    }
  }                     
}

void read_pulse() {
  int Threshold = 510;

  if(millis() < startMillis + 10000) {  
  Signal = analogRead(pulsePin);  // Read the PulseSensor's value.
                                  // Assign this value to the "Signal" variable.
  Serial.println(Signal);       // Send the Signal value to Serial Plotter.

  if(Signal > Threshold){        
     count++;
     digitalWrite (pulseLED,HIGH);// If the signal is above "Threshold", then "turn-on" Arduino's on-Board LED.
     myDelayMs(500);
     digitalWrite (pulseLED,LOW);//  Else, the sigal must be below "Threshold", so "turn-off" this LED.
     myDelayMs(500);
    }   
  }
  else {
  Serial.println("BPM  "+ String(count * 6));
  
  startMillis = millis();
  count = 0;
  }

  // listen for BLE peripherals to connect:
  BLEDevice central = BLE.central();
    
  // if a central is connected to peripheral:
  if (central.connected()) {   
    Serial.print("Heart rate is now: "); // print it
    Serial.println(Signal);
    if (oldSignal != Signal) {      // if the heart rate has changed    
      heartRateChar.writeValue(Signal);
      oldSignal = Signal;           // save the level for next comparison
      Serial.println("Heart rate updated ");
    }
  }
}

void blePeripheralConnectHandler(BLEDevice central) {
  // central connected event handler
  Serial.print("Connected event, central address: ");
  Serial.println(central.address());
  digitalWrite(bleLED, HIGH);    // indicate that we have a connection
}

void blePeripheralDisconnectHandler(BLEDevice central) {
  // central disconnected event handler
  Serial.print("Disconnected event, central address: ");
  Serial.println(central.address());
  digitalWrite(bleLED, LOW);     // indicate that we no longer have a connection
}

void loop() {}
