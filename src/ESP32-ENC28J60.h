/*
 ENC28J60-ETH.h - ETH PHY support for ENC28J60
 Based on ETH.h from Arduino-esp32 and ENC28J60 component from esp-idf.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _ENC28J60_ESP32_ETH_H_
#define _ENC28J60_ESP32_ETH_H_

#include "WiFi.h"
#include "esp_system.h"
#include "esp_eth.h"

#if ESP_IDF_VERSION_MAJOR < 4 || ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4,4,0)
  #error "This version of Arduino is too old"
#endif


class ENC28J60Class {
    private:
        bool initialized;
        bool staticIP;

        esp_eth_handle_t eth_handle;

    protected:
        bool started;
        eth_link_t eth_link;
        static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    public:
        ENC28J60Class();
        ~ENC28J60Class();

        bool begin(int MISO_GPIO, int MOSI_GPIO, int SCLK_GPIO, int CS_GPIO, int INT_GPIO, int SPI_CLOCK_MHZ, int SPI_HOST, bool use_mac_from_efuse=false);

        bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);

        const char * getHostname();
        bool setHostname(const char * hostname);

        bool fullDuplex();
        bool linkUp();
        uint8_t linkSpeed();

        bool enableIpV6();
        IPv6Address localIPv6();

        IPAddress localIP();
        IPAddress subnetMask();
        IPAddress gatewayIP();
        IPAddress dnsIP(uint8_t dns_no = 0);

        IPAddress broadcastIP();
        IPAddress networkID();
        uint8_t subnetCIDR();

        uint8_t * macAddress(uint8_t* mac);
        String macAddress();

        friend class WiFiClient;
        friend class WiFiServer;
};

extern ENC28J60Class ETH;

#endif /* _ETH_H_ */
