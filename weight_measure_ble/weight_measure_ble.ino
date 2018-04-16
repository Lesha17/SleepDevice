/*
    This sketch configures HX711 chip, measures weight data from it, then sends it to BLE client.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "HX711.h"
#include "EEPROM.h"

#define DOUT  5
#define CLK  4

#define CALIBRATION_FACTOR 4750.0
#define WEIGHT_OFFSET 230850.0

// https://www.uuidgenerator.net/ UUID generator is used
#define SERVICE_UUID                         "a22b1352-4007-11e8-b467-0ed5f89f718b" // service UUID
#define CHARACTERISTIC_UUID_SENSOR_VALUE     "a22b15dc-4007-11e8-b467-0ed5f89f718b"
#define CHARACTERISTIC_UUID_ON_BED_VALUE     "a22b1730-4007-11e8-b467-0ed5f89f718b"
#define CHARACTERISTIC_UUID_NOT_ON_BED_VALUE "a22b1852-4007-11e8-b467-0ed5f89f718b"

#define EEPROM_SIZE 16
#define ON_BED_EEPROM_ADDRESS 0
#define ON_BED_EEPROM_INITILIZED_ADDRESS 4
#define NOT_ON_BED_EEPROM_ADDRESS 8
#define NOT_ON_BED_EEPROM_INITILIZED_ADDRESS 12


HX711 * scale;

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
    int eepRomAddr;

public:
    MyCallbacks(int eepRomAddr) : eepRomAddr(eepRomAddr) {}
  
    void onWrite(BLECharacteristic *pCharacteristic) {
      FLOATUNION_t value;

      std::string strValue = pCharacteristic->getValue();
      uint8_t * bytes = (uint8_t*)strValue.data();
      for(int i = 0; i < 4 && i < strValue.length(); ++i) {
        value.bytes[i] = bytes[i];
      }

      EEPROM.writeFloat(eepRomAddr, value.number);
      EEPROM.writeInt(eepRomAddr + 4, 1);
      EEPROM.commit();

      if (strValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");

        for (int i = 0; i < strValue.length(); i++) {
          Serial.print(strValue[i]);
        }
        
        Serial.println("*********");
      }
    }
};

void setup() {
  Serial.begin(115200);

  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM"); delay(1000);
  }
  Serial.print("On bed: "); Serial.println(EEPROM.readFloat(ON_BED_EEPROM_ADDRESS));
  Serial.print("On bed intialized: "); Serial.println(EEPROM.readInt(ON_BED_EEPROM_INITILIZED_ADDRESS));
  Serial.print("Not on bed: "); Serial.println(EEPROM.readFloat(NOT_ON_BED_EEPROM_ADDRESS));
  Serial.print("Not on bed initialized: "); Serial.println(EEPROM.readInt(NOT_ON_BED_EEPROM_INITILIZED_ADDRESS));

  scale = new HX711(DOUT, CLK);

  scale->set_offset(WEIGHT_OFFSET);
  scale->set_scale(CALIBRATION_FACTOR);

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
  if(EEPROM.readInt(ON_BED_EEPROM_INITILIZED_ADDRESS) == 1) {
    FLOATUNION_t value;
    value.number = EEPROM.readFloat(ON_BED_EEPROM_ADDRESS);
    onBedSensorValueCharacteristic->setValue(value.bytes, 4);
    Serial.print("Initialized onBedCharacteristic with value "); Serial.println(value.number);
  }
  onBedSensorValueCharacteristic->addDescriptor(new BLE2902());
  MyCallbacks * onBedCallbacks = new MyCallbacks(ON_BED_EEPROM_ADDRESS);
  onBedSensorValueCharacteristic->setCallbacks(onBedCallbacks);

// NotOnBed sensor value characteristic
  notOnBedSensorValueCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_NOT_ON_BED_VALUE,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
                    );     
  if(EEPROM.readInt(NOT_ON_BED_EEPROM_INITILIZED_ADDRESS) == 1) {
    FLOATUNION_t value;
    value.number = EEPROM.readFloat(NOT_ON_BED_EEPROM_ADDRESS);
    notOnBedSensorValueCharacteristic->setValue(value.bytes, 4);
    Serial.print("Initialized notOnBedCharacteristic with value "); Serial.println(value.number);
  }
  notOnBedSensorValueCharacteristic->addDescriptor(new BLE2902());
  MyCallbacks * notOnBedCallbacks = new MyCallbacks(NOT_ON_BED_EEPROM_ADDRESS);
  notOnBedSensorValueCharacteristic->setCallbacks(notOnBedCallbacks);

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  if (deviceConnected) {
    Serial.print("Weight: ");
    float weight =scale->get_units();
    Serial.print(weight, 1);
    Serial.print(" kg"); 
    Serial.println();
  
    // Convert the value to a char array:
    FLOATUNION_t value;
    value.number = weight; // Assign a number to the float
    
    sensorValueCharacteristic->setValue(value.bytes, 4);  
    sensorValueCharacteristic->notify(); // Send the value to the app!
    
    Serial.print("*** Sent Value: ");
    Serial.print(value.number);
    Serial.println(" ***");
   }
   delay(1000);
}
