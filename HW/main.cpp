#include "mbed.h"
#include "uLCD_4DGL.h"      // uLCD

#include "mbed_rpc.h"       // rpc 
#include "ai.h"
#include "MQTTNetwork.h"    // MQTT
#include "MQTTmbed.h"
#include "MQTTClient.h"

#include "stm32l475e_iot01_accelero.h"


#include "iostream"


uLCD_4DGL uLCD(D1, D0, D2); // uLCD

BufferedSerial pc(USBTX, USBRX);    // rpc
void LEDControl(Arguments* in, Reply* out); // the parameter is necessary
void GestureUI(Arguments* in, Reply* out); 
void loopState(Arguments* in, Reply* out); 
RPCFunction testRPC(&LEDControl, "LEDtest");
RPCFunction GestureUIRPC(&GestureUI, "AI");
RPCFunction loopStateRPC(&loopState, "loop");

Thread tAI;
EventQueue queueAI(32 * EVENTS_EVENT_SIZE);
int runAI();
void confirm(MQTT::Client<MQTTNetwork, Countdown>* client);

InterruptIn Ubutton(USER_BUTTON);

int i = 0;

// MQTT
WiFiInterface *wifi;
volatile int message_num = 0;
volatile int arrivedcount = 0;
volatile bool closed = false;

const char* topic = "SetAngle";

Thread mqtt_thread(osPriorityHigh);
EventQueue mqtt_queue;

void messageArrived(MQTT::MessageData& md);
void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client, int A);
void mqttHandle(MQTT::Client<MQTTNetwork, Countdown>* client);

EventQueue angle_queue(32 * EVENTS_EVENT_SIZE);
Thread angle_thread;


volatile int state = 0;
volatile int angleID = 0;



MQTT::Client<MQTTNetwork, Countdown>* pclient;
int main()
{
    //-----MQTT init-----
    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
            printf("ERROR: No WiFiInterface found.\r\n");
            return -1;
    }


    printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
            printf("\nConnection error: %d\r\n", ret);
            return -1;
    }


    NetworkInterface* net = wifi;
    MQTTNetwork mqttNetwork(net);
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    pclient = &client;

    //TODO: revise host to your IP
    const char* host = "192.168.0.5";
    printf("Connecting to TCP network...\r\n");

    SocketAddress sockAddr;
    sockAddr.set_ip_address(host);
    sockAddr.set_port(1883);

    printf("address is %s/%d\r\n", (sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "None"),  (sockAddr.get_port() ? sockAddr.get_port() : 0) ); //check setting

    int rc = mqttNetwork.connect(sockAddr);//(host, 1883);
    if (rc != 0) {
            printf("Connection error.");
            return -1;
    }
    printf("Successfully connected!\r\n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "Mbed";

    if ((rc = client.connect(data)) != 0){
            printf("Fail to connect MQTT\r\n");
    }

    if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){
            printf("Fail to subscribe\r\n");
    }

    mqtt_thread.start(callback(&mqtt_queue, &EventQueue::dispatch_forever));
    //mqtt_queue.call_every(1s, &mqttHandle, &client);
    //mqtt_queue.dispatch();

    //btn2.rise(mqtt_queue.event(&publish_message, &client));

    // MQTT test
    //int num = 0;
    //while (num != 5) {
    //        client.yield(100);
    //        ++num;
    //}
    
    // -----RPC stufs----
    char buf[256], outbuf[256];
    FILE *devin = fdopen(&pc, "r"); FILE *devout = fdopen(&pc, "w");
    uLCD.printf("\nuLCD working\n");    //Default Green on black text

    tAI.start(callback(&queueAI, &EventQueue::dispatch_forever));
    Ubutton.rise(mqtt_queue.event(&confirm, &client));

    //Ubutton.rise(&confirm);


    while(1) {
        memset(buf, 0, 256);
        for (int i = 0; ; i++) {
            char recv = fgetc(devin);
            if (recv == '\n') {
                printf("\r\n");
                break;
            }
            buf[i] = fputc(recv, devout);
        }
        //Call the static call method on the RPC class
        RPC::call(buf, outbuf);
        printf("%s\r\n", outbuf);
    }
}


void LEDControl(Arguments* in, Reply* out)
{
    i++;
    uLCD.locate(0, 0);
    uLCD.printf("\ntest RPC: %d", i);
}

void GestureUI(Arguments* in, Reply* out)
{
    state = 1;
    queueAI.call(runAI);
}

void loopState(Arguments* in, Reply* out)
{
    state = 0;
}

void confirm(MQTT::Client<MQTTNetwork, Countdown>* client) { 
    mqtt_queue.call(&publish_message, client, angleID);
    //client->yield(700);
}

void mqttHandle(MQTT::Client<MQTTNetwork, Countdown>* client)
{
    if (state ==  2) {
        cout << "mqtt" << endl;
        client->yield(100);
    }
}

int runAI() {

  // Whether we should clear the buffer next time we fetch data
  bool should_clear_buffer = false;
  bool got_data = false;

  // The gesture index of the prediction
  int gesture_index;

  // Set up logging.
  static tflite::MicroErrorReporter micro_error_reporter;
  tflite::ErrorReporter* error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  const tflite::Model* model = tflite::GetModel(g_magic_wand_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report(
        "Model provided is schema version %d not equal "
        "to supported version %d.",
        model->version(), TFLITE_SCHEMA_VERSION);
    return -1;
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  static tflite::MicroOpResolver<6> micro_op_resolver;
  micro_op_resolver.AddBuiltin(
      tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
      tflite::ops::micro::Register_DEPTHWISE_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_MAX_POOL_2D,
                               tflite::ops::micro::Register_MAX_POOL_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                               tflite::ops::micro::Register_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                               tflite::ops::micro::Register_FULLY_CONNECTED());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                               tflite::ops::micro::Register_SOFTMAX());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_RESHAPE,
                               tflite::ops::micro::Register_RESHAPE(), 1);
  // Build an interpreter to run the model with
  static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  tflite::MicroInterpreter* interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors
  interpreter->AllocateTensors();

  // Obtain pointer to the model's input tensor
  TfLiteTensor* model_input = interpreter->input(0);
  if ((model_input->dims->size != 4) || (model_input->dims->data[0] != 1) ||
      (model_input->dims->data[1] != config.seq_length) ||
      (model_input->dims->data[2] != kChannelNumber) ||
      (model_input->type != kTfLiteFloat32)) {
    error_reporter->Report("Bad input tensor parameters in model");
    return -1;
  }

  int input_length = model_input->bytes / sizeof(float);

  TfLiteStatus setup_status = SetupAccelerometer(error_reporter);
  if (setup_status != kTfLiteOk) {
    error_reporter->Report("Set up failed\n");
    return -1;
  }

  error_reporter->Report("Set up successful...\n");

  while (true) {
    if (state == 0) {
        cout << "set a ngleID: " << angleID << endl;
      return 1;
    }

    // Attempt to read new data from the accelerometer
    got_data = ReadAccelerometer(error_reporter, model_input->data.f,
                                 input_length, should_clear_buffer);

    // If there was no new data,
    // don't try to clear the buffer again and wait until next time
    if (!got_data) {
      should_clear_buffer = false;
      continue;
    }

    // Run inference, and report any error
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
      error_reporter->Report("Invoke failed on index: %d\n", begin_index);
      continue;
    }

    // Analyze the results to obtain a prediction
    gesture_index = PredictGesture(interpreter->output(0)->data.f);

    // Clear the buffer next time we read data
    should_clear_buffer = gesture_index < label_num;

    // Produce an output
    if (gesture_index < label_num) {
      //error_reporter->Report(config.output_message[gesture_index]);
      cout << gesture_index << endl;
      angleID = gesture_index;
      uLCD.locate(1, 0);
      uLCD.printf("\nangle: %d", angleID);
    }
  }
}

void messageArrived(MQTT::MessageData& md) {
    MQTT::Message &message = md.message;
    char msg[300];
    sprintf(msg, "Message arrived: QoS%d, retained %d, dup %d, packetID %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf(msg);
    ThisThread::sleep_for(1000ms);
    char payload[300];
    sprintf(payload, "Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    printf(payload);
    ++arrivedcount;
}

void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client, int A) {
    message_num++;
    MQTT::Message message;
    char buff[100];
    sprintf(buff, "AngleID: %d", A);
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*) buff;
    message.payloadlen = strlen(buff) + 1;
    int rc = client->publish(topic, message);

    printf("rc:  %d\r\n", rc);
    printf("Puslish message: %s\r\n", buff);
}
