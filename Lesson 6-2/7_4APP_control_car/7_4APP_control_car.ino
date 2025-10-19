#include "esp_camera.h"
#include <vehicle.h>
#include <ultrasonic.h>
#include <ESP32Servo.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_http_server.h"

#define Shoot_PIN           32//shoot---150ms
#define Yservo_PIN         25

#define LED_Module1         2
#define LED_Module2         12        
#define Left_Line           35
#define Center_Line         36
#define Right_Line          39
#define Buzzer              33

#define CMD_RUN 1
#define CMD_GET 2
#define CMD_STANDBY 3
#define CMD_TRACK_1 4
#define CMD_TRACK_2 5
#define CMD_AVOID 6
#define CMD_FOLLOW 7

// Added arm & soil commands
#define CMD_ARM_PUSH 8
#define CMD_ARM_RETRACT 9

//app music
#define C3 131
#define D3 147
#define E3 165
#define F3 175
#define G3 196
#define A3 221
#define B3 248

#define C4 262
#define D4 294
#define E4 330
#define F4 350
#define G4 393
#define A4 441
#define B4 495

#define C5 525
#define D5 589
#define E5 661
#define F5 700
#define G5 786
#define A5 882
#define B5 990
#define N 0

vehicle Acebott; //car
ultrasonic Ultrasonic; //Ultrasonic wave
Servo Yservo; //Servo

int Left_Tra_Value;
int Center_Tra_Value;
int Right_Tra_Value;
int Black_Line = 2000;
int Off_Road = 4000;
int speeds = 250;
int leftDistance = 0;
int middleDistance = 0;
int rightDistance = 0;

String sendBuff;
String Version = "Firmware Version is 0.12.21";
byte dataLen, index_a = 0;
char buffer[52];
unsigned char prevc=0;
bool isStart = false;
bool ED_client = true;
byte RX_package[17] = {0};
uint16_t angle = 90;
byte action = Stop, device;
byte val = 0;
char model_var = 0;
int UT_distance = 0;

int length0;
int length1;
int length2;
int length3;
/*****app music*****/
// littel star
int tune0[] = { C4, N, C4, G4, N, G4, A4, N, A4, G4, N, F4, N, F4, E4, N, E4, D4, N, D4, C4 };
float durt0[] = { 0.99, 0.01, 1, 0.99, 0.01, 1, 0.99, 0.01, 1, 1.95, 0.05, 0.99, 0.01, 1, 0.99, 0.01, 1, 0.99, 0.01, 1, 2 };
// jingle bell
int tune1[] = { E4, N, E4, N, E4, N, E4, N, E4, N, E4, N, E4, G4, C4, D4, E4 };
float durt1[] = { 0.49, 0.01, 0.49, 0.01, 0.99, 0.01, 0.49, 0.01, 0.49, 0.01, 0.99, 0.01, 0.5, 0.5, 0.75, 0.25, 1, 2};
// happy new year
int tune2[] = { C5, N, C5, N, C5, G4, E5, N, E5, N, E5, C5, N, C5, E5, G5, N, G5, F5, E5, D5, N };
float durt2[] = { 0.49, 0.01, 0.49, 0.01, 1, 1, 0.49, 0.01, 0.49, 0.01, 1, 0.99, 0.01, 0.5, 0.5,0.99,0.01, 1, 0.5, 0.5, 1, 1 };
// have a farm
int tune3[] = { C4, N, C4, N, C4, G3, A3, N, A3, G3,  E4, N, E4, D4, N, D4, C4 };
float durt3[] = { 0.99, 0.01, 0.99, 0.01, 1, 1, 0.99, 0.01, 1, 2, 0.99, 0.01, 1, 0.99, 0.01, 1, 1 };
/*****app music*****/

unsigned long lastDataTimes = 0;
bool st = false;

// Robot WiFi AP configuration - robot will create its own network named "smartcar"
// with password "12345678". The camera should connect to this SSID in STA mode.
// (No STA credentials are stored here.)

// Server receiving telemetry (the app expects ESP32 at known IP; set if using a central server)
const char* TELEMETRY_SERVER = "http://192.168.4.1:80"; // update if needed

// Arm & soil (sourced from TIAGA_JOANE). These must match TIAGA_JOANE's authoritative pins.
// We guard against accidental re-definition to ensure the robot uses the expected pins.
#ifdef ARM_SERVO_PIN
#undef ARM_SERVO_PIN
#endif
#define ARM_SERVO_PIN 27  // authoritative pin; matches TIAGA_JOANE

#ifdef SOIL_SENSOR_PIN
#undef SOIL_SENSOR_PIN
#endif
#define SOIL_SENSOR_PIN 34 // authoritative pin; matches TIAGA_JOANE
int armCurrentPosition = 90;
const int ARM_HOME = 90;
const int ARM_DOWN = 180;
const int ARM_MIN = 90;
const int ARM_MAX = 180;
const int ARM_SPEED = 15; // ms per degree
int soilValue = 0;
int soilPercent = 0;
int dryValue = 3000;
int wetValue = 1000;
bool isReadingSoil = false;
unsigned long lastSoilReadTime = 0;
const unsigned long SOIL_READ_INTERVAL = 1000;
Servo armServo;

// A small helper to inject bytes into the existing buffer parsing (used by serial/HTTP handlers)
void pushByteToParser(uint8_t c) {
    Serial.write(c);
    lastDataTimes = millis();
    if (c == 200) {
        st = false;
    }
    if (c == 0x55 && isStart == false) {
        if (prevc == 0xff) {
            index_a = 1;
            isStart = true;
        }
    } else {
        prevc = c;
        if (isStart) {
            if (index_a == 2) {
                dataLen = c;
            } else if (index_a > 2) {
                dataLen--;
            }
            writeBuffer(index_a, c);
        }
    }
    index_a++;
    if (index_a > 120) {
        index_a = 0;
        isStart = false;
    }
    if (isStart && dataLen == 0 && index_a > 3) {
        isStart = false;
        parseData();
        index_a = 0;
    }
}

// HTTP control endpoint: accept POST /control with raw body or form key "cmd"
static esp_err_t control_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    // If single-char command 's' or 'r', trigger sequences
    if (ret == 1) {
        char c = buf[0];
        if (c == 's' || c == 'S') {
            pushSensorSequence();
            httpd_resp_sendstr(req, "OK");
            return ESP_OK;
        } else if (c == 'r' || c == 'R') {
            retractArmSequence();
            httpd_resp_sendstr(req, "OK");
            return ESP_OK;
        }
    }
    // Otherwise feed bytes into parser
    for (int i = 0; i < ret; i++) {
        pushByteToParser((uint8_t)buf[i]);
    }
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

void startControlServer() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    if (httpd_start(&server, &config) == ESP_OK) {
        // POST /control (raw body -> parser or single-char commands)
        httpd_uri_t control_post_uri = {
            .uri = "/control",
            .method = HTTP_POST,
            .handler = control_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &control_post_uri);

            // GET /control?cmd=... (full compatibility for app-style control calls)
            // Handler defined below for clarity
            extern esp_err_t control_get_handler(httpd_req_t *req);
            httpd_uri_t control_get_uri = {
                .uri = "/control",
                .method = HTTP_GET,
                .handler = control_get_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &control_get_uri);

        // GET /sensor - return latest soil moisture as JSON
        httpd_uri_t sensor_uri = {
            .uri = "/sensor",
            .method = HTTP_GET,
            .handler = [](httpd_req_t *req) -> esp_err_t {
                char resp[64];
                snprintf(resp, sizeof(resp), "{\"moisture\": %d}", soilPercent);
                httpd_resp_set_type(req, "application/json");
                httpd_resp_sendstr(req, resp);
                return ESP_OK;
            },
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &sensor_uri);

        // GET / - simple root page
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = [](httpd_req_t *req) -> esp_err_t {
                const char* page = "<h3>ESP32 Robot Control</h3><p>Use /control and /sensor endpoints.</p>";
                httpd_resp_set_type(req, "text/html");
                httpd_resp_sendstr(req, page);
                return ESP_OK;
            },
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);
    }
}

        // Full GET control handler implementation
        esp_err_t control_get_handler(httpd_req_t *req) {
            char buf[256];
            size_t buf_len = sizeof(buf);
            if (httpd_req_get_url_query_str(req, buf, buf_len) != ESP_OK) {
                httpd_resp_sendstr(req, "Missing query");
                return ESP_OK;
            }
            char cmd[32];
            if (httpd_query_key_value(buf, "cmd", cmd, sizeof(cmd)) != ESP_OK) {
                httpd_resp_sendstr(req, "cmd missing");
                return ESP_OK;
            }
            Serial.printf("control_get_handler: cmd=%s\n", cmd);

            // Car movement: cmd=car&direction=Forward|Backward|Left|Right|Clockwise|Anticlockwise|stop
            if (strcmp(cmd, "car") == 0) {
                char dir[32];
                if (httpd_query_key_value(buf, "direction", dir, sizeof(dir)) == ESP_OK) {
                    Serial.printf("direction=%s\n", dir);
                    if (strcasecmp(dir, "Forward") == 0) {
                        Acebott.Move(Forward, 180);
                    } else if (strcasecmp(dir, "Backward") == 0) {
                        Acebott.Move(Backward, 180);
                    } else if (strcasecmp(dir, "Left") == 0) {
                        Acebott.Move(Contrarotate, 150);
                    } else if (strcasecmp(dir, "Right") == 0) {
                        Acebott.Move(Clockwise, 150);
                    } else if (strcasecmp(dir, "Clockwise") == 0) {
                        Acebott.Move(Clockwise, 180);
                    } else if (strcasecmp(dir, "Anticlockwise") == 0) {
                        Acebott.Move(Contrarotate, 180);
                    } else if (strcasecmp(dir, "stop") == 0 || strcasecmp(dir, "Stop") == 0) {
                        Acebott.Move(Stop, 0);
                    }
                }
                httpd_resp_sendstr(req, "OK");
                return ESP_OK;
            }

            // Speed command: cmd=speed&value=1..3 or numeric
            if (strcmp(cmd, "speed") == 0) {
                char value[16];
                if (httpd_query_key_value(buf, "value", value, sizeof(value)) == ESP_OK) {
                    int v = atoi(value);
                    if (v >= 1 && v <= 3) {
                        speeds = 80 + v * 60; // map level to motor pwm roughly
                    } else {
                        speeds = v;
                    }
                }
                httpd_resp_sendstr(req, "OK");
                return ESP_OK;
            }

            // Servo control: cmd=servo&angle=0..180
            if (strcmp(cmd, "servo") == 0) {
                char angleStr[16];
                if (httpd_query_key_value(buf, "angle", angleStr, sizeof(angleStr)) == ESP_OK) {
                    int a = atoi(angleStr);
                    a = constrain(a, 0, 180);
                    moveArmToPosition(a);
                }
                httpd_resp_sendstr(req, "OK");
                return ESP_OK;
            }

            // CAM LED: cmd=CAM_LED&value=1|0 -- placeholder for compatibility
            if (strcmp(cmd, "CAM_LED") == 0) {
                char val[8];
                if (httpd_query_key_value(buf, "value", val, sizeof(val)) == ESP_OK) {
                    if (val[0] == '1') {
                        // Implement CAM LED if hardware present
                        Serial.println("CAM LED ON");
                    } else {
                        Serial.println("CAM LED OFF");
                    }
                }
                httpd_resp_sendstr(req, "OK");
                return ESP_OK;
            }

            // Soil check: cmd=soil_check or cmd=soil
            if (strcmp(cmd, "soil_check") == 0 || strcmp(cmd, "soil") == 0) {
                pushSensorSequence();
                httpd_resp_sendstr(req, "OK");
                return ESP_OK;
            }

            httpd_resp_sendstr(req, "Unknown cmd");
            return ESP_OK;
        }

// Optional: if you prefer static IPs, uncomment and set gateway appropriately
/*
IPAddress localIP(192,168,4,1);
IPAddress gateway(192,168,4,254); // set to your router IP
IPAddress subnet(255,255,255,0);
WiFi.config(localIP, gateway, subnet);
*/

unsigned char readBuffer(int index_r)
{
  return buffer[index_r]; 
}
void writeBuffer(int index_w,unsigned char c)
{
  buffer[index_w]=c;
}

enum FUNCTION_MODE
{
  STANDBY,
  FOLLOW,
  TRACK_1,
  TRACK_2,
  AVOID,
} function_mode;

void setup()
{
    Serial.setTimeout(10);  // Set the serial port timeout to 10 milliseconds
    Serial.begin(115200);  // Serial communication is initialized with a baud rate of 115200

    Acebott.Init();  // Initialize Acebott
    Ultrasonic.Init(13,14);  // Initialize the ultrasonic module
    
    pinMode(LED_Module1, OUTPUT);  // Set pin of LED module as output
    pinMode(LED_Module2, OUTPUT); 
    pinMode(Shoot_PIN, OUTPUT);  // Set the shooting pin as the output
    pinMode(Left_Line, INPUT);  // Set infrared left line pin as input
    pinMode(Center_Line, INPUT);  // Set the infrared middle line pin as input
    pinMode(Right_Line, INPUT);  // Set the right infrared line pin as input

    ESP32PWM::allocateTimer(1);  // Assign timer 1 to the ESP32PWM library
    Yservo.attach(Yservo_PIN);  // Connect the servo to the Yservo_PIN pin
    Yservo.write(angle);  // Set the steering angle as Angle
    Acebott.Move(Stop, 0);  // Stop the Acebott exercise
    delay(3000); 

    // Arm servo and soil sensor
    armServo.attach(ARM_SERVO_PIN);
    armServo.write(180 - armCurrentPosition);
    pinMode(SOIL_SENSOR_PIN, INPUT);

    length0 = sizeof(tune0) / sizeof(tune0[0]);  // Calculate the length of the tune0 array
    length1 = sizeof(tune1) / sizeof(tune1[0]);  // Calculate the length of the tune1 array
    length2 = sizeof(tune2) / sizeof(tune2[0]);  // Calculate the length of the tune2 array
    length3 = sizeof(tune3) / sizeof(tune3[0]);  // Calculate the length of the tune3 array

    // Robot is BLE-only; no WiFi server will be started here.
        // Initialize WiFi as a soft-AP so the robot creates its own network
        // AP SSID: smartcar, password: 12345678 (change if you want)
        Serial.print("Starting WiFi softAP 'smartcar'...");
        IPAddress apIP(192,168,4,1);
        IPAddress apGateway(192,168,4,1);
        IPAddress apSubnet(255,255,255,0);
        WiFi.mode(WIFI_AP);
        if (!WiFi.softAPConfig(apIP, apGateway, apSubnet)) {
            Serial.println("Failed to configure softAP IP, continuing");
        }
        if (WiFi.softAP("smartcar", "12345678")) {
            Serial.println(" OK");
            Serial.print("AP IP: ");
            Serial.println(WiFi.softAPIP());
            Serial.print("AP MAC: ");
            Serial.println(WiFi.softAPmacAddress());
        } else {
            Serial.println(" Failed to start softAP");
        }
        // Start simple control HTTP server to receive commands over WiFi (AP)
        startControlServer();
}

void loop()
{
    // Handle periodic soil reading if active
    if (isReadingSoil && (millis() - lastSoilReadTime >= SOIL_READ_INTERVAL)) {
        // read and send via BLE notify
                soilValue = analogRead(SOIL_SENSOR_PIN);
                soilPercent = map(soilValue, dryValue, wetValue, 0, 100);
                soilPercent = constrain(soilPercent, 0, 100);
                // Send via HTTP POST to telemetry endpoint
                if (WiFi.status() == WL_CONNECTED) {
                    HTTPClient http;
                    String url = String(TELEMETRY_SERVER) + "/telemetry";
                    http.begin(url);
                    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
                    String payload = "type=soil&value=" + String(soilPercent);
                    int code = http.POST(payload);
                    if (code > 0) {
                        Serial.printf("Telemetry POST code: %d\n", code);
                    } else {
                        Serial.printf("Telemetry POST failed: %s\n", http.errorToString(code).c_str());
                    }
                    http.end();
                }
        lastSoilReadTime = millis();
    }

    // BLE writes call pushByteToParser -> parseData(), so just run behavior loop
    functionMode();
    // keep responsive
    delay(20);
}

void functionMode()
{
    switch (function_mode)
    {
        case FOLLOW:
        {
            model3_func();  // Enter follow mode and call the model3_func() function
        }
        break;
        case TRACK_1:
        {
            model1_func();  // Go into trace mode 1 and call the model1_func() function
        }
        break;
        case TRACK_2:
        {
            model4_func();  // Enter trace mode 2 and call the model4_func() function
        }
        break;        
        case AVOID:
        {
            model2_func();  // Enter obstacle avoidance mode and call the model2_func() function
        }
        break;
        default:
            break;
    }
}

// Send a small telemetry message over HTTP POST (non-blocking best-effort)
void sendTelemetryHttp(const char* msg) {
        if (WiFi.status() != WL_CONNECTED) return;
        HTTPClient http;
        String url = String(TELEMETRY_SERVER) + "/telemetry";
        http.begin(url);
        http.addHeader("Content-Type", "text/plain");
        int code = http.POST(String(msg));
        if (code > 0) {
            Serial.printf("sendTelemetryHttp code=%d\n", code);
        }
        http.end();
}

// --- Arm helpers (from TIAGA) ---
void moveArmToPosition(int angle) {
    angle = constrain(angle, ARM_MIN, ARM_MAX);
    armServo.write(180 - angle);
    armCurrentPosition = angle;
    Serial.printf("Arm position: %d\n", angle);
}

void moveArmSmooth(int targetAngle) {
    targetAngle = constrain(targetAngle, ARM_MIN, ARM_MAX);
    if (targetAngle == armCurrentPosition) return;
    if (targetAngle > armCurrentPosition) {
        for (int pos = armCurrentPosition; pos <= targetAngle; pos++) {
            armServo.write(180 - pos);
            delay(ARM_SPEED);
        }
    } else {
        for (int pos = armCurrentPosition; pos >= targetAngle; pos--) {
            armServo.write(180 - pos);
            delay(ARM_SPEED);
        }
    }
    armCurrentPosition = targetAngle;
}

void pushSensorSequence() {
    if (abs(armCurrentPosition - ARM_DOWN) <= 10) {
        if (!isReadingSoil) {
            isReadingSoil = true;
            lastSoilReadTime = millis();
        }
        return;
    }
    moveArmSmooth(ARM_DOWN);
    isReadingSoil = true;
    lastSoilReadTime = millis();
}

void retractArmSequence() {
    if (abs(armCurrentPosition - ARM_HOME) <= 10) {
        if (isReadingSoil) isReadingSoil = false;
        return;
    }
    isReadingSoil = false;
    moveArmSmooth(ARM_HOME);
}


// NOTE: BLE writes now inject bytes into the parser via pushByteToParser() from the BLE onWrite
// callback. The original implementation expected a 'client' object (e.g., from TCP/WiFi) which
// isn't present in the BLE-only robot firmware. Keep a no-op Receive_data() at the end of the
// file to satisfy callers in model functions; BLE write handling is performed elsewhere.

void model2_func()      // OA
{
    Yservo.write(90);
    UT_distance = Ultrasonic.Ranging();
    //Serial.print("UT_distance:  ");
    //Serial.println(UT_distance);
    middleDistance = UT_distance;

    if (middleDistance <= 25) 
    {
        Acebott.Move(Stop, 0);
        for(int i = 0;i < 500;i++)
        {
          delay(1);
          Receive_data();
          if(function_mode != AVOID)
            return ;
        }
        Yservo.write(45);
        for(int i = 0;i < 300;i++)
        {
          delay(1);
          Receive_data();
          if(function_mode != AVOID)
            return ;
        }
        rightDistance = Ultrasonic.Ranging();
        //Serial.print("rightDistance:  ");
        //Serial.println(rightDistance);
        Yservo.write(135);
        for(int i = 0;i < 300;i++)
        {
          delay(1);
          Receive_data();
          if(function_mode != AVOID)
            return ;
        }
        leftDistance = Ultrasonic.Ranging();
        //Serial.print("leftDistance:  ");
        //Serial.println(leftDistance);
        Yservo.write(90);
        if((rightDistance < 10) && (leftDistance < 10))
        {
            Acebott.Move(Backward, 180);
            for(int i = 0;i < 1000;i++)
            {
                delay(1);
                Receive_data();
                if(function_mode != AVOID)
                    return ;
            }
            Acebott.Move(Contrarotate, 180);//delay(200);
            for(int i = 0;i < 500;i++)
            {
                delay(1);
                Receive_data();
                if(function_mode != AVOID)
                    return ;
            }
        }
        else if(rightDistance < leftDistance) 
        {
            Acebott.Move(Backward, 180);
            for(int i = 0;i < 500;i++)
            {
                delay(1);
                Receive_data();
                if(function_mode != AVOID)
                    return ;
            }
            Acebott.Move(Contrarotate, 180);//delay(200);
            for(int i = 0;i < 500;i++)
            {
                delay(1);
                Receive_data();
                if(function_mode != AVOID)
                    return ;
            }
        }//turn right
        else if(rightDistance > leftDistance)
        {
            Acebott.Move(Backward, 180);
            for(int i = 0;i < 500;i++)
            {
                delay(1);
                Receive_data();
                if(function_mode != AVOID)
                    return ;
            }
            Acebott.Move(Clockwise, 180);//delay(200);
            for(int i = 0;i < 500;i++)
            {
                delay(1);
                Receive_data();
                if(function_mode != AVOID)
                    return ;
            }
        }
        else
        {
            Acebott.Move(Backward, 180);
            for(int i = 0;i < 500;i++)
            {
                delay(1);
                Receive_data();
                if(function_mode != AVOID)
                    return ;
            }
            Acebott.Move(Clockwise, 180);//delay(200); 
            for(int i = 0;i < 500;i++)
            {
                delay(1);
                Receive_data();
                if(function_mode != AVOID)
                    return ;
            }
        }
    }
    else 
    {
        Acebott.Move(Forward, 150);
    }
}

void model3_func()      // follow model
{
    Yservo.write(90);  
    UT_distance = Ultrasonic.Ranging();
    //Serial.println(UT_distance);
    if (UT_distance < 15)
    {
        Acebott.Move(Backward, 200);
    }
    else if (15 <= UT_distance && UT_distance <= 20)
    {
        Acebott.Move(Stop, 0);
    }
    else if (20 <= UT_distance && UT_distance <= 25)
    {
        Acebott.Move(Forward, speeds-70);
    }
    else if (25 <= UT_distance && UT_distance <= 50)
    {
        Acebott.Move(Forward, 220);
    }
    else
    {
        Acebott.Move(Stop, 0);
    }
}

void model4_func()      // tracking model2
{
    Yservo.write(90);
    Left_Tra_Value = analogRead(Left_Line);
    Center_Tra_Value = analogRead(Center_Line);
    Right_Tra_Value = analogRead(Right_Line);
    delay(5);
    if (Left_Tra_Value < Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value < Black_Line)
    {
        Acebott.Move(Forward, 180);
    }
    if (Left_Tra_Value < Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value >= Black_Line)
    {
        Acebott.Move(Forward, 180);
    }
    if (Left_Tra_Value >= Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value < Black_Line)
    {
        Acebott.Move(Forward, 180);
    }
    /*else if (Left_Tra_Value >= Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value < Black_Line)
    {
        Acebott.Move(Contrarotate, 150);
    }*/
    else if (Left_Tra_Value >= Black_Line && Center_Tra_Value < Black_Line && Right_Tra_Value < Black_Line)
    {
        Acebott.Move(Contrarotate, 220);
    }
    else if (Left_Tra_Value < Black_Line && Center_Tra_Value < Black_Line && Right_Tra_Value >= Black_Line)
    {
        Acebott.Move(Clockwise, 220);
    }
    /*else if (Left_Tra_Value < Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value >= Black_Line)
    {
        Acebott.Move(Clockwise, 150);
    }*/
    /*else if (Left_Tra_Value >= Black_Line && Left_Tra_Value < Off_Road && Center_Tra_Value >= Black_Line && Center_Tra_Value < Off_Road && Right_Tra_Value >= Black_Line && Right_Tra_Value < Off_Road)
    {
        Acebott.Move(Forward, 180);
    }*/
    else if (Left_Tra_Value >= Off_Road && Center_Tra_Value >= Off_Road && Right_Tra_Value >= Off_Road)
    {
        Acebott.Move(Stop, 0);
    }
}

void model1_func()      // tracking model1
{
    //MYservo.write(90);
    Left_Tra_Value = analogRead(Left_Line);
    //Center_Tra_Value = analogRead(Center_Line);
    Right_Tra_Value = analogRead(Right_Line);
    //Serial.println(Left_Tra_Value);
    delay(5);
    if (Left_Tra_Value < Black_Line && Right_Tra_Value < Black_Line)
    {
        Acebott.Move(Forward, 130);
    }
    else if (Left_Tra_Value >= Black_Line && Right_Tra_Value < Black_Line)
    {
        Acebott.Move(Contrarotate, 150);
    }
    else if (Left_Tra_Value < Black_Line && Right_Tra_Value >= Black_Line)
    {
        Acebott.Move(Clockwise, 150);
    }
    else if (Left_Tra_Value >= Black_Line && Left_Tra_Value < Off_Road && Right_Tra_Value >= Black_Line && Right_Tra_Value < Off_Road)
    {
        Acebott.Move(Stop, 0);
    }
    else if (Left_Tra_Value >= Off_Road && Right_Tra_Value >= Off_Road)
    {
        Acebott.Move(Stop, 0);
    }
}

void Servo_Move(int angles)  //servo
{
  Yservo.write(angles);
  if (angles >= 180) angles = 180;
  if (angles <= 1) angles = 1;
  delay(10);
}

void Music_a()
{
    for(int x=0;x<length0;x++) 
    { 
        tone(Buzzer, tune0[x]);
        delay(500 * durt0[x]);
        noTone(Buzzer);
    }
}
void Music_b()
{
    for(int x=0;x<length1;x++) 
    { 
        tone(Buzzer, tune1[x]);
        delay(500 * durt1[x]);
        noTone(Buzzer);
    }
}
void Music_c()
{
    for(int x=0;x<length2;x++) 
    { 
        tone(Buzzer, tune2[x]);
        delay(500 * durt2[x]);
        noTone(Buzzer);
    }
}
void Music_d()
{
    for(int x=0;x<length3;x++) 
    { 
        tone(Buzzer, tune3[x]);
        delay(300 * durt3[x]);
        noTone(Buzzer);
    }
}
void Buzzer_run(int M)
{
    switch (M)
    {
        case 0x01:
            Music_a();
            break;
        case 0x02:
            Music_b();
            break;
        case 0x03:
            Music_c();
            break;
        case 0x04:
            Music_d();
            break;
        default:
            break;
    }
}

void runModule(int device)
{
  val = readBuffer(12);
  switch(device) 
  {
    case 0x0C:
    {   
      switch (val)
      {
        case 0x01:
            Acebott.Move(Forward, speeds);
            break;
        case 0x02:
            Acebott.Move(Backward, speeds);
            break;
        case 0x03:
            Acebott.Move(Move_Left, speeds);
            break;
        case 0x04:
            Acebott.Move(Move_Right, speeds);
            break;
        case 0x05:
            Acebott.Move(Top_Left, speeds);
            break;
        case 0x06:
            Acebott.Move(Bottom_Left, speeds);
            break;
        case 0x07:
            Acebott.Move(Top_Right, speeds);
            break;
        case 0x08:
            Acebott.Move(Bottom_Right, speeds);
            break;
        case 0x0A:
            Acebott.Move(Clockwise, speeds);
            break;
        case 0x09:
            Acebott.Move(Contrarotate, speeds);
            break;
        case 0x00:
            Acebott.Move(Stop, 0);
            break;
        default:
            break;
      }
    }break;
    case 0x02:
    {  
        Servo_Move(val);
    }break;
    case 0x03:
    {  
        Buzzer_run(val);
    }break;
    case 0x05:
    {    
        digitalWrite(LED_Module1,val);
        digitalWrite(LED_Module2,val);
    }break;
    case 0x08:
    {    
        digitalWrite(Shoot_PIN,HIGH);
        delay(150);
        digitalWrite(Shoot_PIN,LOW);
    }break;
    case 0x0D:
    {
        speeds = val;
    }break;
    case 0x0E:
    {
        // ARM PUSH
        pushSensorSequence();
    }break;
    case 0x0F:
    {
        // ARM RETRACT
        retractArmSequence();
    }break;
    case 0x10:
    {
        // ARM DIRECT CONTROL: value = angle
        moveArmSmooth(val);
    }break;
  }   
}
void parseData()
{ 
    isStart = false;
    int action = readBuffer(9);
    int device = readBuffer(10);
    switch (action)
    {
        case CMD_RUN:
            //callOK_Len01();
            function_mode = STANDBY;
            runModule(device);
            break;
        case CMD_STANDBY:
            //callOK_Len01();
            function_mode = STANDBY;
            Acebott.Move(Stop, 0);
            Yservo.write(90);
            break;
        case CMD_TRACK_1:
            //callOK_Len01();
            function_mode = TRACK_1;
            //Serial.write(0x01);
            break;
        case CMD_TRACK_2:
            //callOK_Len01();
            function_mode = TRACK_2;
            break;
        case CMD_AVOID:
            //callOK_Len01();
            function_mode = AVOID;
            break;
        case CMD_FOLLOW:
            //callOK_Len01();
            function_mode = FOLLOW;
            break;
        case CMD_ARM_PUSH:
            pushSensorSequence();
            break;
        case CMD_ARM_RETRACT:
            retractArmSequence();
            break;
        default:break;
    }
}

void RXpack_func() {
    // TCP receive function removed; BLE onWrite injects bytes into parser.
}

// Keep a no-op Receive_data so model functions that call it still compile.
void Receive_data() {
    // no-op: BLE writes are handled asynchronously via pushByteToParser
}

