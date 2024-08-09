/* 
   EN: Module for connecting to the network via wires using Ethernet technology. 
       Details and a list of supported Ethernet chips can be found in the Espressif documentation: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_eth.html
   RU: Модуль для подключения к сети посредством проводов по технологии Ethernet. 
       Подробности и перечень поддкрживаемых ethernet-чипов можно найти в документации Espressif: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_eth.html
   --------------------------
   (с) 2024 Разживин Александр | Razzhivin Alexander
   kotyara12@yandex.ru | https://kotyara12.ru | tg: @kotyara1971
*/

#include "project_config.h"
#include "def_consts.h"

#if defined(CONFIG_ETH_ENABLED) && (CONFIG_ETH_ENABLED == 1)

#ifndef __RE_ETHERNET_H__
#define __RE_ETHERNET_H__ 

#include <stdint.h>
#include <stdbool.h>
#include <cstring>
#include <time.h> 
#include "esp_eth.h"
#include "esp_mac.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ETH_PHY_IP101   1
#define ETH_PHY_RTL8201 2
#define ETH_PHY_LAN87XX 3
#define ETH_PHY_DP83848 4
#define ETH_PHY_KSZ80XX 5

esp_err_t ethernetStart();
esp_err_t ethernetStop();

#ifdef __cplusplus
}
#endif

#endif // __RE_ETHERNET_H__

#endif // CONFIG_ETH_ENABLED)
