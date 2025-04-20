# Section Control for AgOpenGPS

Section control using an external box for AgOpenGPS. Fairly simple code designed to work with switches.

# Hardware

This code can be used directly with these boards sold on aliexpress…

![Board](../Pics/ESP32.png)

https://a.aliexpress.com/_Ewm7fKz

Here is the connection diagram. (For the board, don't forget to add the pull-up resistors(10K to 50K) for Auto/Manual mode.)

![Schema](../Pics/SchemaESP32.jpg)

To be able to upload to the aliexpress board, you will need a TTL/USB adapter.

# Meaning of LEDs

_WiFi LED flashes quickly: WiFi configuration mode. You can save the SSID and password by connecting to the "Control Section WiFi Config". The web configuration page opens at 192.168.4.1, on a phone it should open automatically.

_WiFi LED flashes more slowly: Connection is being attempted.

_Fixed WiFi LED: Connection established.

_AOG LED flashes. Connection established with AGIO.

_Steady AOG LED: Field open. Control Section is ready for use.


Switching between WiFi configuration mode and connection attempts is performed every 2 minutes if the configuration web page has not been opened, or if connection attempts have failed.

# Coming soon

The code should be extended to 16 sections by combining two panels.
Possible use of push buttons.