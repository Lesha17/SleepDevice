/*
    This sketch configures HX711 chip, measures weight data from it, then sends it to BLE client.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "HX711.h"

#define DOUT  5
#define CLK  4

// https://www.uuidgenerator.net/ UUID generator is used
#define SERVICE_UUID                         "a22b1352-4007-11e8-b467-0ed5f89f718b" // service UUID
#define CHARACTERISTIC_UUID_SENSOR_VALUE     "a22b15dc-4007-11e8-b467-0ed5f89f718b"
#define CHARACTERISTIC_UUID_ON_BED_VALUE     "a22b1730-4007-11e8-b467-0ed5f89f718b"
#define CHARACTERISTIC_UUID_NOT_ON_BED_VALUE "a22b1852-4007-11e8-b467-0ed5f89f718b"

HX711 * scale;
float calibration_factor = -7050; //-7050 worked for my 440lb max scale setup

BLECharacteristic *sensorValueCharacteristic;
BLECharacteristic *onBedSensorValueCharacteristic;
BLECharacteristic *notOnBedSensorValueCharacteristic;
bool deviceConnected = false;

typedef union
{
 float number;
 uint8_t bytes[4];
} FLOATUNION_t;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("*Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("*Device disconnected");
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");

        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }
        
        Serial.println("*********");
      }
    }
};

void setup() {
  Serial.begin(115200);

  scale = new HX711(DOUT, CLK);

  scale->set_scale();
  scale->tare(); //Reset the scale to 0

  long zero_factor = scale->read_average(); //Get a baseline reading
  Serial.print("Zero factor: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);

  BLEDevice::init("Sleep monitoring device"); // Give it a name

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Sensor value characteristic
  sensorValueCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_SENSOR_VALUE,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  sensorValueCharacteristic->addDescriptor(new BLE2902());

  // OnBed sensor value characteristic
  onBedSensorValueCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_ON_BED_VALUE,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                    );     
  onBedSensorValueCharacteristic->addDescriptor(new BLE2902());
  onBedSensorValueCharacteristic->setCallbacks(new MyCallbacks());

// NotOnBed sensor value characteristic
  notOnBedSensorValueCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_NOT_ON_BED_VALUE,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                    );     
  notOnBedSensorValueCharacteristic->addDescriptor(new BLE2902());
  notOnBedSensorValueCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  if (deviceConnected) {
  Serial.print("Reading: ");
  float scale_units =scale->get_units();
  Serial.print(scale_units, 1);
  Serial.print(" lbs"); //Change this to kg and re-adjust the calibration factor if you follow SI units like a sane person
  Serial.print(" calibration_factor: ");
  Serial.print(calibration_factor);
  Serial.println();
  
    // Let's convert the value to a char array:
    FLOATUNION_t txValue;
    txValue.number = scale_units; // Assign a number to the float
    
    sensorValueCharacteristic->setValue(txValue.bytes, 4);  
    sensorValueCharacteristic->notify(); // Send the value to the app!
    
    Serial.print("*** Sent Value: ");
    Serial.print(txValue.number);
    Serial.println(" ***");

    // You can add the rxValue checks down here instead
    // if you set "rxValue" as a global var at the top!
    // Note you will have to delete "std::string" declaration
    // of "rxValue" in the callback function.
//    if (rxValue.find("A") != -1) { 
//      Serial.println("Turning ON!");
//      digitalWrite(LED, HIGH);
//    }
//    else if (rxValue.find("B") != -1) {
//      Serial.println("Turning OFF!");
//      digitalWrite(LED, LOW);
//    }
  }
  delay(1000);
}
