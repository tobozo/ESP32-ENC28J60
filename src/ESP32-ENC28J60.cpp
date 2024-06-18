/*
 ESP32-ENC28J60.h - ETH PHY support for ENC28J60
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

#include "ESP32-ENC28J60.h"

#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_mac.h"

#if __has_include("soc/emac_ext_struct.h")
  #include "soc/emac_ext_struct.h"
#else
  #define SOC_HAS_NO_EMAC
  #include "extmod/emac_ext_struct.h"
#endif

#include "soc/rtc.h"
extern "C"
{
  esp_eth_mac_t* enc28j60_begin(int MISO_GPIO, int MOSI_GPIO, int SCLK_GPIO, int CS_GPIO, int INT_GPIO, int SPI_CLOCK_MHZ, int SPI_HOST);
  #include "extmod/esp_eth_enc28j60.h"
}


#include "lwip/err.h"
#include "lwip/dns.h"

extern void tcpipInit();

/**
* @brief Callback function invoked when lowlevel initialization is finished
*
* @param[in] eth_handle: handle of Ethernet driver
*
* @return
*       - ESP_OK: process extra lowlevel initialization successfully
*       - ESP_FAIL: error occurred when processing extra lowlevel initialization
*/

//static eth_clock_mode_t eth_clock_mode = ETH_CLK_MODE;

/**
* @brief Callback function invoked when lowlevel deinitialization is finished
*
* @param[in] eth_handle: handle of Ethernet driver
*
* @return
*       - ESP_OK: process extra lowlevel deinitialization successfully
*       - ESP_FAIL: error occurred when processing extra lowlevel deinitialization
*/
//static esp_err_t on_lowlevel_deinit_done(esp_eth_handle_t eth_handle){
//    return ESP_OK;
//}





ENC28J60Class::ENC28J60Class()
  : initialized(false), staticIP(false), eth_handle(NULL), started(false), eth_link(ETH_LINK_DOWN) {
}

ENC28J60Class::~ENC28J60Class() {}
esp_netif_t *eth_netif{nullptr};
//bool ENC28J60Class::begin(uint8_t phy_addr, int power, int mdc, int mdio, eth_phy_type_t type, eth_clock_mode_t clock_mode, bool use_mac_from_efuse)
bool ENC28J60Class::begin(int MISO_GPIO, int MOSI_GPIO, int SCLK_GPIO, int CS_GPIO, int INT_GPIO, int SPI_CLOCK_MHZ, int SPI_HOST, uint8_t* ENC28J60_Mac) {
  Network.begin();

  //uint8_t ENC28J60_Default_Mac[6] = { 0x02, 0x00, 0x00, 0x12, 0x34, 0x56 };

  if (esp_read_mac(mac_eth, ESP_MAC_ETH) == ESP_OK) {
    char macStr[18] = { 0 };

    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac_eth[0], mac_eth[1], mac_eth[2],
            mac_eth[3], mac_eth[4], mac_eth[5]);

    log_i("Using built-in mac_eth = %s", macStr);

    esp_base_mac_addr_set(mac_eth);
  } else {
    log_i("Using user mac_eth");
    memcpy(mac_eth, ENC28J60_Mac, sizeof(mac_eth));

    esp_base_mac_addr_set(ENC28J60_Mac);
  }


  esp_netif_init();                                  // Initialize TCP/IP network interface (should be called only once in application)
  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();  // apply default network interface configuration for Ethernet
  eth_netif = esp_netif_new(&cfg);      // create network interface for Ethernet driver

  esp_eth_mac_t* eth_mac = enc28j60_begin(MISO_GPIO, MOSI_GPIO, SCLK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, SPI_HOST);

  if (eth_mac == NULL) {
    log_e("esp_eth_mac_new_esp32 failed");
    return false;
  }

  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  phy_config.autonego_timeout_ms = 0;  // ENC28J60 doesn't support auto-negotiation
  phy_config.reset_gpio_num = -1;      // ENC28J60 doesn't have a pin to reset internal PHY
  esp_eth_phy_t* eth_phy = esp_eth_phy_new_enc28j60(&phy_config);

  if (eth_phy == NULL) {
    log_e("esp_eth_phy_new failed");
    return false;
  }

  eth_handle = NULL;
  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(eth_mac, eth_phy);
  //eth_config.on_lowlevel_init_done = on_lowlevel_init_done;
  //eth_config.on_lowlevel_deinit_done = on_lowlevel_deinit_done;
  if (esp_eth_driver_install(&eth_config, &eth_handle) != ESP_OK || eth_handle == NULL) {
    log_e("esp_eth_driver_install failed");
    return false;
  }

  /* ENC28J60 doesn't burn any factory MAC address, we need to set it manually.
	   02:00:00 is a Locally Administered OUI range so should not be used except when testing on a LAN under your control.
	*/
  //eth_mac->set_addr(eth_mac, ENC28J60_Default_Mac);
  eth_mac->set_addr(eth_mac, mac_eth);
  // ENC28J60 Errata #1 check
  if (emac_enc28j60_get_chip_info(eth_mac) < ENC28J60_REV_B5 && SPI_CLOCK_MHZ < 8) {
    log_e("SPI frequency must be at least 8 MHz for chip revision less than 5");
    ESP_ERROR_CHECK(ESP_FAIL);
  }

  /* attach Ethernet driver to TCP/IP stack */
  if (esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)) != ESP_OK) {
    log_e("esp_netif_attach failed");
    return false;
  }

  if (esp_eth_start(eth_handle) != ESP_OK) {
    log_e("esp_eth_start failed");
    return false;
  }

  // holds a few microseconds to let DHCP start and enter into a good state
  // FIX ME -- adresses issue https://github.com/espressif/arduino-esp32/issues/5733
  delay(50);

  return true;
}

bool ENC28J60Class::config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1, IPAddress dns2) {
  esp_err_t err = ESP_OK;
  esp_netif_ip_info_t info;

  if (static_cast<uint32_t>(local_ip) != 0) {
    info.ip.addr = static_cast<uint32_t>(local_ip);
    info.gw.addr = static_cast<uint32_t>(gateway);
    info.netmask.addr = static_cast<uint32_t>(subnet);
  } else {
    info.ip.addr = 0;
    info.gw.addr = 0;
    info.netmask.addr = 0;
  }

  err = esp_netif_dhcpc_stop(eth_netif);
  if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
    log_e("DHCP could not be stopped! Error: %d", err);
    return false;
  }

  err = esp_netif_set_ip_info(eth_netif, &info);
  if (err != ERR_OK) {
    log_e("STA IP could not be configured! Error: %d", err);
    return false;
  }

  if (info.ip.addr) {
    staticIP = true;
  } else {
    err = esp_netif_dhcpc_start(eth_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
      log_w("DHCP could not be started! Error: %d", err);
      return false;
    }
    staticIP = false;
  }

  ip_addr_t d;
  d.type = IPADDR_TYPE_V4;

  if (static_cast<uint32_t>(dns1) != 0) {
    // Set DNS1-Server
    d.u_addr.ip4.addr = static_cast<uint32_t>(dns1);
    dns_setserver(0, &d);
  }

  if (static_cast<uint32_t>(dns2) != 0) {
    // Set DNS2-Server
    d.u_addr.ip4.addr = static_cast<uint32_t>(dns2);
    dns_setserver(1, &d);
  }

  return true;
}

IPAddress ENC28J60Class::localIP() {
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(eth_netif, &ip)) {
    return IPAddress();
  }
  return IPAddress(ip.ip.addr);
}

IPAddress ENC28J60Class::subnetMask() {
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(eth_netif, &ip)) {
    return IPAddress();
  }
  return IPAddress(ip.netmask.addr);
}

IPAddress ENC28J60Class::gatewayIP() {
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(eth_netif, &ip)) {
    return IPAddress();
  }
  return IPAddress(ip.gw.addr);
}

IPAddress ENC28J60Class::dnsIP(uint8_t dns_no) {
  const ip_addr_t* dns_ip = dns_getserver(dns_no);
  return IPAddress(dns_ip->u_addr.ip4.addr);
}

IPAddress ENC28J60Class::broadcastIP() {
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(eth_netif, &ip)) {
    return IPAddress();
  }
  return WiFiGenericClass::calculateBroadcast(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

IPAddress ENC28J60Class::networkID() {
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(eth_netif, &ip)) {
    return IPAddress();
  }
  return WiFiGenericClass::calculateNetworkID(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

uint8_t ENC28J60Class::subnetCIDR() {
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(eth_netif, &ip)) {
    return (uint8_t)0;
  }
  return WiFiGenericClass::calculateSubnetCIDR(IPAddress(ip.netmask.addr));
}

const char* ENC28J60Class::getHostname() {
  const char* hostname;
  if (esp_netif_get_hostname(eth_netif, &hostname)) {
    return NULL;
  }
  return hostname;
}

bool ENC28J60Class::setHostname(const char* hostname) {
  return esp_netif_set_hostname(eth_netif, hostname) == 0;
}

bool ENC28J60Class::fullDuplex() {
  return true;  //todo: do not see an API for this
}

bool ENC28J60Class::linkUp() {
  return eth_link == ETH_LINK_UP;
}

uint8_t ENC28J60Class::linkSpeed() {
  eth_speed_t link_speed;
  esp_eth_ioctl(eth_handle, ETH_CMD_G_SPEED, &link_speed);
  return (link_speed == ETH_SPEED_10M) ? 10 : 100;
}

bool ENC28J60Class::enableIpV6() {
  return esp_netif_create_ip6_linklocal(eth_netif) == 0;
}

IPAddress ENC28J60Class::localIPv6() {
  static esp_ip6_addr_t addr;
  if (esp_netif_get_ip6_linklocal(eth_netif, &addr)) {
    return IPAddress(IPv6);
  }
  return IPAddress(IPv6, (const uint8_t*)addr.addr, addr.zone);
}

uint8_t* ENC28J60Class::macAddress(uint8_t* mac) {
  if (!mac) {
    return NULL;
  }
  esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac);
  return mac;
}

String ENC28J60Class::macAddress(void) {
  uint8_t mac[6] = { 0, 0, 0, 0, 0, 0 };
  char macStr[18] = { 0 };
  macAddress(mac);
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

ENC28J60Class ETH;
