// #include <Arduino.h>
// #include <Wire.h>
// #include <SPI.h>
// #include <Adafruit_Sensor.h>
// #include <Adafruit_LIS3DH.h>

// Adafruit_LIS3DH lis = Adafruit_LIS3DH();

// void setup() {
//   // put your setup code here, to run once:
//   Serial.begin(115200);
//   while (!Serial) {
//     delay(10); // will pause Zero, Leonardo, etc until serial console opens
//   }

//   Serial.println("Adafruit LIS3DH test!");

//   if (!lis.begin(0x18)) { // change this to 0x19 for alternative i2c address
//     Serial.println("Couldnt start");
//     while (1) yield();
//   }
//   Serial.println("LIS3DH found!");

//   lis.setRange(LIS3DH_RANGE_2_G); // 2, 4, 8 or 16 G!

//   Serial.print("Range = ");
//   switch (lis.getRange()) {
//   case LIS3DH_RANGE_2_G:
//     Serial.println("+-2G");
//     break;
//   case LIS3DH_RANGE_4_G:
//     Serial.println("+-4G");
//     break;
//   case LIS3DH_RANGE_8_G:
//     Serial.println("+-8G");
//     break;
//   case LIS3DH_RANGE_16_G:
//     Serial.println("+-16G");
//     break;
//   }
// }

// void loop() { 
//   lis.read();      // get X Y and Z data at once
//   sensors_event_t event;
//   lis.getEvent(&event);

//   //Serial.print(">X: "); Serial.print(lis.x); Serial.print("\t");
//   //Serial.print(">Y: "); Serial.print(lis.y); Serial.print("\t");
//   //Serial.print(">Z: "); Serial.print(lis.z); Serial.print("\t");

//   Serial.print(">X_g: "); Serial.print(lis.x_g); Serial.print("\r\n");
//   Serial.print(">Y_g: "); Serial.print(lis.y_g); Serial.print("\r\n");
//   Serial.print(">Z_g: "); Serial.println(lis.z_g); Serial.print("\r\n");

//   delay(20);
// }
