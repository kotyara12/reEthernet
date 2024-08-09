#include "reEthernet.h"

#if defined(CONFIG_ETH_ENABLED) && (CONFIG_ETH_ENABLED == 1)

#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "rLog.h"
#include "reEsp32.h"
#include "reEvents.h"

static const char* logTAG = "ETH";

// -----------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------- Low level init ----------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static esp_eth_mac_t *_mac = nullptr;
static esp_eth_phy_t *_phy = nullptr;
static esp_netif_t *_eth_netif = nullptr;
static esp_eth_netif_glue_handle_t _eth_netif_glue = nullptr;
static esp_eth_handle_t _eth_handle = nullptr;

esp_err_t ethernetInit()
{
  esp_err_t ret = ESP_OK;

  // Init common MAC and PHY configs to default
  eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

  // Update PHY config based on board specific configuration
  phy_config.phy_addr = CONFIG_ETH_PHY_ADDR;
  phy_config.reset_gpio_num = CONFIG_ETH_GPIO_POWER_PIN;

  // Init vendor specific MAC config to default
  eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

  // Update vendor specific MAC config based on board configuration
  esp32_emac_config.interface = EMAC_DATA_INTERFACE_RMII;
  esp32_emac_config.smi_mdc_gpio_num = CONFIG_ETH_GPIO_MDC;
  esp32_emac_config.smi_mdio_gpio_num = CONFIG_ETH_GPIO_MDIO;
  esp32_emac_config.clock_config.rmii.clock_mode = CONFIG_ETH_CLK_MODE;
  esp32_emac_config.clock_config.rmii.clock_gpio = (emac_rmii_clock_gpio_t)CONFIG_ETH_GPIO_CLK;

  // Create new ESP32 Ethernet MAC instance
  _mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
  
  // Create new PHY instance based on board configuration
  #if CONFIG_ETH_PHY_TYPE == ETH_PHY_IP101
    rlog_d(logTAG, "Selected PHY: IP101");
    _phy = esp_eth_phy_new_ip101(&phy_config);
  #elif CONFIG_ETH_PHY_TYPE == ETH_PHY_RTL8201
    rlog_d(logTAG, "Selected PHY: RTL8201");
    _phy = esp_eth_phy_new_rtl8201(&phy_config);
  #elif CONFIG_ETH_PHY_TYPE == ETH_PHY_LAN87XX
    rlog_d(logTAG, "Selected PHY: LAN87XX");
    _phy = esp_eth_phy_new_lan87xx(&phy_config);
  #elif CONFIG_ETH_PHY_TYPE == ETH_PHY_DP83848
    rlog_d(logTAG, "Selected PHY: DP83848");
    _phy = esp_eth_phy_new_dp83848(&phy_config);
  #elif CONFIG_ETH_PHY_TYPE == ETH_PHY_KSZ80XX
    rlog_d(logTAG, "Selected PHY: KSZ80XX");
    _phy = esp_eth_phy_new_ksz80xx(&phy_config);
  #endif

  // Init Ethernet driver to default and install it
  esp_eth_config_t config = ETH_DEFAULT_CONFIG(_mac, _phy);
  ret = esp_eth_driver_install(&config, &_eth_handle);
  if (ret == ESP_OK) {
    rlog_i(logTAG, "Ethernet driver installed");
    return ret;
  } else goto err;

err:
  rlog_e(logTAG, "Failed to install ethernet driver: %d (%s)", ret, esp_err_to_name(ret));
  if (_eth_handle != nullptr) {
    esp_eth_driver_uninstall(_eth_handle);
  };
  if (_mac != nullptr) _mac->del(_mac);
  if (_phy != nullptr) _phy->del(_phy);
  return ret;
}

esp_err_t ethernetDeinit()
{
  esp_err_t ret = ESP_ERR_INVALID_ARG;
  if (_eth_handle != nullptr) {
    ret = esp_eth_driver_uninstall(_eth_handle);
    if (ret != ESP_OK) {
      rlog_e(logTAG, "Ethernet uninstall failed: %d (%s)", ret, esp_err_to_name(ret));
      return ret;
    };
    if (_mac != nullptr) _mac->del(_mac);
    if (_phy != nullptr) _phy->del(_phy);
    rlog_i(logTAG, "Ethernet driver uninstalled");
    return ret;  
  };
  rlog_e(logTAG, "Ethernet deinit failed: ethernet handle cannot be NULL");
  return ret;
}

// -----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------- Ethernet event handlers -----------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static void ethernetEventHandler_Ethernet(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  uint8_t mac_addr[6] = {0};
  switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
      esp_eth_ioctl(_eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
      rlog_i(logTAG, "Ethernet link up: mac address %02x:%02x:%02x:%02x:%02x:%02x",
        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
      // Re-dispatch event to another loop
      eventLoopPost(RE_WIFI_EVENTS, RE_ETHERNET_CONNECTED, nullptr, 0, portMAX_DELAY);  
      break;

    case ETHERNET_EVENT_DISCONNECTED:
      rlog_i(logTAG, "Ethernet link down");
      // Re-dispatch event to another loop
      eventLoopPost(RE_WIFI_EVENTS, RE_ETHERNET_DISCONNECTED, nullptr, 0, portMAX_DELAY);  
      break;

    case ETHERNET_EVENT_START:
      rlog_i(logTAG, "Ethernet started");
      // Re-dispatch event to another loop
      eventLoopPost(RE_WIFI_EVENTS, RE_ETHERNET_STARTED, nullptr, 0, portMAX_DELAY);  
      break;

    case ETHERNET_EVENT_STOP:
      rlog_i(logTAG, "Ethernet stopped");
      // Re-dispatch event to another loop
      eventLoopPost(RE_WIFI_EVENTS, RE_ETHERNET_STOPPED, nullptr, 0, portMAX_DELAY);  
      break;

    default:
      break;
  };
}

static void ethernetEventHandler_GotIp(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_data) {
    ip_event_got_ip_t * data = (ip_event_got_ip_t*)event_data;
    eventLoopPost(RE_WIFI_EVENTS, RE_ETHERNET_GOT_IP, data, sizeof(ip_event_got_ip_t), portMAX_DELAY);  
    // Log
    #if CONFIG_RLOG_PROJECT_LEVEL >= RLOG_LEVEL_INFO
      uint8_t * ip = (uint8_t*)&(data->ip_info.ip.addr);
      uint8_t * mask = (uint8_t*)&(data->ip_info.netmask.addr);
      uint8_t * gw = (uint8_t*)&(data->ip_info.gw.addr);
      rlog_i(logTAG, "Ethernet got IP-address: %d.%d.%d.%d, mask: %d.%d.%d.%d, gateway: %d.%d.%d.%d",
          ip[0], ip[1], ip[2], ip[3], mask[0], mask[1], mask[2], mask[3], gw[0], gw[1], gw[2], gw[3]);
    #endif
  };
}

// -----------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------- Ethernet routines --------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

esp_err_t ethernetStart()
{
  esp_err_t ret = ESP_OK;
  
  rlog_i(logTAG, "Start Ethernet network...");
  
  // Install ethernet driver
  ret = ethernetInit();
  if ((ret == ESP_OK) && (_eth_handle != nullptr)) {
    // Start the system events task
    ret = esp_event_loop_create_default();
    if (ret == ESP_ERR_INVALID_STATE) ret = ESP_OK;
    if (ret != ESP_OK) {
      rlog_e(logTAG, "Failed to create default event loop: %d (%s)", ret, esp_err_to_name(ret));
      return ret;
    };

    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ret = esp_netif_init();
    if (ret == ESP_ERR_INVALID_SIZE) ret = ESP_OK;

    // Create instance of esp-netif for Ethernet
    esp_netif_inherent_config_t netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    netif_config.if_key = "ETH";
    netif_config.if_desc = "Ethernet";
    netif_config.route_prio = 32767;
    esp_netif_config_t config = {
      .base = &netif_config,
      .driver = nullptr,
      .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };
    _eth_netif = esp_netif_new(&config);
    if (_eth_netif == nullptr) {
      rlog_e(logTAG, "Failed to create netif interface");
      return ESP_FAIL;
    };

    _eth_netif_glue = esp_eth_new_netif_glue(_eth_handle);
    if (_eth_netif_glue == nullptr) {
      rlog_e(logTAG, "Failed to create netif glue");
      return ESP_FAIL;
    };

    // Attach Ethernet driver to TCP/IP stack
    ret = esp_netif_attach(_eth_netif, _eth_netif_glue);
    if (ret == ESP_OK) {
      // Register user defined event handers
      RE_ERROR_CHECK_EVENT(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &ethernetEventHandler_Ethernet, NULL));
      RE_ERROR_CHECK_EVENT(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ethernetEventHandler_GotIp, NULL));

      // Start Ethernet driver state machine
      ret = esp_eth_start(_eth_handle);
      if (ret != ESP_OK) {
        rlog_e(logTAG, "Failed to start ethernet driver: %d (%s)", ret, esp_err_to_name(ret));
      };
    } else {
      rlog_e(logTAG, "Failed to attach ethernet driver to TCP/IP stack: %d (%s)", ret, esp_err_to_name(ret));
    };
  };

  return ret;
}

esp_err_t ethernetStop()
{
  esp_err_t ret = ESP_FAIL;

  if (_eth_handle != nullptr) {
    rlog_i(logTAG, "Stop and deinitialize Ethernet network...");

    ret = esp_eth_stop(_eth_handle);
    if (ret != ESP_OK) {
      rlog_e(logTAG, "Failed to stop ethernet driver: %d (%s)", ret, esp_err_to_name(ret));
    };

    RE_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, ethernetEventHandler_GotIp));
    RE_ERROR_CHECK(esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, ethernetEventHandler_Ethernet));

    RE_ERROR_CHECK(esp_eth_del_netif_glue(_eth_netif_glue));
    esp_netif_destroy(_eth_netif);
    esp_netif_deinit();

    ret = ethernetDeinit();
  };

  return ret;
}

#endif // CONFIG_ETH_ENABLED)
