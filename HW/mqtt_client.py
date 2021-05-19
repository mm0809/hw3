import paho.mqtt.client as paho
import time
import serial
import time


state = 0

serdev = '/dev/ttyACM0'                # use the device name you get from `ls /dev/ttyACM*`
s = serial.Serial(serdev, 9600)

# https://os.mbed.com/teams/mqtt/wiki/Using-MQTT#python-client

# MQTT broker hosted on local machine
mqttc = paho.Client()

# Settings for connection
# TODO: revise host to your IP
host = "192.168.0.5"
topic = "SetAngle"
topic2 = "TiltAngle"

cnt = 0;

# Callbacks 
def on_connect(self, mosq, obj, rc):
    print("Connected rc: " + str(rc))

def on_message(mosq, obj, msg):
    print("[Received] Topic: " + msg.topic + ", Message: " + str(msg.payload) + "\n");
    tmp = msg.payload.decode('ascii')
    #print(tmp[0])
    global state
    if (state == 1 and tmp[0] == "9"):
        global cnt
        cnt += 1
        print(cnt)
        if (cnt == 10):
            state = 0;
            cnt = 0;
            s.write(bytes("/loop/run\r", 'UTF-8'))
    else:
        state = 1
        s.write(bytes("/loop/run\r", 'UTF-8'))

    #else:
        #s.write(bytes("/loop/run\r", 'UTF-8'))

    #tmp = msg.payload.decode('UTF-8')
    #print(tmp)
    #str2 = tmp.replace("\n", "")
    #if (msg.topic == topic):
    #    print("pub\n")
    #    ret = mqttc.publish("stopAI", "9\n", qos=0)
    #    if (ret[0] != 0):
    #            print("Publish failed")
    #    mqttc.loop()
    #    time.sleep(1.5)


def on_subscribe(mosq, obj, mid, granted_qos):
    print("Subscribed OK")

def on_unsubscribe(mosq, obj, mid, granted_qos):
    print("Unsubscribed OK")

# Set callbacks
mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.on_subscribe = on_subscribe
mqttc.on_unsubscribe = on_unsubscribe

# Connect and subscribe
print("Connecting to " + host + "/" + topic)
mqttc.connect(host, port=1883, keepalive=60)
mqttc.subscribe(topic, 0)
print("subscribe to ", topic2)
mqttc.subscribe(topic2, 0)

# Publish messages from Python
#num = 0
#while num != 5:
#    ret = mqttc.publish(topic, "Message from Python!\n", qos=0)
#    if (ret[0] != 0):
#            print("Publish failed")
#    mqttc.loop()
#    time.sleep(1.5)
#    num += 1

# Loop forever, receiving messages
s.write(bytes("/AI/run\r", 'UTF-8'))
while (state == 0):
    mqttc.loop()

time.sleep(1)
s.write(bytes("/Angle/run\r", 'UTF-8'))
mqttc.loop_forever()
