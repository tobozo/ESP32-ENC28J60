/*
    This sketch shows the Ethernet event usage for ENC28J60

*/

#include <ESP32-ENC28J60.h>

#define SPI_HOST       1
#define SPI_CLOCK_MHZ  8
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


static bool eth_connected = false;


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

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

HTTPClient http;
WiFiClientSecure client;


void testClient(const char * url)
{
  Serial.printf("\nConnecting to %s\n", url);

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if( String(url).startsWith("https") ) {
    client.setInsecure();
    http.begin( client, url );
  } else {
    http.begin( url );
  }

  int httpCode = http.GET();

  if( httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY ) {
    Serial.printf("Error: Status %i\n", httpCode );
  }

  Stream* stream = http.getStreamPtr();

  while (stream->available()) {
    Serial.write(stream->read());
  }

  Serial.println("closing connection\n");
  http.end();
}


void setup()
{
  Serial.begin( 115200 );
  WiFi.onEvent( WiFiEvent );
  ETH.begin( MISO_GPIO, MOSI_GPIO, SCLK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, SPI_HOST );

  while( !eth_connected) {
    Serial.println("Connecting...");
    delay( 1000 );
  }

  testClient("https://google.com");

}


void loop()
{
}
