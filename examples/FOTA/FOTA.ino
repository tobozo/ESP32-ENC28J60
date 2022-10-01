/*
    This sketch shows the Ethernet event usage for ENC28J60

*/

#include <ESP32-ENC28J60.h>

#define SPI_HOST       1
#define SPI_CLOCK_MHZ  20
#define INT_GPIO       4
//
#if CONFIG_IDF_TARGET_ESP32
    #define MISO_GPIO 12
    #define MOSI_GPIO 13
    #define SCLK_GPIO 14
    #define CS_GPIO   15
#elif CONFIG_IDF_TARGET_ESP32C3
    #define MISO_GPIO  2
    #define MOSI_GPIO  7
    #define SCLK_GPIO  6
    #define CS_GPIO   10
#else
    #define MISO_GPIO 13
    #define MOSI_GPIO 11
    #define SCLK_GPIO 12
    #define CS_GPIO   10
#endif


#include <ESP32-targz.h>
#include <esp32FOTA.hpp> // https://github.com/chrisjoyce911/esp32FOTA
#include <debug/test_fota_common.h>

// esp32fota settings
int firmware_version_major  = 1;
int firmware_version_minor  = 1;
int firmware_version_patch  = 0;

#if !defined FOTA_URL
  #define FOTA_URL "https://phpsecu.re/esp32/esp32fota/firmware.json"
#endif

const char* firmware_name   = "esp32-fota-http";
const bool check_signature  = false;
const bool disable_security = true;
// for debug only
const char* title           = "1.1";
const char* description     = "Basic Ethernet example with no security and no filesystem";

esp32FOTA FOTA;

static bool eth_connected = false;

static bool EthernetConnected()
{
  return eth_connected;
}


// using WiFiEvent to handle Ethernet events :-)
void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}


void setup()
{
  Serial.begin(115200);

  PrintFOTAInfo();

  {
    auto cfg = FOTA.getConfig();
    cfg.name         = firmware_name;
    cfg.manifest_url = FOTA_URL;
    cfg.sem          = SemverClass( firmware_version_major, firmware_version_minor, firmware_version_patch );
    cfg.check_sig    = check_signature;
    cfg.unsafe       = disable_security;
    //cfg.root_ca      = MyRootCA;
    //cfg.pub_key      = MyRSAKey;
    FOTA.setConfig( cfg );
    FOTA.setStatusChecker( EthernetConnected );
    //GzUpdateClass::getInstance().useDict( true );
  }

  WiFi.onEvent( WiFiEvent );
  ETH.begin( MISO_GPIO, MOSI_GPIO, SCLK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, SPI_HOST );

}


void loop()
{
  if( !eth_connected) {
    Serial.println("Connecting...");
    delay(1000);
    return;
  }
  FOTA.handle();
  delay(20000);
}

