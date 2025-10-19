#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp32-hal-ledc.h"
#include "sdkconfig.h"
// WebServer and BLE removed to reduce flash usage on ESP32-CAM

// Connect to the robot's soft-AP (smartcar) in STA mode.
// This SSID/password must match the robot's softAP configuration below.
const char *ssid = "smartcar";
const char *password = "12345678";
// no WebServer instance - using esp_http_server only
//Define the camera pin
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
// Set web page template in flash to reduce RAM usage
const char INDEX_HTML_P1[] PROGMEM = "<!DOCTYPE html> <html> <head> <meta charset=\"UTF-8\" /> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" /> <title>ESP32 CAM</title> <style> * { padding: 0px; margin: 0px; } body { display: flex; align-items: center; justify-content: center; height: 100vh; } .image-container { background-color: black; max-width: 100%; max-height: 200px; } .image-container img { display: block; margin: 0 auto; max-width: 100%; max-height: 100%; } </style> </head> <body> <div class=\"image-container\"> <img src=\"http://";
const char INDEX_HTML_P2[] PROGMEM = ":81/stream\" /> </div> </body> </html>";

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;


typedef struct
{
  size_t size;   // number of values used for filtering
  size_t index;  // current value index
  size_t count;  // value count
  int sum;
  int *values;  // array to be filled with values
} ra_filter_t;

//Set up web data transfer
static ra_filter_t ra_filter;
static ra_filter_t *ra_filter_init(ra_filter_t *filter, size_t sample_size) {
  memset(filter, 0, sizeof(ra_filter_t));
  filter->values = (int *)malloc(sample_size * sizeof(int));
  if (!filter->values) {
    return NULL;
  }
  memset(filter->values, 0, sample_size * sizeof(int));
  filter->size = sample_size;
  return filter;
}

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
static int ra_filter_run(ra_filter_t *filter, int value) {
  if (!filter->values) {
    return value;
  }
  filter->sum -= filter->values[filter->index];
  filter->values[filter->index] = value;
  filter->sum += filter->values[filter->index];
  filter->index++;
  filter->index = filter->index % filter->size;
  if (filter->count < filter->size) {
    filter->count++;
  }
  return filter->sum / filter->count;
}
#endif

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  struct timeval _timestamp;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[128];
  static int64_t last_frame = 0;
  if (!last_frame) {
    last_frame = esp_timer_get_time();
  }
  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "60");
  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      log_e("Camera capture failed");
      res = ESP_FAIL;
    } else {
      _timestamp.tv_sec = fb->timestamp.tv_sec;
      _timestamp.tv_usec = fb->timestamp.tv_usec;

      if (fb->format != PIXFORMAT_JPEG) {
        // Skip non-JPEG frames - conversion code removed to save flash
        esp_camera_fb_return(fb);
        fb = NULL;
        res = ESP_FAIL;
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      log_e("Send frame failed");
      break;
    }
    int64_t fr_end = esp_timer_get_time();
    int64_t frame_time = fr_end - last_frame;
    frame_time /= 1000;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
#endif
    log_i("MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)",
          (uint32_t)(_jpg_buf_len),
          (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
          avg_frame_time, 1000.0 / avg_frame_time);
  }
  return res;
}
#define HTTPD_DEFAULT_CONFIG() \
  { \
    .task_priority = tskIDLE_PRIORITY + 5, \
    .stack_size = 4096, \
    .core_id = tskNO_AFFINITY, \
    .server_port = 81, \
    .ctrl_port = 32768, \
    .max_open_sockets = 7, \
    .max_uri_handlers = 8, \
    .max_resp_headers = 8, \
    .backlog_conn = 5, \
    .lru_purge_enable = false, \
    .recv_wait_timeout = 5, \
    .send_wait_timeout = 5, \
    .global_user_ctx = NULL, \
    .global_user_ctx_free_fn = NULL, \
    .global_transport_ctx = NULL, \
    .global_transport_ctx_free_fn = NULL, \
    .enable_so_linger = false, \
    .linger_timeout = 0, \
    .open_fn = NULL, \
    .close_fn = NULL, \
    .uri_match_fn = NULL \
  }
//Define the camera control function
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 16;

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };
  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = [](httpd_req_t *req) -> esp_err_t {
      // assemble HTML: INDEX_HTML_P1 + IP + INDEX_HTML_P2
      String ipStr = WiFi.localIP().toString();
      char page[512];
      snprintf(page, sizeof(page), "%s%s%s", INDEX_HTML_P1, ipStr.c_str(), INDEX_HTML_P2);
      httpd_resp_set_type(req, "text/html");
      httpd_resp_send(req, page, strlen(page));
      return ESP_OK;
    },
    .user_ctx = NULL
  };
  ra_filter_init(&ra_filter, 20);

  log_i("Starting web server on port: '%d'", config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    httpd_register_uri_handler(camera_httpd, &index_uri);
  }

  // Start a lightweight root server on port 80 for app control/compatibility
  httpd_handle_t control_httpd = NULL;
  httpd_config_t control_config = HTTPD_DEFAULT_CONFIG();
  control_config.server_port = 80;
  if (httpd_start(&control_httpd, &control_config) == ESP_OK) {
    // root index at port 80 redirects/sends a small page linking to camera stream
    httpd_uri_t root80 = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = [](httpd_req_t *req) -> esp_err_t {
        String ipStr = WiFi.localIP().toString();
        char page[256];
        snprintf(page, sizeof(page), "<html><body><h3>ESP32-CAM</h3><p>Stream: http://%s:81/stream</p></body></html>", ipStr.c_str());
        httpd_resp_set_type(req, "text/html");
        httpd_resp_sendstr(req, page);
        return ESP_OK;
      },
      .user_ctx = NULL
    };
    httpd_register_uri_handler(control_httpd, &root80);
  }
}
//Example Initialize camera parameters
void setup() {
  Serial.begin(115200);
  // Ensure any AP is turned off, then go to STA mode (connect to existing WiFi network)
  WiFi.softAPdisconnect(true); // explicitly stop the soft-AP if it was previously running
  WiFi.mode(WIFI_STA);
  // Set a firmware static IP so this device joins the robot's AP at a known address
  // Camera will use 192.168.4.2 when connecting to the robot's AP
  IPAddress localIP(192,168,4,2);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);
  if (!WiFi.config(localIP, gateway, subnet)) {
    Serial.println("Failed to configure static IP, will continue with DHCP");
  }
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Print MAC address so user can reserve DHCP on router
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());

  // Using only esp_http_server for streaming; index and stream endpoints are handled
  // by startCameraServer(). BLE and WebServer code removed to save flash.

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  // Reduce XCLK and default frame size to decrease code and memory footprint
  config.xclk_freq_hz = 10000000;
  config.frame_size = FRAMESIZE_SVGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  // config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 30;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  // Keep a simple configuration: one frame buffer in PSRAM if available; else DRAM
  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.fb_count = 1;
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count = 1;
  }
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  sensor_t *s = esp_camera_sensor_get();
  // drop down frame size for higher initial frame rate
  //s->set_framesize(s, FRAMESIZE_SXGA); //Byte length sample value: 60000
    s->set_framesize(s, FRAMESIZE_SVGA); //Byte length sample value: 40000
  //s->set_framesize(s, FRAMESIZE_QVGA);  // Byte length sample value: 10000
    s->set_vflip(s, 1);//Picture orientation settings (up and down)
    s->set_hmirror(s, 1);//Picture orientation settings (left and right)
    startCameraServer();
}
void loop() {
  // Nothing to do here; esp_http_server handles requests in its own tasks.
  delay(100);
}
