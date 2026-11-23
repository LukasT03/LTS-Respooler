// ------------------ Local network and LTS Cloud -------------------
// Same file in both board sketches. Next to BLE, the app reaches the board over a
// WebSocket on the local network (ws://<board>/ws, found via mDNS as _lts-respooler._tcp)
// and, with remote access on, over LTS Cloud. All three carry the same JSON.

#define CLOUD_HOST "cloud.lts-design.com"
#define CLOUD_PATH "/v1/board"
#define CLOUD_IDLE_STATUS_MS 60000  // status interval while no app watches over the cloud
#define CLOUD_RX_TIMEOUT_MS 75000   // the server pings every 30 s
#define CLOUD_RETRY_MIN_MS 5000
#define CLOUD_RETRY_MAX_MS 300000
#define NET_MAX_MESSAGE 1024
#define LOCAL_MAX_CLIENTS 4

// ISRG Root X1 and X2 (Let's Encrypt), the certificate chain of LTS Cloud ends at both
static const char cloudRootCA[] =
  "-----BEGIN CERTIFICATE-----\n"
  "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\n"
  "-----END CERTIFICATE-----\n"
  "-----BEGIN CERTIFICATE-----\n"
  "MIICGzCCAaGgAwIBAgIQQdKd0XLq7qeAwSxs6S+HUjAKBggqhkjOPQQDAzBPMQsw\n"
  "CQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2gg\n"
  "R3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMjAeFw0yMDA5MDQwMDAwMDBaFw00\n"
  "MDA5MTcxNjAwMDBaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5ldCBT\n"
  "ZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgyMHYw\n"
  "EAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0HttwW\n"
  "+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7AlF9\n"
  "ItgKbppbd9/w+kHsOdx1ymgHDB/qo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0T\n"
  "AQH/BAUwAwEB/zAdBgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwCgYIKoZI\n"
  "zj0EAwMDaAAwZQIwe3lORlCEwkSHRhtFcP9Ymd70/aTSVaYgLXTWNLxBo1BfASdW\n"
  "tL4ndQavEi51mI38AjEAi/V3bNTIZargCyzuFJ0nN6T5U6VR5CmD1/iQMVtCnwr1\n"
  "/q4AaOeMSQ+2b1tbFfLn\n"
  "-----END CERTIFICATE-----\n";

struct NetCommand {
  char* json;
  CommandSource src;
};

char boardIdHex[17];
char boardSecret[33];
QueueHandle_t netCommandQueue = nullptr;
volatile bool netStatusPending = false;

httpd_handle_t localServer = nullptr;
int localClientFds[LOCAL_MAX_CLIENTS];
volatile int localClientCount = 0;

NetworkClientSecure* cloudClient = nullptr;
SemaphoreHandle_t cloudOutboxMutex = nullptr;
String cloudOutbox;
volatile bool cloudConnected = false;
volatile int cloudApps = 0;
unsigned long cloudLastStatus = 0;

// ---------------------- Identity -----------------------
// The ID comes from the chip and survives every flash. The secret lives in NVS, a USB
// flash erases it and the app then sends the new one to LTS Cloud (/v1/rekey).
void initBoardIdentity() {
  uint64_t mac = ESP.getEfuseMac();
  uint8_t hash[32];
  mbedtls_sha256((const uint8_t*)&mac, 6, hash, 0);
  for (int i = 0; i < 8; i++) sprintf(boardIdHex + i * 2, "%02x", hash[i]);

  prefs.begin("respooler", false);
  String secret = prefs.getString("secret", "");
  if (secret.length() != 32) {
    uint8_t raw[16];
    esp_fill_random(raw, sizeof(raw));
    char hex[33];
    for (int i = 0; i < 16; i++) sprintf(hex + i * 2, "%02x", raw[i]);
    secret = hex;
    prefs.putString("secret", secret);
  }
  prefs.end();
  strlcpy(boardSecret, secret.c_str(), sizeof(boardSecret));
}

const char* boardId() {
  return boardIdHex;
}

// ID and secret, only over BLE and the local network
void sendBoardKey() {
  StaticJsonDocument<128> doc;
  doc["ID"] = boardIdHex;
  doc["KEY"] = boardSecret;
  String jsonOut;
  serializeJson(doc, jsonOut);
  notifyApps(jsonOut);
}

// --------------------- Command queue ---------------------
// Commands from the network run in the main loop, takes ownership of json
void queueNetCommand(char* json, CommandSource src) {
  NetCommand c = { json, src };
  if (!netCommandQueue || xQueueSend(netCommandQueue, &c, 0) != pdTRUE) free(json);
}

void networkPoll() {
  NetCommand c;
  while (netCommandQueue && xQueueReceive(netCommandQueue, &c, 0) == pdTRUE) {
    handleCommand(std::string(c.json), c.src);
    free(c.json);
  }
  if (netStatusPending) {
    netStatusPending = false;
    sendStatus(true);
  }
}

// --------------------- Local WebSocket -------------------
esp_err_t localWsHandler(httpd_req_t* req) {
  if (req->method == HTTP_GET) {
    int fd = httpd_req_to_sockfd(req);
    for (int i = 0; i < LOCAL_MAX_CLIENTS; i++) {
      if (localClientFds[i] < 0) {
        localClientFds[i] = fd;
        localClientCount++;
        break;
      }
    }
    netStatusPending = true;
    return ESP_OK;
  }

  httpd_ws_frame_t frame = {};
  esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
  if (err != ESP_OK) return err;
  if (frame.len == 0) return ESP_OK;
  if (frame.len > NET_MAX_MESSAGE) return ESP_FAIL;

  char* buf = (char*)malloc(frame.len + 1);
  if (!buf) return ESP_ERR_NO_MEM;
  frame.payload = (uint8_t*)buf;
  err = httpd_ws_recv_frame(req, &frame, frame.len);
  if (err != ESP_OK || frame.type != HTTPD_WS_TYPE_TEXT) {
    free(buf);
    return err;
  }
  buf[frame.len] = 0;
  queueNetCommand(buf, SRC_LOCAL);
  return ESP_OK;
}

void localClose(httpd_handle_t hd, int fd) {
  for (int i = 0; i < LOCAL_MAX_CLIENTS; i++) {
    if (localClientFds[i] == fd) {
      localClientFds[i] = -1;
      localClientCount--;
    }
  }
  close(fd);
}

struct LocalFrame {
  char* data;
  size_t len;
};

void localSendWork(void* arg) {
  LocalFrame* f = (LocalFrame*)arg;
  for (int i = 0; i < LOCAL_MAX_CLIENTS; i++) {
    int fd = localClientFds[i];
    if (fd < 0 || httpd_ws_get_fd_info(localServer, fd) != HTTPD_WS_CLIENT_WEBSOCKET) continue;
    httpd_ws_frame_t frame = {};
    frame.final = true;
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = (uint8_t*)f->data;
    frame.len = f->len;
    httpd_ws_send_frame_async(localServer, fd, &frame);
  }
  free(f->data);
  free(f);
}

void localSend(const String& json) {
  if (!localServer || localClientCount <= 0) return;
  LocalFrame* f = (LocalFrame*)malloc(sizeof(LocalFrame));
  char* data = strdup(json.c_str());
  if (!f || !data) {
    free(f);
    free(data);
    return;
  }
  f->data = data;
  f->len = json.length();
  if (httpd_queue_work(localServer, localSendWork, f) != ESP_OK) {
    free(data);
    free(f);
  }
}

void startLocalServer() {
  for (int i = 0; i < LOCAL_MAX_CLIENTS; i++) localClientFds[i] = -1;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_open_sockets = LOCAL_MAX_CLIENTS;
  config.lru_purge_enable = true;
  config.keep_alive_enable = true;
  config.close_fn = localClose;
  if (httpd_start(&localServer, &config) != ESP_OK) {
    localServer = nullptr;
    return;
  }
  httpd_uri_t ws = {};
  ws.uri = "/ws";
  ws.method = HTTP_GET;
  ws.handler = localWsHandler;
  ws.is_websocket = true;
  httpd_register_uri_handler(localServer, &ws);

  String host = String("lts-respooler-") + String(boardIdHex).substring(0, 4);
  if (MDNS.begin(host.c_str())) {
    MDNS.addService("lts-respooler", "tcp", 80);
    MDNS.addServiceTxt("lts-respooler", "tcp", "id", (const char*)boardIdHex);
    MDNS.addServiceTxt("lts-respooler", "tcp", "board", BOARD_NAME);
    MDNS.addServiceTxt("lts-respooler", "tcp", "fw", FIRMWARE_VERSION);
  }
}

// ----------------------- LTS Cloud -----------------------
bool cloudRead(uint8_t* buf, size_t len, unsigned long timeoutMs) {
  unsigned long start = millis();
  size_t got = 0;
  while (got < len) {
    int n = cloudClient->read(buf + got, len - got);
    if (n > 0) {
      got += n;
      continue;
    }
    if (!cloudClient->connected() || millis() - start > timeoutMs) return false;
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
  return true;
}

bool cloudReadLine(String& line) {
  line = "";
  uint8_t c;
  while (cloudRead(&c, 1, 10000)) {
    if (c == '\n') return true;
    if (c != '\r' && line.length() < 256) line += (char)c;
  }
  return false;
}

// Frames from a client are masked
bool cloudWriteFrame(uint8_t opcode, const uint8_t* data, size_t len) {
  uint8_t header[8];
  size_t h = 0;
  header[h++] = 0x80 | opcode;
  if (len < 126) {
    header[h++] = 0x80 | len;
  } else {
    header[h++] = 0x80 | 126;
    header[h++] = len >> 8;
    header[h++] = len & 0xFF;
  }
  uint8_t* mask = header + h;
  esp_fill_random(mask, 4);
  h += 4;

  uint8_t* buf = (uint8_t*)malloc(h + len);
  if (!buf) return false;
  memcpy(buf, header, h);
  for (size_t i = 0; i < len; i++) buf[h + i] = data[i] ^ mask[i % 4];
  bool ok = cloudClient->write(buf, h + len) == h + len;
  free(buf);
  return ok;
}

// Reads one frame, false when the connection should be dropped
bool cloudReadFrame() {
  uint8_t head[2];
  if (!cloudRead(head, 2, 5000)) return false;
  uint8_t opcode = head[0] & 0x0F;
  size_t len = head[1] & 0x7F;
  if (len == 126) {
    uint8_t ext[2];
    if (!cloudRead(ext, 2, 5000)) return false;
    len = (ext[0] << 8) | ext[1];
  } else if (len == 127 || (head[1] & 0x80)) {
    return false;  // the server neither masks nor sends anything that large
  }
  if (len > NET_MAX_MESSAGE) return false;

  char* buf = (char*)malloc(len + 1);
  if (!buf) return false;
  if (!cloudRead((uint8_t*)buf, len, 5000)) {
    free(buf);
    return false;
  }
  buf[len] = 0;

  switch (opcode) {
    case 0x1: {
      StaticJsonDocument<64> doc;
      if (strncmp(buf, "{\"APPS\"", 7) == 0 && deserializeJson(doc, buf) == DeserializationError::Ok) {
        int apps = doc["APPS"] | 0;
        if (apps > 0 && cloudApps == 0) netStatusPending = true;
        cloudApps = apps;
        free(buf);
      } else {
        queueNetCommand(buf, SRC_CLOUD);
      }
      return true;
    }
    case 0x8:
      free(buf);
      return false;
    case 0x9: {
      bool ok = cloudWriteFrame(0xA, (uint8_t*)buf, len);
      free(buf);
      return ok;
    }
    default:
      free(buf);
      return true;
  }
}

bool cloudConnect() {
  cloudClient->setCACert(cloudRootCA);
  if (!cloudClient->connect(CLOUD_HOST, 443)) return false;

  uint8_t keyRaw[16];
  esp_fill_random(keyRaw, sizeof(keyRaw));
  unsigned char key[32];
  size_t keyLen = 0;
  mbedtls_base64_encode(key, sizeof(key), &keyLen, keyRaw, sizeof(keyRaw));
  key[keyLen] = 0;

  String request = String("GET " CLOUD_PATH " HTTP/1.1\r\n"
                          "Host: " CLOUD_HOST "\r\n"
                          "Upgrade: websocket\r\n"
                          "Connection: Upgrade\r\n"
                          "Sec-WebSocket-Version: 13\r\n"
                          "User-Agent: LTS-Respooler/" FIRMWARE_VERSION "\r\n"
                          "Sec-WebSocket-Key: ")
                   + (const char*)key + "\r\nAuthorization: Board " + boardIdHex + ":" + boardSecret + "\r\n\r\n";
  if (cloudClient->print(request) != request.length()) return false;

  String line;
  if (!cloudReadLine(line) || !line.startsWith("HTTP/1.1 101")) return false;
  while (cloudReadLine(line)) {
    if (line.length() == 0) return true;
  }
  return false;
}

void cloudDisconnect() {
  cloudClient->stop();
  cloudConnected = false;
  cloudApps = 0;
}

void cloudTask(void* parameter) {
  unsigned long retryDelay = CLOUD_RETRY_MIN_MS;
  unsigned long nextAttempt = 0;
  unsigned long lastRx = 0;

  for (;;) {
    bool wanted = remoteAccess && WiFi.status() == WL_CONNECTED && !otaInProgress;
    if (!wanted) {
      if (cloudConnected) cloudDisconnect();
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    if (!cloudConnected) {
      if ((long)(millis() - nextAttempt) < 0) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        continue;
      }
      if (cloudConnect()) {
        cloudConnected = true;
        retryDelay = CLOUD_RETRY_MIN_MS;
        lastRx = millis();
        netStatusPending = true;
      } else {
        cloudDisconnect();
        nextAttempt = millis() + retryDelay;
        retryDelay = min(retryDelay * 2, (unsigned long)CLOUD_RETRY_MAX_MS);
      }
      continue;
    }

    String out;
    xSemaphoreTake(cloudOutboxMutex, portMAX_DELAY);
    out = cloudOutbox;
    cloudOutbox = "";
    xSemaphoreGive(cloudOutboxMutex);

    bool ok = out.length() == 0 || cloudWriteFrame(0x1, (const uint8_t*)out.c_str(), out.length());
    while (ok && cloudClient->available() > 0) {
      ok = cloudReadFrame();
      lastRx = millis();
    }
    if (!ok || !cloudClient->connected() || millis() - lastRx > CLOUD_RX_TIMEOUT_MS) {
      cloudDisconnect();
      nextAttempt = millis() + retryDelay;
      continue;
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

bool networkCloudConnected() {
  return cloudConnected;
}

// --------------------------- Status ----------------------------
// Over the cloud, status streams only while an app watches. Otherwise it goes out
// on changes (forced) and once a minute.
bool cloudWantsStatus(bool force) {
  return cloudConnected && (force || cloudApps > 0 || millis() - cloudLastStatus >= CLOUD_IDLE_STATUS_MS);
}

bool networkWantsStatus(bool force) {
  return localClientCount > 0 || cloudWantsStatus(force);
}

void networkSendStatus(const String& json, bool force) {
  localSend(json);
  if (cloudWantsStatus(force)) {
    cloudLastStatus = millis();
    xSemaphoreTake(cloudOutboxMutex, portMAX_DELAY);
    cloudOutbox = json;
    xSemaphoreGive(cloudOutboxMutex);
  }
}

// ---------------------------- Setup ----------------------------
// After WiFi.mode() and loadSettings()
void networkSetup() {
  initBoardIdentity();
  netCommandQueue = xQueueCreate(8, sizeof(NetCommand));
  cloudOutboxMutex = xSemaphoreCreateMutex();
  cloudClient = new NetworkClientSecure();
  startLocalServer();
  xTaskCreatePinnedToCore(cloudTask, "Cloud", 8192, NULL, 1, NULL, 0);
}
