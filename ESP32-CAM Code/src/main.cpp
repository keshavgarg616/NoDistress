#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// --- Configuration ---
const char *ssid = "SSID";
const char *password = "Password";

// Motor Pin Definitions (Using SD Card Pins)
#define LEFT_FWD 14
#define LEFT_REV 15
#define RIGHT_FWD 13
#define RIGHT_REV 12

// Camera model: AI-THINKER
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

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL; // New server handle for the stream

String distress_score = "0";
String emotion_score = "N/A";
String posture_score = "N/A";

// --- Motor Control Logic ---
void stopMotors()
{
    digitalWrite(LEFT_FWD, LOW);
    digitalWrite(LEFT_REV, LOW);
    digitalWrite(RIGHT_FWD, LOW);
    digitalWrite(RIGHT_REV, LOW);
}
void moveForward()
{
    digitalWrite(LEFT_FWD, HIGH);
    digitalWrite(LEFT_REV, LOW);
    digitalWrite(RIGHT_FWD, HIGH);
    digitalWrite(RIGHT_REV, LOW);
}
void moveBackward()
{
    digitalWrite(LEFT_FWD, LOW);
    digitalWrite(LEFT_REV, HIGH);
    digitalWrite(RIGHT_FWD, LOW);
    digitalWrite(RIGHT_REV, HIGH);
}
void turnLeft()
{
    digitalWrite(LEFT_FWD, LOW);
    digitalWrite(LEFT_REV, HIGH);
    digitalWrite(RIGHT_FWD, HIGH);
    digitalWrite(RIGHT_REV, LOW);
}
void turnRight()
{
    digitalWrite(LEFT_FWD, HIGH);
    digitalWrite(LEFT_REV, LOW);
    digitalWrite(RIGHT_FWD, LOW);
    digitalWrite(RIGHT_REV, HIGH);
}

// --- Handler for the Control Page ---
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    String html = R"rawliteral(
        <html>
        <head>
            <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
            <style>
                body { font-family: Arial; text-align: center; margin: 0; padding: 10px; background: #222; color: white;}
                img { width: 100%; max-width: 600px; border: 2px solid #555; border-radius: 8px; }
                .dpad { display: grid; grid-template-columns: 80px 80px 80px; gap: 10px; justify-content: center; margin-top: 20px; }
                button { width: 80px; height: 80px; font-size: 24px; font-weight: bold; border-radius: 10px; border: none; background: #007bff; color: white; cursor: pointer; user-select: none; -webkit-user-select: none;}
                button:active { background: #0056b3; }
                .empty { background: transparent; }
                .stats { margin-top: 20px; padding: 10px; background: #333; border-radius: 8px; display: inline-block; text-align: left; }
                .stats p { margin: 5px 0; font-size: 18px; }
            </style>
            <script>
                // Point the image source to Port 81 automatically
                window.onload = function() {
                    document.getElementById("stream-img").src = "http://" + window.location.hostname + ":81/stream";
                    setInterval(fetchData, 2000);
                }

                function sendCommand(cmd, event) {
                    if(event) event.preventDefault(); // Prevents ghost-clicks on mobile touch
                    fetch('/action?go=' + cmd);
                }
                
                function fetchData() {
                    fetch('/data')
                        .then(response => response.json())
                        .then(data => {
                            document.getElementById('wifi').innerText = data.wifi + " dBm";
                            document.getElementById('distress').innerText = data.distress + "%";
                            document.getElementById('emotion').innerText = data.emotion;
                            document.getElementById('posture').innerText = data.posture;
                        });
                }
            </script>
        </head>
        <body>
            <h2>Rover Cam</h2>
            <img id="stream-img" src="">
            <div class="stats">
                <p><strong>WiFi Signal:</strong> <span id="wifi">Loading...</span></p>
                <p><strong>Distress Rating:</strong> <span id="distress">Loading...</span></p>
                <p><strong>Emotion Score:</strong> <span id="emotion">Loading...</span></p>
                <p><strong>Posture Score:</strong> <span id="posture">Loading...</span></p>
            </div>
            <div class="dpad">
                <div class="empty"></div>
                <button onmousedown="sendCommand('F', event)" onmouseup="sendCommand('S', event)" ontouchstart="sendCommand('F', event)" ontouchend="sendCommand('S', event)">&#8593;</button>
                <div class="empty"></div>
                <button onmousedown="sendCommand('L', event)" onmouseup="sendCommand('S', event)" ontouchstart="sendCommand('L', event)" ontouchend="sendCommand('S', event)">&#8592;</button>
                <button onmousedown="sendCommand('S', event)" ontouchstart="sendCommand('S', event)">&#9632;</button>
                <button onmousedown="sendCommand('R', event)" onmouseup="sendCommand('S', event)" ontouchstart="sendCommand('R', event)" ontouchend="sendCommand('S', event)">&#8594;</button>
                <div class="empty"></div>
                <button onmousedown="sendCommand('B', event)" onmouseup="sendCommand('S', event)" ontouchstart="sendCommand('B', event)" ontouchend="sendCommand('S', event)">&#8595;</button>
                <div class="empty"></div>
            </div>
        </body>
        </html>
    )rawliteral";
    return httpd_resp_send(req, html.c_str(), HTTPD_RESP_USE_STRLEN);
}

// --- Handler for Motor API ---
static esp_err_t action_handler(httpd_req_t *req)
{
    char buf[100];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
    {
        char param[10];
        if (httpd_query_key_value(buf, "go", param, sizeof(param)) == ESP_OK)
        {
            char cmd = param[0];

            // Print to Serial Monitor for debugging
            Serial.print("Received Command: ");
            Serial.println(cmd);

            if (cmd == 'F')
                moveForward();
            else if (cmd == 'B')
                moveBackward();
            else if (cmd == 'L')
                turnLeft();
            else if (cmd == 'R')
                turnRight();
            else if (cmd == 'S')
                stopMotors();
        }
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

// --- Handler for Updating Scores from Python Backend ---
static esp_err_t update_scores_handler(httpd_req_t *req)
{
    char buf[100];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
    {
        char param[32];
        if (httpd_query_key_value(buf, "distress", param, sizeof(param)) == ESP_OK)
            distress_score = String(param);
        if (httpd_query_key_value(buf, "emotion", param, sizeof(param)) == ESP_OK)
            emotion_score = String(param);
        if (httpd_query_key_value(buf, "posture", param, sizeof(param)) == ESP_OK)
            posture_score = String(param);
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

// --- Handler for Fetching Data for the Webpage ---
static esp_err_t data_handler(httpd_req_t *req)
{
    char json_response[256];
    snprintf(json_response, sizeof(json_response), "{\"distress\":\"%s\",\"emotion\":\"%s\",\"posture\":\"%s\",\"wifi\":%d}",
             distress_score.c_str(), emotion_score.c_str(), posture_score.c_str(), WiFi.RSSI());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

// --- Handler for the Video Stream (Now completely isolated) ---
static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[128];

    res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
    if (res != ESP_OK)
        return res;

    while (true)
    {
        fb = esp_camera_fb_get();
        if (!fb)
            return ESP_FAIL;

        if (fb->format != PIXFORMAT_JPEG)
        {
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            esp_camera_fb_return(fb);
            fb = NULL;
            if (!jpeg_converted)
                return ESP_FAIL;
        }
        else
        {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        size_t hlen = snprintf((char *)part_buf, 128, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        if (res == ESP_OK)
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        if (res == ESP_OK)
            res = httpd_resp_send_chunk(req, "\r\n--123456789000000000000987654321\r\n", 35);

        if (fb)
        {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        else if (_jpg_buf)
        {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        if (res != ESP_OK)
            break;
    }
    return res;
}

// --- Handler to Capture a Single Frame ---
static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;

    fb = esp_camera_fb_get();
    if (!fb)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t fb_len = 0;
    if (fb->format == PIXFORMAT_JPEG)
    {
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    }
    else
    {
        uint8_t *buf = NULL;
        bool jpeg_converted = frame2jpg(fb, 80, &buf, &fb_len);
        if (!jpeg_converted)
        {
            esp_camera_fb_return(fb);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        res = httpd_resp_send(req, (const char *)buf, fb_len);
        free(buf);
    }
    esp_camera_fb_return(fb);
    return res;
}

// --- Multi-Server Startup ---
void startCameraServer()
{
    // 1. Start Control Server on Port 80
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL};
    httpd_uri_t action_uri = {.uri = "/action", .method = HTTP_GET, .handler = action_handler, .user_ctx = NULL};
    httpd_uri_t update_scores_uri = {.uri = "/update_scores", .method = HTTP_GET, .handler = update_scores_handler, .user_ctx = NULL};
    httpd_uri_t data_uri = {.uri = "/data", .method = HTTP_GET, .handler = data_handler, .user_ctx = NULL};
    httpd_uri_t capture_uri = {.uri = "/capture", .method = HTTP_GET, .handler = capture_handler, .user_ctx = NULL};

    if (httpd_start(&camera_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &action_uri);
        httpd_register_uri_handler(camera_httpd, &update_scores_uri);
        httpd_register_uri_handler(camera_httpd, &data_uri);
        httpd_register_uri_handler(camera_httpd, &capture_uri);
    }

    // 2. Start Stream Server on Port 81
    httpd_config_t stream_config = HTTPD_DEFAULT_CONFIG();
    stream_config.server_port = 81;
    stream_config.ctrl_port = 32769; // Must be altered so it doesn't conflict with Port 80's control socket
    httpd_uri_t stream_uri = {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL};

    if (httpd_start(&stream_httpd, &stream_config) == ESP_OK)
    {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}

void setup()
{
    Serial.begin(115200);

    // Initialize Motor Pins
    pinMode(LEFT_FWD, OUTPUT);
    pinMode(LEFT_REV, OUTPUT);
    pinMode(RIGHT_FWD, OUTPUT);
    pinMode(RIGHT_REV, OUTPUT);
    stopMotors();

    // Initialize Camera
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
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    if (esp_camera_init(&config) != ESP_OK)
    {
        Serial.println("Camera init failed");
        return;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s)
    {
        s->set_vflip(s, 1);
        s->set_hmirror(s, 1);
    }

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.println(WiFi.localIP());

    startCameraServer();
}

void loop()
{
    delay(1000);
}