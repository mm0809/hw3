# hw3

## How to set up and run my program
1. Creat ```mbed_app.json```
``` json
{
    "config": {
    "wifi-ssid": {
            "help": "WiFi SSID",
            "value": "\"SSID\""
    },
    "wifi-password": {
            "help": "WiFi Password",
            "value": "\"PASSWORD\""
    }
    },
    "target_overrides": {
        "B_L4S5I_IOT01A": {
            "target.components_add": ["ism43362"],
            "ism43362.provide-default": true,
            "target.network-default-interface-type": "WIFI",
            "target.macros_add" : ["MBEDTLS_SHA1_C"]
        }
    }
}
```
2. Change ip in ```main.cpp```(line:105) and ```wifi_mqtt/mqtt_client.py```(line:19)
3. Compile the program
```
$ sudo mbed compile --source . --source ~/ee2405/mbed-os/ -m B_L4S5I_IOT01A -t GCC_ARM --profile tflite.json -f
```
4. Wait for mbed initialize [(according to the LEDs)](#LEDs-and-state).
5. Execute ```$ sudo python3 wifi_mqtt/mqtt_client.py```

## LEDs and state 


| LED 1    | LED 2 | LED 3 | state                     |
| -------- | ----- | ----- | ------------------------- |
| 0        | 0     | 0     | initlizing                |
| 0        | 1     | 0     | RPC loop                  |
| 0        | 1     | 1     | gesture detecting         |
| blinking | 1     | 0     | gravity reference setting |
| 1        | 1     | 0     | tilt angle detecting      |

## Result screen shot
![](https://github.com/mm0809/hw3/blob/main/HW/img/Screenshot%20from%202021-05-10%2001-28-03.png?raw=true)
