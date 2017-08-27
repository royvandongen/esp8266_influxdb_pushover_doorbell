===ESP8266 InfluxDB Pushover Doorbell Interface===

This sketch can be used to create a doorbell alert with a relay.

In this latest release i made an upgrade, You can now fill in your variables via the WiFiManager ( on first boot ). Just connect to the DOORBELL-***** network and fill in all the fields.

If one field is missing, the device will become an AP again so you can change the values.

The hardware used for this project:

1 x Wemos Mini
1 x Wemos compatible relay board with relais contact on output pin D1
1 x Wemos empty expirimenting board where i added input screw terminals for the bell button and power input

My setup is as follows :

* The doorbell button is connected to input pin D6 ( so complete seperate from the "old" circuit ).
* The doorbell is connected to a AC 8Volt power supply, Which output is switched via the Relay NO Contact.
* The Wemos is programmed to set a max-ring time and a throttle for what we call in the Netherlands "Belletje-Trekken". The function works as follows:
	1. A person pressed the button.
	2. Debouncing takes place because reasons, Triggers an interrupt. ( a timeout of 10000ms is set)
	3. The doorbell relay is now activated for 500ms.
	4. The Pushover message + InfluxDB logging is now done.
	5. If the timeout, set in step 2 is done, proceed to listen again for new rings.

	IF anyone keeps dossing the doorbell, every press after the doorbell rang will add 10000ms timeout to the pre-set timeout. this mitigates the "fun" fact for the funny people ringing the doorbell at strange times and stuff.

Use, change or otherwise abuse the code as needed but if we ever meet, i really like Hefe Weissen beer :P ( and please, don't do bad things with this code )
