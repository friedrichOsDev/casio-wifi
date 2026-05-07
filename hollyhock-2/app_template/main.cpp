#include <appdef.hpp>
#include <sdk/calc/calc.hpp>
#include "wifi.h"
#include "wifi_app.hpp"

APP_NAME("Casio WiFi")
APP_DESCRIPTION("The GUI for my WiFi module driver")
APP_AUTHOR("friedrichOsDev")
APP_VERSION("1.0.0")

extern "C"
void main() {
	calcInit();
    wifi_init();

	{
		WiFiMenu menu;
		menu.ShowDialog();
	}

	uint32_t key1, key2;
	while (1) {
		getKey(&key1, &key2);
		if (testKey(key1, key2, KEY_CLEAR)) {
			break;
		}
	}

	wifi_deinit();
	calcEnd();
}
