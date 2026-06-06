#include "config_manager.h"
#include <DNSServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <mbedtls/base64.h>
#include "portal_cert.h"

ConfigManager configManager;

static DNSServer* dnsServer = nullptr;
static const uint32_t PORTAL_HEAP_LOG_INTERVAL_MS = 30000;

// HTTPS server instance
static httpsserver::HTTPSServer* httpsServer = nullptr;
static httpsserver::SSLCert* sslCert = nullptr;

// Handler functions for HTTPS server
void handleRoot(httpsserver::HTTPRequest * req, httpsserver::HTTPResponse * res) {
    res->setHeader("Content-Type", "text/html");
    res->print(configManager.portalHTML());
}

void handleScan(httpsserver::HTTPRequest * req, httpsserver::HTTPResponse * res) {
    res->setHeader("Content-Type", "application/json");
    res->print(configManager.scanNetworksJSON());
}

void handleSave(httpsserver::HTTPRequest * req, httpsserver::HTTPResponse * res) {
    res->setHeader("Content-Type", "application/json");
    
    if (req->getMethod() == "POST") {
        // Read POST body
        std::string body = "";
        uint8_t buf[128];
        size_t len;
        while ((len = req->readBytes(buf, sizeof(buf))) > 0) {
            body.append((char*)buf, len);
        }
        
        // Parse form data
        String bodyStr = String(body.c_str());
        String ssid = "", wifiPass = "", webUser = "", webPass = "";
        
        int pos = 0;
        while (pos < bodyStr.length()) {
            int ampPos = bodyStr.indexOf('&', pos);
            if (ampPos < 0) ampPos = bodyStr.length();
            String pair = bodyStr.substring(pos, ampPos);
            int eqPos = pair.indexOf('=');
            if (eqPos >= 0) {
                String key = pair.substring(0, eqPos);
                String value = pair.substring(eqPos + 1);
                value.replace("+", " ");
                value.replace("%20", " ");
                
                if (key == "ssid") ssid = value;
                else if (key == "wifiPass") wifiPass = value;
                else if (key == "webUser") webUser = value;
                else if (key == "webPass") webPass = value;
            }
            pos = ampPos + 1;
        }
        
        if (ssid.length() == 0) {
            res->setStatusCode(400);
            res->print("{\"error\":\"no ssid\"}");
        } else {
            // Save configuration — including WiFi creds so they survive a
            // reboot (arduino-esp32 3.x no longer auto-persists WiFi.begin()
            // credentials).
            const DeviceConfig& cfg = configManager.get();
            DeviceConfig newCfg = cfg;
            newCfg.configured = true;
            strncpy(newCfg.wifiSSID,     ssid.c_str(),     sizeof(newCfg.wifiSSID)     - 1);
            newCfg.wifiSSID[sizeof(newCfg.wifiSSID) - 1] = '\0';
            strncpy(newCfg.wifiPassword, wifiPass.c_str(), sizeof(newCfg.wifiPassword) - 1);
            newCfg.wifiPassword[sizeof(newCfg.wifiPassword) - 1] = '\0';
            if (webUser.length() > 0) {
                strncpy(newCfg.webUsername, webUser.c_str(), sizeof(newCfg.webUsername) - 1);
                newCfg.webUsername[sizeof(newCfg.webUsername) - 1] = '\0';
            }
            if (webPass.length() > 0) {
                strncpy(newCfg.webPassword, webPass.c_str(), sizeof(newCfg.webPassword) - 1);
                newCfg.webPassword[sizeof(newCfg.webPassword) - 1] = '\0';
            }
            configManager.update(newCfg);

            res->print("{\"status\":\"ok\"}");

            // Connect to WiFi
            delay(500);
            WiFi.mode(WIFI_AP_STA);
            WiFi.begin(ssid.c_str(), wifiPass.c_str());

            uint32_t start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
                delay(100);
            }

            if (WiFi.status() != WL_CONNECTED) {
                WiFi.mode(WIFI_AP);
            }
        }
    } else {
        res->setStatusCode(405);
        res->print("{\"error\":\"method not allowed\"}");
    }
}

void ConfigManager::startPortalServer() {
    if (!portalCert_init()) {
        Serial.println("ERROR: Failed to init portal certificate");
        return;
    }

    sslCert = portalCert_newSSLCert();
    if (!sslCert) {
        Serial.println("ERROR: Failed to build SSLCert for portal");
        return;
    }

    httpsServer = new httpsserver::HTTPSServer(sslCert, 443, 4);
    httpsServer->registerNode(new httpsserver::ResourceNode("/",     "GET",  &handleRoot));
    httpsServer->registerNode(new httpsserver::ResourceNode("/scan", "GET",  &handleScan));
    httpsServer->registerNode(new httpsserver::ResourceNode("/save", "POST", &handleSave));
    httpsServer->start();
    Serial.println("Portal HTTPS server started on :443");
}

void ConfigManager::stopPortalServer() {
    if (httpsServer) {
        httpsServer->stop();
        delete httpsServer;
        httpsServer = nullptr;
    }
    if (sslCert) {
        delete sslCert;
        sslCert = nullptr;
    }
}

ConfigManager::ConfigManager() {
    memset(&m_config, 0, sizeof(DeviceConfig));
}

bool ConfigManager::begin() {
    m_prefs.begin("ldg-config", false);

    if (!load()) {
        loadDefaults();
        save();
    }

    m_prefs.end();
    return true;
}

const DeviceConfig& ConfigManager::get() const {
    return m_config;
}

bool ConfigManager::update(const DeviceConfig& config) {
    m_config = config;
    m_config.configured = true;
    return save();
}

String ConfigManager::portalHTML() {
    return R"RAW(<!DOCTYPE html>
<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>LDG Tuner Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#0a0a1a;color:#e8e8f0;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
.card{background:#1a1d2e;border:1px solid #2a2d3e;border-radius:10px;padding:24px;width:100%;max-width:420px}
h2{color:#22c55e;font-size:18px;margin-bottom:4px}
p.sub{color:#6b7280;font-size:12px;margin-bottom:16px}
label{display:block;font-size:12px;color:#888;margin-bottom:4px;margin-top:12px}
input{width:100%;padding:10px;background:#151725;border:1px solid #2a2d3e;border-radius:6px;color:#e8e8f0;font-size:14px;outline:none}
input:focus{border-color:#22c55e}
button{width:100%;padding:12px;margin-top:20px;background:#22c55e;color:#000;border:none;border-radius:6px;font-size:14px;font-weight:600;cursor:pointer}
button:hover{background:#16a34a}
button.secondary{background:transparent;color:#22c55e;border:1px solid #22c55e;margin-top:10px}
.step{display:none}.step.active{display:block}
.net-item{padding:8px 10px;border-bottom:1px solid #2a2d3e;cursor:pointer;font-size:13px}
.net-item:hover{background:#1a1d2e}
.net-item.selected{background:#22c55e22}
.net-rssi{float:right;color:#6b7280;font-size:11px}
.lock{color:#666;font-size:11px}
#netList{max-height:200px;overflow-y:auto;border:1px solid #2a2d3e;border-radius:6px;margin-top:4px}
</style></head><body>
<div class='card'>
<h2>LDG Tuner Setup</h2>
<p class='sub'>Configure your device</p>
<div class='step active' id='step1'>
<h3 style='font-size:14px;margin-bottom:8px'>Select WiFi Network</h3>
<div id='netList'><div style='padding:12px;color:#6b7280;text-align:center'>Scanning...</div></div>
<label>WiFi Password</label>
<input type='password' id='wifiPass' placeholder='Network password'>
<button onclick='doStep1()'>Next</button>
</div>
<div class='step' id='step2'>
<h3 style='font-size:14px;margin-bottom:8px'>Web UI Password</h3>
<label>Web Username</label>
<input type='text' id='webUser' value='admin'>
<label>Web Password</label>
<input type='password' id='webPass' placeholder='Choose a strong password'>
<button onclick='doStep2()'>Save &amp; Connect</button>
<button class='secondary' onclick='goStep(1)'>Back</button>
</div>
<div class='step' id='step3'>
<h3 style='font-size:14px;margin-bottom:8px'>Connecting...</h3>
<p class='sub'>The device is connecting to your network.</p>
</div>
</div>
<script>
let selectedSSID='';
function scan(){fetch('/scan').then(r=>r.json()).then(nets=>{
var nl=document.getElementById('netList');nl.textContent='';
if(!nets.length){nl.textContent='No networks found';return;}
nets.forEach(n=>{
var item=document.createElement('div');
item.className='net-item';
item.addEventListener('click',function(){selNet(item,n.ssid);});
item.textContent=n.ssid;
var rssi=document.createElement('span');
rssi.className='net-rssi';
rssi.textContent=n.rssi+'dBm';
item.appendChild(rssi);
if(n.secure){var lk=document.createElement('span');lk.className='lock';lk.textContent='\uD83D\uDD12';item.appendChild(lk);}
nl.appendChild(item);
});
}).catch(()=>{var nl=document.getElementById('netList');nl.textContent='';var d=document.createElement('div');d.style.cssText='padding:12px;color:#ef4444;text-align:center';d.textContent='Scan failed';nl.appendChild(d);});
}
function selNet(el,ssid){document.querySelectorAll('.net-item').forEach(e=>e.classList.remove('selected'));el.classList.add('selected');selectedSSID=ssid;}
function goStep(n){document.querySelectorAll('.step').forEach(s=>s.classList.remove('active'));document.getElementById('step'+n).classList.add('active');}
function doStep1(){if(!selectedSSID){alert('Select a network');return;}goStep(2);}
function doStep2(){
let d=new FormData();
d.append('ssid',selectedSSID);d.append('wifiPass',document.getElementById('wifiPass').value);
d.append('webUser',document.getElementById('webUser').value);
d.append('webPass',document.getElementById('webPass').value);
goStep(3);
fetch('/save',{method:'POST',body:d}).then(r=>r.json()).then(d=>{
if(d.status==='ok'){setTimeout(()=>location.reload(),12000);}
else{alert('Error: '+d.error);goStep(2);}
}).catch(()=>{setTimeout(()=>location.reload(),12000);});
}
scan();
</script></body></html>)RAW";
}

String ConfigManager::scanNetworksJSON() {
    int n = WiFi.scanNetworks();
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < n && i < 20; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"] = WiFi.SSID(i);
        o["rssi"] = WiFi.RSSI(i);
        o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    String r;
    serializeJson(doc, r);
    return r;
}

bool ConfigManager::setupWiFi() {
    // Try saved credentials from NVS. WiFi.SSID() can't be used here — under
    // arduino-esp32 3.x it returns the *currently connected* AP, which is
    // always empty at boot. We own STA creds via DeviceConfig.
    if (m_config.configured && m_config.wifiSSID[0] != '\0') {
        Serial.printf("Attempting to connect to saved network: %s\n", m_config.wifiSSID);
        WiFi.mode(WIFI_STA);
        WiFi.begin(m_config.wifiSSID, m_config.wifiPassword);
        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            delay(100);
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }
        Serial.println("\nFailed to connect to saved network");
    }

    // Start configuration portal
    Serial.println("Starting configuration portal...");
    WiFi.softAP("LDGConfig", "configure");
    delay(100);
    Serial.printf("AP started: %s\n", WiFi.softAPIP().toString().c_str());

    dnsServer = new DNSServer();
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(53, "*", WiFi.softAPIP());

    // Start captive portal web server
    startPortalServer();

    // Portal runs until the user successfully joins a network. There is no
    // other configuration path on this device, so a hard timeout would brick
    // first-boot setup for any user who doesn't finish quickly enough.
    uint32_t nextHeapLog = millis();
    while (true) {
        dnsServer->processNextRequest();
        if (httpsServer) httpsServer->loop();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nConnected: %s\n", WiFi.localIP().toString().c_str());
            if (dnsServer) {
                delete dnsServer;
                dnsServer = nullptr;
            }
            // Let the /save handler's TLS connection finish so its slot is
            // free before we stop the server (the lib's stop() loops until
            // all connections close).
            delay(3000);
            stopPortalServer();
#ifndef WITH_DISPLAY
            // Drop the softAP — only display units need to keep the AP up to
            // serve the remote-unit link. Reconfiguration after this point
            // happens via a reset (POST /api/config {"reset":true} or NVS
            // wipe) which boots back into the portal flow.
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
#endif
            return true;
        }

        if ((int32_t)(millis() - nextHeapLog) >= 0) {
            nextHeapLog += PORTAL_HEAP_LOG_INTERVAL_MS;
            Serial.printf("portal: uptime=%lus heap_free=%u min_free=%u\n",
                          (unsigned long)(millis() / 1000),
                          (unsigned)ESP.getFreeHeap(),
                          (unsigned)ESP.getMinFreeHeap());
        }

        delay(10);
    }
}

void ConfigManager::reset() {
    m_prefs.begin("ldg-config", false);
    m_prefs.clear();
    m_prefs.end();

    loadDefaults();
    m_config.configured = false;
}

bool ConfigManager::isConfigured() const {
    return m_config.configured;
}

void ConfigManager::loadDefaults() {
    m_config.wifiSSID[0] = '\0';
    m_config.wifiPassword[0] = '\0';
    strncpy(m_config.mqttBroker, MQTT_BROKER, sizeof(m_config.mqttBroker) - 1);
    strncpy(m_config.mqttUsername, MQTT_USERNAME, sizeof(m_config.mqttUsername) - 1);
    strncpy(m_config.mqttPassword, MQTT_PASSWORD, sizeof(m_config.mqttPassword) - 1);
    strncpy(m_config.webUsername, WEB_AUTH_USERNAME, sizeof(m_config.webUsername) - 1);
    strncpy(m_config.webPassword, WEB_AUTH_PASSWORD, sizeof(m_config.webPassword) - 1);
    m_config.meterPsuVoltage = METER_PSU_VOLTAGE;
    m_config.mqttPort = MQTT_PORT;
    m_config.remoteUnitId = REMOTE_UNIT_ID;
    m_config.configured = false;
}

bool ConfigManager::save() {
    m_prefs.begin("ldg-config", false);

    m_prefs.putString("wifiSSID", m_config.wifiSSID);
    m_prefs.putString("wifiPassword", m_config.wifiPassword);
    m_prefs.putString("mqttBroker", m_config.mqttBroker);
    m_prefs.putString("mqttUsername", m_config.mqttUsername);
    m_prefs.putString("mqttPassword", m_config.mqttPassword);
    m_prefs.putString("webUsername", m_config.webUsername);
    m_prefs.putString("webPassword", m_config.webPassword);
    m_prefs.putFloat("meterPsuVoltage", m_config.meterPsuVoltage);
    m_prefs.putUShort("mqttPort", m_config.mqttPort);
    m_prefs.putUChar("remoteUnitId", m_config.remoteUnitId);
    m_prefs.putBool("configured", m_config.configured);

    m_prefs.end();
    return true;
}

bool ConfigManager::load() {
    m_prefs.begin("ldg-config", false);

    if (!m_prefs.isKey("configured")) {
        m_prefs.end();
        return false;
    }

    String mqttBroker = m_prefs.getString("mqttBroker", "");
    if (mqttBroker.isEmpty()) {
        m_prefs.end();
        return false;
    }

    strncpy(m_config.wifiSSID,     m_prefs.getString("wifiSSID", "").c_str(),     sizeof(m_config.wifiSSID)     - 1);
    strncpy(m_config.wifiPassword, m_prefs.getString("wifiPassword", "").c_str(), sizeof(m_config.wifiPassword) - 1);
    strncpy(m_config.mqttBroker, mqttBroker.c_str(), sizeof(m_config.mqttBroker) - 1);
    strncpy(m_config.mqttUsername, m_prefs.getString("mqttUsername", "").c_str(), sizeof(m_config.mqttUsername) - 1);
    strncpy(m_config.mqttPassword, m_prefs.getString("mqttPassword", "").c_str(), sizeof(m_config.mqttPassword) - 1);
    strncpy(m_config.webUsername, m_prefs.getString("webUsername", "").c_str(), sizeof(m_config.webUsername) - 1);
    strncpy(m_config.webPassword, m_prefs.getString("webPassword", "").c_str(), sizeof(m_config.webPassword) - 1);
    m_config.meterPsuVoltage = m_prefs.getFloat("meterPsuVoltage", METER_PSU_VOLTAGE);
    m_config.mqttPort = m_prefs.getUShort("mqttPort", MQTT_PORT);
    m_config.remoteUnitId = m_prefs.getUChar("remoteUnitId", REMOTE_UNIT_ID);
    m_config.configured = m_prefs.getBool("configured", false);

    m_prefs.end();
    return true;
}
