#include "everblu_meters.h"

// Project source : 
// http://www.lamaisonsimon.fr/wiki/doku.php?id=maison2:compteur_d_eau:compteur_d_eau

// Require EspMQTTClient library (by Patrick Lapointe) version 1.13.3
// Install from Arduino library manager (and its dependancies)
// https://github.com/plapointe6/EspMQTTClient/releases/tag/1.13.3
#include "EspMQTTClient.h"

// Edit "everblu_meters.h" file then change the define at the end of the file

#ifndef LED_BUILTIN
// Change this pin if needed
#define LED_BUILTIN 2
#endif


EspMQTTClient mqtt(
  "MyESSID",            // Your Wifi SSID
  "MyWiFiKey",          // Your WiFi key
  "mqtt.server.com",    // MQTT Broker server ip
  "MQTTUsername",       // Can be omitted if not needed
  "MQTTPassword",       // Can be omitted if not needed
  "EverblueCyble",      // Client name that uniquely identify your device
  1883                  // MQTT Broker server port
);

char *jsonTemplate = 
"{                    \
\"liters\": %d,       \
\"counter\" : %d,     \
\"battery\" : %d,     \
\"timestamp\" : %ld   \
}";

void onUpdateData()
{
  // Call back this function in 24 hours (in miliseconds)
  mqtt.executeDelayed(1000 * 60 * 60 * 24, onUpdateData);

  struct tmeter_data meter_data;
  meter_data = get_meter_data();

  time_t tnow = time(nullptr);
  struct tm *ptm = gmtime(&tnow);
  Serial.printf("Current date (UTC) : %04d/%02d/%02d %02d:%02d:%02d - %s\n", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, String(tnow, DEC).c_str());

  if (meter_data.reads_counter == 0 && meter_data.liters == 0) {
    Serial.println("Unable to retrieve data from meter. Retry later...");
    // Call back this function in 1 hour (in miliseconds)
    mqtt.executeDelayed(1000 * 60 * 60 * 1, onUpdateData);
    return;
  }

  digitalWrite(LED_BUILTIN, LOW); // turned on

  Serial.printf("Liters : %d\nBattery (in months) : %d\nCounter : %d\n\n", meter_data.liters, meter_data.battery_left, meter_data.reads_counter);

  mqtt.publish("everblu/cyble/liters", String(meter_data.liters, DEC), true);
  delay(50); // Do not remove
  mqtt.publish("everblu/cyble/counter", String(meter_data.reads_counter, DEC), true);
  delay(50); // Do not remove
  mqtt.publish("everblu/cyble/battery", String(meter_data.battery_left, DEC), true);
  delay(50); // Do not remove
  mqtt.publish("everblu/cyble/timestamp", String(tnow, DEC), true); // timestamp since epoch in UTC
  delay(50); // Do not remove

  char json[512];
  sprintf(json, jsonTemplate, meter_data.liters, meter_data.reads_counter, meter_data.battery_left, tnow);
  mqtt.publish("everblu/cyble/json", json, true); // send all data as a json message
}


String jsonDiscoveryDevice1(
"{ \
  \"name\": \"Compteur Eau Index\", \
  \"unique_id\": \"water_meter_value\",\
  \"object_id\": \"water_meter_value\",\
  \"icon\": \"mdi:water\",\
  \"unit_of_measurement\": \"Litres\",\
  \"qos\": \"0\",\
  \"state_topic\": \"everblu/cyble/liters\",\
  \"force_update\": \"true\",\
  \"device\" : {\
  \"identifiers\" : [\
  \"14071984\" ],\
  \"name\": \"Compteur Eau\",\
  \"model\": \"Everblu Cyble ESP8266/ESP32\",\
  \"manufacturer\": \"Psykokwak\",\
  \"suggested_area\": \"Home\"}\
}");

String jsonDiscoveryDevice2(
"{ \
  \"name\": \"Compteur Eau Batterie\", \
  \"unique_id\": \"water_meter_battery\",\
  \"object_id\": \"water_meter_battery\",\
  \"device_class\": \"battery\",\
  \"icon\": \"mdi:battery\",\
  \"unit_of_measurement\": \"%\",\
  \"qos\": \"0\",\
  \"state_topic\": \"everblu/cyble/battery\",\
  \"value_template\": \"{{ [(value|int), 100] | min }}\",\
  \"force_update\": \"true\",\
  \"device\" : {\
  \"identifiers\" : [\
  \"14071984\" ],\
  \"name\": \"Compteur Eau\",\
  \"model\": \"Everblu Cyble ESP8266/ESP32\",\
  \"manufacturer\": \"Psykokwak\",\
  \"suggested_area\": \"Home\"}\
}");

String jsonDiscoveryDevice3(
"{ \
  \"name\": \"Compteur Eau Compteur\", \
  \"unique_id\": \"water_meter_counter\",\
  \"object_id\": \"water_meter_counter\",\
  \"icon\": \"mdi:counter\",\
  \"qos\": \"0\",\
  \"state_topic\": \"everblu/cyble/counter\",\
  \"force_update\": \"true\",\
  \"device\" : {\
  \"identifiers\" : [\
  \"14071984\" ],\
  \"name\": \"Compteur Eau\",\
  \"model\": \"Everblu Cyble ESP8266/ESP32\",\
  \"manufacturer\": \"Psykokwak\",\
  \"suggested_area\": \"Home\"}\
}");

void onConnectionEstablished()
{
  Serial.println("Connected to MQTT Broker :)");

  Serial.println("Configure time from NTP server.");
  configTzTime("UTC0", "pool.ntp.org");

  mqtt.subscribe("everblu/cyble/trigger", [](const String& message) {
    if (message.length() > 0)
      onUpdateData();
  });

  Serial.println("Send MQTT config for HA.");
  // Auto discovery
  delay(50); // Do not remove
  mqtt.publish("homeassistant/sensor/water_meter_value/config", jsonDiscoveryDevice1, true);
  delay(50); // Do not remove
  mqtt.publish("homeassistant/sensor/water_meter_battery/config", jsonDiscoveryDevice2, true);
  delay(50); // Do not remove
  mqtt.publish("homeassistant/sensor/water_meter_counter/config", jsonDiscoveryDevice3, true);
  delay(50); // Do not remove

  onUpdateData();
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // turned off

  mqtt.setMaxPacketSize(1024);
  //mqtt.enableDebuggingMessages(true);

  /*
  // Use this piece of code to find the right frequency.
  for (float i = 433.76f; i < 433.890f; i += 0.0005f) {
    Serial.printf("Test frequency : %f\n", i);
    cc1101_init(i);

    struct tmeter_data meter_data;
    meter_data = get_meter_data();

    if (meter_data.reads_counter != 0 || meter_data.liters != 0) {
      Serial.printf("\n------------------------------\nGot frequency : %f\n------------------------------\n", i);

      Serial.printf("Liters : %d\nBattery (in months) : %d\nCounter : %d\n\n", meter_data.liters, meter_data.battery_left, meter_data.reads_counter);

      digitalWrite(LED_BUILTIN, LOW); // turned on

      while (42);
    }
  }
  */



  cc1101_init(FREQUENCY);

  /*
  // Use this piece of code to test
  struct tmeter_data meter_data;
  meter_data = get_meter_data();
  Serial.printf("\nLiters : %d\nBattery (in months) : %d\nCounter : %d\nTime start : %d\nTime end : %d\n\n", meter_data.liters, meter_data.battery_left, meter_data.reads_counter, meter_data.time_start, meter_data.time_end);
  while (42);
  */
}

void loop()
{
  mqtt.loop(); 
}
