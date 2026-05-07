#include <appdef.hpp>
#include <sdk/calc/calc.hpp>

APP_NAME("Casio WiFi")
APP_DESCRIPTION("The GUI for my WiFi module driver")
APP_AUTHOR("friedrichOsDev")
APP_VERSION("1.0.0")

extern "C"
void main() {
	calcInit();

	calcEnd();
}
