#include "webserver.h"
#include <ElegantOTA.h>

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include "msp.h"
#include "msptypes.h"
#include "debug.h"

static const uint8_t DNS_PORT = 53;
static IPAddress netMsk(255, 255, 255, 0);
static DNSServer dnsServer;
static IPAddress ipAddress;
static AsyncWebServer server(80);
static AsyncEventSource events("/events");

static const char *wifi_hostname = "plt";
static const char *wifi_ap_ssid_prefix = "PhobosLT";
static const char *wifi_ap_password = "phoboslt";
static const char *wifi_ap_address = "20.0.0.1";
String wifi_ap_ssid;
esp_now_peer_info_t peerInfo;
uint8_t targetAddress[6];
uint8_t uniAddress[6];
MSP msp;

int sendMSPViaEspnow(mspPacket_t *packet)
{
  int esp_err = -1;
  uint8_t packetSize = msp.getTotalPacketSize(packet);
  uint8_t nowDataOutput[packetSize];

  uint8_t result = msp.convertToByteArray(packet, nowDataOutput);
  DEBUG("Sending MSP data: ");
  for (uint16_t i = 0; i < packetSize; ++i) {
    DEBUG("%u ", nowDataOutput[i]);
 }
    DEBUG("\n");

  if (!result)
  {
    return esp_err;
  }

    esp_err = esp_now_send(targetAddress, (uint8_t *) &nowDataOutput, packetSize);
    if (esp_err != ESP_OK) {
        DEBUG("Error sending ESP-NOW data: %s\n", esp_err_to_name(esp_err));
    } else {
        DEBUG("ESP-NOW data sent successfully\n");
    }

  return esp_err;
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
        DEBUG("SENT OK\n");
    else
        DEBUG("SENT FAIL\n");
}

#define MSP_ELRS_SET_OSD_BEAT                        0x01
#define MSP_ELRS_SET_OSD_CLEAR                       0x02
#define MSP_ELRS_SET_OSD_WRITE                       0x03
#define MSP_ELRS_SET_OSD_SHOW                        0x04

mspPacket_t packet;
void sendMSPOSDMessage(byte subfunction, char msg[]){

    packet.reset();
    packet.makeCommand();
    packet.function = MSP_ELRS_SET_OSD;

    packet.addByte(subfunction);
    packet.addByte(3);    
    packet.addByte(3);    
    packet.addByte(0); 
    packet.addByte(0); 
    if (subfunction == MSP_ELRS_SET_OSD_WRITE)
    {
        for (int i = 0; i < strlen(msg); i++)
        {
            packet.addByte(msg[i]);
        }
    }
    packet.addByte(0); 

    sendMSPViaEspnow(&packet);
    usleep(100);
}

void sendElrsHello(){
    char buf[] = "PLT TIMER CONNECTED!";
    sendMSPOSDMessage(MSP_ELRS_SET_OSD_CLEAR, buf);
    sendMSPOSDMessage(MSP_ELRS_SET_OSD_WRITE, buf);
    sendMSPOSDMessage(MSP_ELRS_SET_OSD_SHOW, buf);
}

void getLapString(uint32_t lapTime, char *output) {
    uint32_t min   = lapTime / 1000 / 60;
    uint32_t seconds = lapTime / 1000;
    uint32_t milis = lapTime % 1000;

    sprintf(output, "%02u:%02u.%03u", min, seconds, milis);
}

void sendElrsEvent(uint32_t lapTime) {
    char lapTimeMsg[10];
    getLapString(lapTime, lapTimeMsg);

    char buf[20];
    snprintf(buf, sizeof(buf), "TIME: %s", lapTimeMsg);
    DEBUG("Sending ELRS OSD message: %s\n", buf);
    sendMSPOSDMessage(MSP_ELRS_SET_OSD_CLEAR, buf);
    sendMSPOSDMessage(MSP_ELRS_SET_OSD_WRITE, buf);
    sendMSPOSDMessage(MSP_ELRS_SET_OSD_SHOW, buf);
}

void getBindingUID(char *phrase, uint8_t *bindingUID)
{
    String bindingPhraseFull = String("-DMY_BINDING_PHRASE=\"") + phrase + "\"";
    int len = bindingPhraseFull.length();
    char bindingPhrase[len + 1];
    bindingPhraseFull.toCharArray(bindingPhrase, len + 1);

    unsigned char md5Hash[16];
    MD5Builder md5;
    md5.begin();
    md5.add(bindingPhrase);
    md5.calculate();
    md5.getBytes(md5Hash);

    uint8_t uidBytes[6];
    memcpy(uidBytes, md5Hash, 6);
    DEBUG("\nBinding phrase uid: ");
    for (int i = 0; i < 6; i++)
    {
        bindingUID[i] = uidBytes[i];
        DEBUG("%u,", uidBytes[i]);
    }
    DEBUG(". Hello :) \n");
}


void Webserver::init(Config *config, LapTimer *lapTimer, BatteryMonitor *batMonitor, Buzzer *buzzer, Led *l) {

    ipAddress.fromString(wifi_ap_address);

    conf = config;
    timer = lapTimer;
    monitor = batMonitor;
    buz = buzzer;
    led = l;

    wifi_ap_ssid = String(wifi_ap_ssid_prefix) + "_" + WiFi.macAddress().substring(WiFi.macAddress().length() - 6);
    wifi_ap_ssid.replace(":", "");

    WiFi.persistent(false);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);
    if (conf->getSsid()[0] == 0) {
        changeMode = WIFI_AP;
    } else {
        changeMode = WIFI_STA;
    }
    changeTimeMs = millis();
    lastStatus = WL_DISCONNECTED;
}

void Webserver::sendRssiEvent(uint8_t rssi) {
    if (!servicesStarted) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", rssi);
    events.send(buf, "rssi");
}


void Webserver::sendLaptimeEvent(uint32_t lapTime) {
    if (!servicesStarted) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", lapTime);
    events.send(buf, "lap");
}

void Webserver::handleWebUpdate(uint32_t currentTimeMs) {
    if (timer->isLapAvailable()) {
        sendLaptimeEvent(timer->getLapTime());
        sendElrsEvent(timer->getLapTime());
    }

    if (sendRssi && ((currentTimeMs - rssiSentMs) > WEB_RSSI_SEND_TIMEOUT_MS)) {
        sendRssiEvent(timer->getRssi());
        rssiSentMs = currentTimeMs;
    }

    wl_status_t status = WiFi.status();

    if (status != lastStatus && wifiMode == WIFI_STA) {
        DEBUG("WiFi status = %u\n", status);
        switch (status) {
            case WL_NO_SSID_AVAIL:
            case WL_CONNECT_FAILED:
            case WL_CONNECTION_LOST:
                changeTimeMs = currentTimeMs;
                changeMode = WIFI_AP;
                break;
            case WL_DISCONNECTED:  // try reconnection
                changeTimeMs = currentTimeMs;
                break;
            case WL_CONNECTED:
                buz->beep(200);
                led->off();
                wifiConnected = true;
                break;
            default:
                break;
        }
        lastStatus = status;
    }
    if (status != WL_CONNECTED && wifiMode == WIFI_STA && (currentTimeMs - changeTimeMs) > WIFI_CONNECTION_TIMEOUT_MS) {
        changeTimeMs = currentTimeMs;
        if (!wifiConnected) {
            changeMode = WIFI_AP;  // if we didnt manage to ever connect to wifi network
        } else {
            DEBUG("WiFi Connection failed, reconnecting\n");
            WiFi.reconnect();
            startServices();
            buz->beep(100);
            led->blink(200);
        }
    }
    if (changeMode != wifiMode && changeMode != WIFI_OFF && (currentTimeMs - changeTimeMs) > WIFI_RECONNECT_TIMEOUT_MS) {
        switch (changeMode) {
            case WIFI_AP:
                DEBUG("Changing to WiFi AP mode\n");

                WiFi.disconnect();
                WiFi.setTxPower(WIFI_POWER_19_5dBm);
                WiFi.setHostname(wifi_hostname);
                WiFi.softAPConfig(ipAddress, ipAddress, netMsk);
                esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
                esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
                WiFi.softAP(wifi_ap_ssid.c_str(), wifi_ap_password);
                WiFi.mode(WIFI_AP_STA);
                if (conf->getElrsBindPhrase()[0] != 0)
                {

                    DEBUG("Enablind ESP NOW for backpack\n");
                    uint8_t bindingUID[6];
                    getBindingUID(conf->getElrsBindPhrase(), bindingUID);

                    memcpy(targetAddress, bindingUID, sizeof(targetAddress));
                    memcpy(uniAddress, bindingUID, sizeof(uniAddress));
                    uniAddress[0] = uniAddress[0] & ~0x01;

                    if (esp_now_init() != 0)
                    {
                      DEBUG("Error initializing ESP-NOW\n");
                      return;
                    }
                    esp_wifi_set_mac(WIFI_IF_STA, uniAddress);
                    memset(&peerInfo, 0, sizeof(peerInfo));
                    memcpy(peerInfo.peer_addr, targetAddress, 6);
                    peerInfo.channel = 1;
                    peerInfo.encrypt = false;
                    if (esp_now_add_peer(&peerInfo) != ESP_OK)
                    {
                        DEBUG("ESP-NOW failed to add peer\n");
                        return;
                    }
    
                    esp_now_register_send_cb(OnDataSent);
                    sendElrsHello();
                }

                DEBUG("Start services\n");
                startServices();
                buz->beep(1000);
                led->on(1000);
                break;
            case WIFI_STA:
                DEBUG("Connecting to WiFi network\n");
                wifiMode = WIFI_STA;
                WiFi.setHostname(wifi_hostname);  // hostname must be set before the mode is set to STA
                WiFi.mode(wifiMode);
                changeTimeMs = currentTimeMs;
                WiFi.begin(conf->getSsid(), conf->getPassword());
                startServices();
                led->blink(200);
            default:
                break;
        }

        changeMode = WIFI_OFF;
    }

    if (servicesStarted) {
        dnsServer.processNextRequest();
    }
}

/** Is this an IP? */
static boolean isIp(String str) {
    for (size_t i = 0; i < str.length(); i++) {
        int c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9')) {
            return false;
        }
    }
    return true;
}

/** IP to String? */
static String toStringIp(IPAddress ip) {
    String res = "";
    for (int i = 0; i < 3; i++) {
        res += String((ip >> (8 * i)) & 0xFF) + ".";
    }
    res += String(((ip >> 8 * 3)) & 0xFF);
    return res;
}

static bool captivePortal(AsyncWebServerRequest *request) {
    extern const char *wifi_hostname;

    if (!isIp(request->host()) && request->host() != (String(wifi_hostname) + ".local")) {
        DEBUG("Request redirected to captive portal\n");
        request->redirect(String("http://") + toStringIp(request->client()->localIP()));
        return true;
    }
    return false;
}

static void handleRoot(AsyncWebServerRequest *request) {
    if (captivePortal(request)) {  // If captive portal redirect instead of displaying the page.
        return;
    }
    request->send(LittleFS, "/index.html", "text/html");
}

static void handleNotFound(AsyncWebServerRequest *request) {
    if (captivePortal(request)) {  // If captive portal redirect instead of displaying the error page.
        return;
    }
    String message = F("File Not Found\n\n");
    message += F("URI: ");
    message += request->url();
    message += F("\nMethod: ");
    message += (request->method() == HTTP_GET) ? "GET" : "POST";
    message += F("\nArguments: ");
    message += request->args();
    message += F("\n");

    for (uint8_t i = 0; i < request->args(); i++) {
        message += String(F(" ")) + request->argName(i) + F(": ") + request->arg(i) + F("\n");
    }
    AsyncWebServerResponse *response = request->beginResponse(404, "text/plain", message);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "-1");
    request->send(response);
}

static bool startLittleFS() {
    if (!LittleFS.begin()) {
        DEBUG("LittleFS mount failed\n");
        return false;
    }
    DEBUG("LittleFS mounted sucessfully\n");
    return true;
}

static void startMDNS() {
    if (!MDNS.begin(wifi_hostname)) {
        DEBUG("Error starting mDNS\n");
        return;
    }

    String instance = String(wifi_hostname) + "_" + WiFi.macAddress();
    instance.replace(":", "");
    MDNS.setInstanceName(instance);
    MDNS.addService("http", "tcp", 80);
}

void Webserver::startServices() {
    if (servicesStarted) {
        MDNS.end();
        startMDNS();
        return;
    }

    startLittleFS();

    server.on("/", handleRoot);
    server.on("/generate_204", handleRoot);  // handle Andriod phones doing shit to detect if there is 'real' internet and possibly dropping conn.
    server.on("/gen_204", handleRoot);
    server.on("/library/test/success.html", handleRoot);
    server.on("/hotspot-detect.html", handleRoot);
    server.on("/connectivity-check.html", handleRoot);
    server.on("/check_network_status.txt", handleRoot);
    server.on("/ncsi.txt", handleRoot);
    server.on("/fwlink", handleRoot);

    server.on("/status", [this](AsyncWebServerRequest *request) {
        char buf[1024];
        char configBuf[256];
        conf->toJsonString(configBuf);
        float voltage = (float)monitor->getBatteryVoltage() / 10;
        const char *format =
            "\
Heap:\n\
\tFree:\t%i\n\
\tMin:\t%i\n\
\tSize:\t%i\n\
\tAlloc:\t%i\n\
LittleFS:\n\
\tUsed:\t%i\n\
\tTotal:\t%i\n\
Chip:\n\
\tModel:\t%s Rev %i, %i Cores, SDK %s\n\
\tFlashSize:\t%i\n\
\tFlashSpeed:\t%iMHz\n\
\tCPU Speed:\t%iMHz\n\
Network:\n\
\tIP:\t%s\n\
\tMAC:\t%s\n\
EEPROM:\n\
%s\n\
Battery Voltage:\t%0.1fv";

        snprintf(buf, sizeof(buf), format,
                 ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getHeapSize(), ESP.getMaxAllocHeap(), LittleFS.usedBytes(), LittleFS.totalBytes(),
                 ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getSdkVersion(), ESP.getFlashChipSize(), ESP.getFlashChipSpeed() / 1000000, getCpuFrequencyMhz(),
                 WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(), configBuf, voltage);
        request->send(200, "text/plain", buf);
        led->on(200);
    });

    server.on("/timer/start", HTTP_POST, [this](AsyncWebServerRequest *request) {
        timer->start();
        request->send(200, "application/json", "{\"status\": \"OK\"}");
    });

    server.on("/timer/stop", HTTP_POST, [this](AsyncWebServerRequest *request) {
        timer->stop();
        request->send(200, "application/json", "{\"status\": \"OK\"}");
    });

    server.on("/timer/rssiStart", HTTP_POST, [this](AsyncWebServerRequest *request) {
        sendRssi = true;
        request->send(200, "application/json", "{\"status\": \"OK\"}");
        led->on(200);
    });

    server.on("/timer/rssiStop", HTTP_POST, [this](AsyncWebServerRequest *request) {
        sendRssi = false;
        request->send(200, "application/json", "{\"status\": \"OK\"}");
        led->on(200);
    });

    server.on("/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        conf->toJson(*response);
        request->send(response);
        led->on(200);
    });

    AsyncCallbackJsonWebHandler *configJsonHandler = new AsyncCallbackJsonWebHandler("/config", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject jsonObj = json.as<JsonObject>();
#ifdef DEBUG_OUT
        serializeJsonPretty(jsonObj, DEBUG_OUT);
        DEBUG("\n");
#endif
        conf->fromJson(jsonObj);
        request->send(200, "application/json", "{\"status\": \"OK\"}");
        led->on(200);
    });

    server.serveStatic("/", LittleFS, "/").setCacheControl("max-age=600");

    events.onConnect([this](AsyncEventSourceClient *client) {
        if (client->lastId()) {
            DEBUG("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        }
        client->send("start", NULL, millis(), 1000);
        led->on(200);
    });

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Max-Age", "600");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

    server.onNotFound(handleNotFound);

    server.addHandler(&events);
    server.addHandler(configJsonHandler);

    ElegantOTA.setAutoReboot(true);
    ElegantOTA.begin(&server);

    server.begin();

    dnsServer.start(DNS_PORT, "*", ipAddress);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);

    startMDNS();

    servicesStarted = true;
}
