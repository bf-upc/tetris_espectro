// ============================================================
//  TETRIS — Consola ESPectro (ESP32-S3)
//  Basat en la template oficial. Seccions NO MODIFICAR intactes.
// ============================================================

#include <Arduino.h>
#include <SPI.h>
#include <LovyanGFX.hpp>
#include <driver/i2s.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <nvs_flash.h>
#include <nvs.h>

// ============================================================
//  PANTALLA — NO MODIFICAR
// ============================================================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Bus_SPI       _bus;
    lgfx::Panel_ILI9488 _panel;
    lgfx::Light_PWM     _light;
public:
    LGFX() {
        { auto cfg = _bus.config();
          cfg.spi_host   = SPI2_HOST;
          cfg.spi_mode   = 0;
          cfg.freq_write = 40000000;
          cfg.pin_sclk   = 47;
          cfg.pin_mosi   = 38;
          cfg.pin_miso   = 48;
          cfg.pin_dc     = 2;
          _bus.config(cfg);
          _panel.setBus(&_bus); }
        { auto cfg = _panel.config();
          cfg.pin_cs   = 1;
          cfg.pin_rst  = 0;
          cfg.pin_busy = -1;
          cfg.memory_width  = 320;
          cfg.memory_height = 480;
          cfg.panel_width   = 320;
          cfg.panel_height  = 480;
          cfg.invert    = false;
          cfg.rgb_order = false;
          _panel.config(cfg); }
        { auto cfg = _light.config();
          cfg.pin_bl      = 39;
          cfg.invert      = false;
          cfg.freq        = 44100;
          cfg.pwm_channel = 0;
          _light.config(cfg);
          _panel.setLight(&_light); }
        setPanel(&_panel);
    }
};

LGFX tft;

// ============================================================
//  PINS — NO MODIFICAR
// ============================================================
#define JOY_X_PIN  5
#define JOY_Y_PIN  4
#define JOY_SW_PIN 42
#define BTN_A_PIN  40
#define BTN_B_PIN  41

#define SCREEN_W 320
#define SCREEN_H 480

// ============================================================
//  I2S AUDIO — NO MODIFICAR
// ============================================================
#define I2S_BCLK    8
#define I2S_LRCLK  16
#define I2S_DIN    18
#define SAMPLE_RATE 44100
#define I2S_PORT    I2S_NUM_0

void audioInit() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = 512,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
    };
    i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_BCLK,
        .ws_io_num    = I2S_LRCLK,
        .data_out_num = I2S_DIN,
        .data_in_num  = I2S_PIN_NO_CHANGE,
    };
    i2s_set_pin(I2S_PORT, &pins);
    i2s_zero_dma_buffer(I2S_PORT);
}

// To sinusoïdal — freq(Hz), durada(ms), volum(0.0-0.2)
void playTone(float freq, int durationMs, float volume = 0.1f) {
    const int bufSize = 256;
    int16_t buf[bufSize * 2];
    const int samples = SAMPLE_RATE * durationMs / 1000;
    int written = 0;
    while (written < samples) {
        int chunk = min(bufSize, samples - written);
        for (int i = 0; i < chunk; i++) {
            float t   = (float)(written + i) / SAMPLE_RATE;
            int16_t v = (int16_t)(sinf(2.0f * M_PI * freq * t) * 32767.0f * volume);
            buf[i*2] = v; buf[i*2+1] = v;
        }
        size_t bw;
        i2s_write(I2S_PORT, buf, chunk * 4, &bw, portMAX_DELAY);
        written += chunk;
    }
}

void playSilence(int durationMs) {
    int16_t buf[512] = {0};
    const int samples = SAMPLE_RATE * durationMs / 1000;
    int written = 0;
    while (written < samples) {
        int chunk = min(256, samples - written);
        size_t bw;
        i2s_write(I2S_PORT, buf, chunk * 4, &bw, portMAX_DELAY);
        written += chunk;
    }
}

// ============================================================
//  SPLASH SCREEN — NO MODIFICAR
// ============================================================
void showSplash() {
    tft.fillScreen(TFT_BLACK);
    const uint16_t TARONJA = tft.color565(255, 80, 0);
    tft.fillRect(0, 0, SCREEN_W, 6, TARONJA);
    tft.setTextSize(5);
    int y_titol = 140;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(SCREEN_W/2 - tft.textWidth("ESPectro")/2, y_titol);
    tft.print("ESP");
    tft.setTextColor(TARONJA, TFT_BLACK);
    tft.print("ectro");
    tft.drawFastHLine(40, y_titol+55, SCREEN_W-80, TARONJA);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    const char* slogan = "Consola portatil ESP32-S3";
    tft.setCursor(SCREEN_W/2 - tft.textWidth(slogan)/2, y_titol+70);
    tft.print(slogan);
    const char* autors = "Noel Medina & Bernat Figuerola - UPC 2026";
    tft.setCursor(SCREEN_W/2 - tft.textWidth(autors)/2, y_titol+86);
    tft.print(autors);
    tft.fillRect(0, SCREEN_H-6, SCREEN_W, 6, TARONJA);
    playTone(220.0f, 120, 0.15f); playSilence(30);
    playTone(277.2f, 120, 0.15f); playSilence(30);
    playTone(329.6f, 120, 0.15f);
    delay(500);
}

// ============================================================
//  FREERTOS — NO MODIFICAR
// ============================================================
SemaphoreHandle_t recordMutex;
volatile bool     wifiActiu = false;

// ── WiFi / Game Loader ────────────────────────────────────────
#define AP_SSID "ESPectro"
#define AP_PASS "gameloader"
#define AP_IP   "192.168.4.1"

#define MAX_HISTORY 20

// ── Clau del rècord ──────────────────────────────────────────
#define RECORD_KEY "tetris"

// ============================================================
//  RECORDS NVS — NO MODIFICAR
// ============================================================
int loadRecord() {
    nvs_handle_t h;
    nvs_flash_init();
    int32_t r = 0;
    if (nvs_open("records", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i32(h, RECORD_KEY, &r);
        nvs_close(h);
    }
    return (int)r;
}

String loadHistory(const char* key) {
    nvs_handle_t h;
    nvs_flash_init();
    String hist = "[]";
    String hkey = String(key) + "_h";
    if (nvs_open("records", NVS_READONLY, &h) == ESP_OK) {
        size_t len = 0;
        if (nvs_get_str(h, hkey.c_str(), nullptr, &len) == ESP_OK && len > 0) {
            char* buf = new char[len];
            nvs_get_str(h, hkey.c_str(), buf, &len);
            hist = String(buf);
            delete[] buf;
        }
        nvs_close(h);
    }
    return hist;
}

void registerGame(const char* key) {
    nvs_handle_t h;
    nvs_flash_init();
    if (nvs_open("records", NVS_READWRITE, &h) == ESP_OK) {
        // Llegir llista actual "game_list" -> "road_rush,snake,pong"
        char buf[256] = "";
        size_t len = sizeof(buf);
        nvs_get_str(h, "game_list", buf, &len);
        String list = String(buf);
        // Afegir si no hi és
        if (list.indexOf(key) < 0) {
            if (list.length() > 0) list += ",";
            list += key;
            nvs_set_str(h, "game_list", list.c_str());
            nvs_commit(h);
        }
        nvs_close(h);
    }
}
void saveRecord(int score) {
    registerGame(RECORD_KEY);  //
    nvs_handle_t h;
    nvs_flash_init();
    if (nvs_open("records", NVS_READWRITE, &h) == ESP_OK) {
        int32_t current = 0;
        nvs_get_i32(h, RECORD_KEY, &current);
        if (score > current)
            nvs_set_i32(h, RECORD_KEY, (int32_t)score);

        String hkey = String(RECORD_KEY) + "_h";
        String hist = "[]";
        size_t len = 0;
        if (nvs_get_str(h, hkey.c_str(), nullptr, &len) == ESP_OK && len > 0) {
            char* buf = new char[len];
            nvs_get_str(h, hkey.c_str(), buf, &len);
            hist = String(buf);
            delete[] buf;
        }
        String inner = hist.substring(1, hist.length()-1);
        String nova;
        if (inner.length() == 0) {
            nova = "[" + String(score) + "]";
        } else {
            int count = 1;
            for (int i = 0; i < (int)inner.length(); i++)
                if (inner[i] == ',') count++;
            if (count >= MAX_HISTORY) {
                int lc = inner.lastIndexOf(',');
                inner = (lc >= 0) ? inner.substring(0, lc) : "";
            }
            nova = (inner.length() > 0)
                ? "[" + String(score) + "," + inner + "]"
                : "[" + String(score) + "]";
        }
        nvs_set_str(h, hkey.c_str(), nova.c_str());
        nvs_commit(h);
        nvs_close(h);
    }
}

String getAllRecords() {
    if (xSemaphoreTake(recordMutex, pdMS_TO_TICKS(100)) != pdTRUE)
        return "{}";

    nvs_handle_t h;
    nvs_flash_init();
    String json = "{";
    if (nvs_open("records", NVS_READONLY, &h) == ESP_OK) {
        char buf[256] = "";
        size_t len = sizeof(buf);
        nvs_get_str(h, "game_list", buf, &len);
        String list = String(buf);
        
        bool first = true;
        int start = 0;
        while (start < (int)list.length()) {
            int comma = list.indexOf(',', start);
            String key = (comma < 0) 
                ? list.substring(start) 
                : list.substring(start, comma);
            
            if (key.length() > 0) {
                int32_t best = 0;
                nvs_get_i32(h, key.c_str(), &best);
                String hist = loadHistory(key.c_str());
                if (!first) json += ",";
                json += "\"" + key + "\":{\"best\":" + String(best) + 
                        ",\"history\":" + hist + "}";
                first = false;
            }
            if (comma < 0) break;
            start = comma + 1;
        }
        nvs_close(h);
    }
    json += "}";
    xSemaphoreGive(recordMutex);
    return json;
}

// ============================================================
//  DASHBOARD WEB — NO MODIFICAR
// ============================================================
WebServer server(80);

const char PAGE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="ca">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESPectro — Dashboard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;}
body{background:#0d0d0d;color:#eee;font-family:'Courier New',monospace;
     min-height:100vh;padding:1em;}
h1{color:#ff5000;text-align:center;font-size:1.8em;letter-spacing:4px;
   padding:0.5em 0;border-bottom:2px solid #ff5000;margin-bottom:1em;}
h1 span{color:#fff;}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:1em;max-width:900px;margin:0 auto;}
@media(max-width:600px){.grid{grid-template-columns:1fr;}}
.card{background:#1a1a1a;border:1px solid #333;border-radius:10px;padding:1.2em;}
.card h2{color:#ff5000;font-size:0.9em;letter-spacing:2px;margin-bottom:0.8em;}
.game-card{background:#1a1a1a;border:1px solid #333;border-radius:10px;
           padding:1.2em;margin-bottom:1em;}
.game-card h3{color:#ff5000;font-size:0.9em;letter-spacing:2px;margin-bottom:0.8em;}
.stat{display:flex;justify-content:space-between;align-items:center;
      padding:0.4em 0;border-bottom:1px solid #222;}
.stat:last-child{border:none;}
.stat-label{color:#888;font-size:0.85em;}
.stat-val{color:#0f0;font-weight:bold;font-size:1.1em;}
.stat-val.gold{color:#ffd700;}
.chart{margin-top:0.8em;}
.chart-title{color:#888;font-size:0.75em;margin-bottom:0.5em;}
.bars{display:flex;align-items:flex-end;gap:3px;height:80px;}
.bar-wrap{display:flex;flex-direction:column;align-items:center;flex:1;}
.bar{width:100%;background:#ff5000;border-radius:2px 2px 0 0;min-height:2px;}
.bar-val{color:#888;font-size:0.55em;margin-top:2px;}
input[type=file]{display:none;}
label.btn,button.btn{display:inline-block;padding:0.6em 1.2em;margin:0.3em 0;
  background:#ff5000;color:#fff;border:none;border-radius:6px;
  font-size:0.9em;font-family:monospace;cursor:pointer;width:100%;}
label.btn:hover,button.btn:hover{background:#cc3e00;}
#filename{color:#ff5000;margin:0.4em 0;font-size:0.85em;min-height:1.2em;}
#progress{width:100%;background:#222;border-radius:4px;height:12px;
          margin:0.5em 0;display:none;}
#bar{height:100%;width:0;background:#ff5000;border-radius:4px;transition:width 0.3s;}
#status{min-height:1.5em;font-size:0.85em;color:#ff0;}
.ok{color:#0f0!important;} .err{color:#f44!important;}
</style>
</head>
<body>
<h1><span>ESP</span>ectro — Dashboard</h1>
<div class="grid">
  <div id="games-col">
    <div id="games-container">
      <div class="game-card"><span style="color:#555">Carregant...</span></div>
    </div>
  </div>
  <div class="card">
    <h2>Carregar joc</h2>
    <label class="btn" for="file">Triar .bin</label>
    <input type="file" id="file" accept=".bin">
    <div id="filename">Cap arxiu seleccionat</div>
    <button class="btn" onclick="upload()">Pujar joc</button>
    <div id="progress"><div id="bar"></div></div>
    <div id="status"></div>
  </div>
</div>
<script>
function avg(arr){return arr.length?Math.round(arr.reduce((a,b)=>a+b,0)/arr.length):0;}
function renderAllGames(data){
  const entries=Object.entries(data);
  const container=document.getElementById('games-container');
  if(!entries.length){
    container.innerHTML='<div class="game-card"><span style="color:#555">Cap joc registrat</span></div>';
    return;
  }
  container.innerHTML=entries.map(([key,gd])=>{
    const hist=gd.history||[];
    const best=gd.best||0;
    const mitjana=avg(hist);
    const darrera=hist[0]||0;
    const last10=hist.slice(0,10).reverse();
    const maxVal=Math.max(...last10,1);
    const bars=last10.length?last10.map(v=>{
      const h=Math.round((v/maxVal)*70);
      return`<div class="bar-wrap"><div class="bar" style="height:${h}px;background:${v===best&&best>0?'#ffd700':'#ff5000'}"></div><div class="bar-val">${v}</div></div>`;
    }).join(''):'<span style="color:#555;font-size:0.8em">Sense dades</span>';
    return`<div class="game-card">
      <h3>${key.replace(/_/g,' ').toUpperCase()}</h3>
      <div class="stat"><span class="stat-label">Record</span><span class="stat-val gold">${best} pts</span></div>
      <div class="stat"><span class="stat-label">Partides</span><span class="stat-val">${hist.length}</span></div>
      <div class="stat"><span class="stat-label">Mitjana</span><span class="stat-val">${mitjana} pts</span></div>
      <div class="stat"><span class="stat-label">Darrera</span><span class="stat-val ${darrera===best&&best>0?'gold':''}">${darrera} pts</span></div>
      <div class="chart"><div class="chart-title">Ultimes partides</div>
      <div class="bars">${bars}</div></div>
    </div>`;
  }).join('');
}
function load(){
  fetch('/records').then(r=>r.json()).then(renderAllGames).catch(()=>{});
}
load();
setInterval(load,10000);
const fi=document.getElementById('file');
fi.addEventListener('change',()=>{document.getElementById('filename').textContent=fi.files[0]?.name||'Cap arxiu';});
function upload(){
  const file=fi.files[0];
  const status=document.getElementById('status');
  const bar=document.getElementById('bar');
  const prog=document.getElementById('progress');
  if(!file){status.textContent='Selecciona un arxiu';return;}
  if(!file.name.endsWith('.bin')){status.textContent='Ha de ser .bin';return;}
  const xhr=new XMLHttpRequest();
  xhr.open('POST','/update',true);
  xhr.upload.onprogress=e=>{
    if(e.lengthComputable){const pct=Math.round(e.loaded/e.total*100);prog.style.display='block';bar.style.width=pct+'%';status.textContent='Pujant... '+pct+'%';}
  };
  xhr.onload=()=>{
    if(xhr.status===200){status.textContent='Instal.lat. Reiniciant...';status.className='ok';}
    else{status.textContent='Error: '+xhr.responseText;status.className='err';}
  };
  xhr.onerror=()=>{status.textContent='Error connexio';status.className='err';};
  const fd=new FormData();fd.append('firmware',file,file.name);xhr.send(fd);
}
</script>
</body>
</html>
)rawhtml";

void handleRoot()    { server.send_P(200, "text/html", PAGE_HTML); }
void handleRecords() { server.send(200, "application/json", getAllRecords()); }
void handleUpdate()  {
    server.send(200, "text/plain", Update.hasError() ? "FALLO" : "OK");
    delay(500); ESP.restart();
}
void handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        tft.fillRect(0, 260, 320, 60, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 270);
        tft.print("Rebent firmware...");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
            Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
        static size_t total = 0;
        total += upload.currentSize;
        tft.fillRect(10, 300, constrain((int)(total/10000),0,100)*3, 10, TFT_GREEN);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            tft.fillRect(0, 260, 320, 60, TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(10, 270); tft.print("Joc instal·lat!");
            tft.setCursor(10, 295); tft.print("Reiniciant...");
        } else { Update.printError(Serial); }
    }
}
// ============================================================
//  HANDLERS MCP — NO MODIFICAR
// ============================================================
void handleMcpTools() {
    String json = "{\"tools\":["
        "{\"name\":\"get_records\","
        "\"description\":\"Records i historial de puntuacions\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
        "{\"name\":\"get_status\","
        "\"description\":\"Estat de la consola: uptime i memoria\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
        "{\"name\":\"get_system_info\","
        "\"description\":\"Info hardware: CPU, flash, PSRAM\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
        "]}";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

void handleMcpGetRecords() {
    String records = getAllRecords();
    String resp = "{\"content\":[{\"type\":\"text\",\"text\":" + records + "}]}";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", resp);
}

void handleMcpGetStatus() {
    unsigned long uptime = millis() / 1000;
    String resp = "{\"content\":[{\"type\":\"text\",\"text\":{"
                  "\"uptime_s\":" + String(uptime) + ","
                  "\"free_heap_bytes\":" + String(ESP.getFreeHeap()) + ","
                  "\"wifi_ssid\":\"ESPectro\","
                  "\"ip\":\"192.168.4.1\","
                  "\"version\":\"1.0.0\"}}]}";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", resp);
}

void handleMcpGetSystemInfo() {
    String resp = "{\"content\":[{\"type\":\"text\",\"text\":{"
                  "\"chip\":\"ESP32-S3\","
                  "\"cpu_freq_mhz\":" + String(ESP.getCpuFreqMHz()) + ","
                  "\"flash_size_mb\":" + String(ESP.getFlashChipSize()/1024/1024) + ","
                  "\"free_heap_bytes\":" + String(ESP.getFreeHeap()) + ","
                  "\"free_psram_bytes\":" + String(ESP.getFreePsram()) + ","
                  "\"sdk_version\":\"" + String(ESP.getSdkVersion()) + "\"}}]}";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", resp);
}

void handleMcpCall() {
    String tool = server.arg("tool");
    if      (tool == "get_records")     handleMcpGetRecords();
    else if (tool == "get_status")      handleMcpGetStatus();
    else if (tool == "get_system_info") handleMcpGetSystemInfo();
    else {
        String err = "{\"error\":\"Tool no trobada: " + tool + "\"}";
        server.send(404, "application/json", err);
    }
}

// ── Tasca WiFi (core 0, prioritat 1) ─────────────────────────
void wifiTask(void* param) {
    while (true) {
        if (wifiActiu) server.handleClient();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// ============================================================
//  GAME LOADER — NO MODIFICAR
// ============================================================
void runGameLoader() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(tft.color565(255,80,0), TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(30, 40); tft.print("GAME LOADER");
    tft.drawFastHLine(10, 88, 300, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 108); tft.print("Xarxa WiFi:");
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 130); tft.print(AP_SSID);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 158); tft.print("Contrasenya:");
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(10, 180); tft.print(AP_PASS);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 210); tft.print("Obre al navegador:");
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(10, 232); tft.printf("http://%s", AP_IP);
    tft.drawFastHLine(10, 256, 300, TFT_DARKGREY);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 268); tft.print("RECORD:");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(10, 282);
    tft.printf("%s: %d pts", RECORD_KEY, loadRecord());
    tft.setTextColor(tft.color565(150,150,150), TFT_BLACK);
    tft.setCursor(10, 440); tft.print("Prem A per tornar al menu");

    while (true) {
        if (digitalRead(BTN_A_PIN) == LOW) {
            delay(300);
            return;
        }
        delay(20);
    }
}

// ============================================================
//  MENÚ PRINCIPAL — NO MODIFICAR (pots canviar el títol)
// ============================================================
void drawMenu(int bestScore) {
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(tft.color565(255,60,0), TFT_BLACK);
    tft.setTextSize(4);
    const char* linia1 = "SUPER";
    const char* linia2 = "TETRIS";
    tft.setCursor(SCREEN_W/2 - tft.textWidth(linia1)/2, 50);  tft.print(linia1);
    tft.setCursor(SCREEN_W/2 - tft.textWidth(linia2)/2, 100); tft.print(linia2);

    tft.drawFastHLine(40, 158, 240, tft.color565(255,60,0));

    tft.setTextColor(tft.color565(255,215,0), TFT_BLACK);
    tft.setTextSize(2);
    String best = "Record: " + String(bestScore) + " pts";
    tft.setCursor(SCREEN_W/2 - tft.textWidth(best)/2, 175);
    tft.print(best);

    tft.drawFastHLine(40, 210, 240, tft.color565(255,60,0));

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(SCREEN_W/2 - tft.textWidth("Prem A per jugar")/2, 250);
    tft.print("Prem A per jugar");
    tft.setCursor(SCREEN_W/2 - tft.textWidth("Prem B per carregar")/2, 290);
    tft.print("Prem B per carregar");
    tft.setCursor(SCREEN_W/2 - tft.textWidth("un nou joc")/2, 312);
    tft.print("un nou joc");
}

// ============================================================
//  VARIABLES GLOBALS DEL JOC (TETRIS)
// ============================================================
#define COLS    10
#define ROWS    20
#define CELL    22
#define BOARD_X  8
#define BOARD_Y 36
#define SX      236      // origen X de la barra lateral

// Tauler de joc (0 = buit, 1..7 = color de la peça fixada)
uint8_t board[ROWS][COLS];
// Còpia del que ja hi ha dibuixat a pantalla (-1 força redibuix)
int8_t  displayed[ROWS][COLS];

// 7 peces × 4 rotacions × 4 blocs × (x,y) dins d'una caixa 4×4
const int8_t PIECES[7][4][4][2] = {
    { // I
        {{0,1},{1,1},{2,1},{3,1}},
        {{2,0},{2,1},{2,2},{2,3}},
        {{0,2},{1,2},{2,2},{3,2}},
        {{1,0},{1,1},{1,2},{1,3}}
    },
    { // O
        {{1,0},{2,0},{1,1},{2,1}},
        {{1,0},{2,0},{1,1},{2,1}},
        {{1,0},{2,0},{1,1},{2,1}},
        {{1,0},{2,0},{1,1},{2,1}}
    },
    { // T
        {{1,0},{0,1},{1,1},{2,1}},
        {{1,0},{1,1},{2,1},{1,2}},
        {{0,1},{1,1},{2,1},{1,2}},
        {{1,0},{0,1},{1,1},{1,2}}
    },
    { // S
        {{1,0},{2,0},{0,1},{1,1}},
        {{1,0},{1,1},{2,1},{2,2}},
        {{1,1},{2,1},{0,2},{1,2}},
        {{0,0},{0,1},{1,1},{1,2}}
    },
    { // Z
        {{0,0},{1,0},{1,1},{2,1}},
        {{2,0},{1,1},{2,1},{1,2}},
        {{0,1},{1,1},{1,2},{2,2}},
        {{1,0},{0,1},{1,1},{0,2}}
    },
    { // J
        {{0,0},{0,1},{1,1},{2,1}},
        {{1,0},{2,0},{1,1},{1,2}},
        {{0,1},{1,1},{2,1},{2,2}},
        {{1,0},{1,1},{0,2},{1,2}}
    },
    { // L
        {{2,0},{0,1},{1,1},{2,1}},
        {{1,0},{1,1},{1,2},{2,2}},
        {{0,1},{1,1},{2,1},{0,2}},
        {{0,0},{1,0},{1,1},{1,2}}
    }
};

uint16_t pieceColor[8];   // índex 0 sense usar; 1..7 = colors

int  curType, curRot, curX, curY, nextType;
long tScore;
int  tLines, tLevel, tBest, dropInterval;
bool gameOver;

// Valors anteriors de la barra lateral (per redibuixar només els canvis)
long pvScore; int pvLevel, pvLines, pvBest, pvNext;

// ── Comprovació de col·lisió d'una peça en una posició donada ──
bool collides(int type, int rot, int ox, int oy) {
    for (int i = 0; i < 4; i++) {
        int cx = ox + PIECES[type][rot][i][0];
        int cy = oy + PIECES[type][rot][i][1];
        if (cx < 0 || cx >= COLS || cy >= ROWS) return true;
        if (cy < 0) continue;               // encara per sobre del tauler
        if (board[cy][cx]) return true;
    }
    return false;
}

// ── Fixa la peça actual al tauler ─────────────────────────────
void lockPiece() {
    for (int i = 0; i < 4; i++) {
        int cx = curX + PIECES[curType][curRot][i][0];
        int cy = curY + PIECES[curType][curRot][i][1];
        if (cy >= 0 && cy < ROWS && cx >= 0 && cx < COLS)
            board[cy][cx] = curType + 1;
    }
}

// ── Dibuixa una cel·la del tauler ─────────────────────────────
void drawCell(int r, int c, int v) {
    int px = BOARD_X + c * CELL;
    int py = BOARD_Y + r * CELL;
    if (v == 0) {
        tft.fillRect(px, py, CELL, CELL, TFT_BLACK);
        tft.drawRect(px, py, CELL, CELL, tft.color565(22,22,22));
    } else {
        uint16_t col = pieceColor[v];
        tft.fillRect(px, py, CELL, CELL, col);
        tft.drawRect(px, py, CELL, CELL, TFT_BLACK);
        // petit bisell de llum a dalt/esquerra
        tft.drawFastHLine(px+2, py+2, CELL-4, tft.color565(255,255,255));
        tft.drawFastVLine(px+2, py+2, CELL-4, tft.color565(255,255,255));
    }
}

// ── Redibuixa només les cel·les que han canviat ───────────────
void renderBoard() {
    int pc[4][2];
    for (int i = 0; i < 4; i++) {
        pc[i][0] = curX + PIECES[curType][curRot][i][0];
        pc[i][1] = curY + PIECES[curType][curRot][i][1];
    }
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int v = board[r][c];
            for (int i = 0; i < 4; i++)
                if (pc[i][0] == c && pc[i][1] == r) { v = curType + 1; break; }
            if (displayed[r][c] != v) {
                drawCell(r, c, v);
                displayed[r][c] = v;
            }
        }
    }
}

// ── Elimina línies completes (amb flash) i retorna quantes ────
int clearLinesAnimated() {
    bool full[ROWS];
    int cnt = 0;
    for (int r = 0; r < ROWS; r++) {
        full[r] = true;
        for (int c = 0; c < COLS; c++)
            if (!board[r][c]) { full[r] = false; break; }
        if (full[r]) cnt++;
    }
    if (cnt == 0) return 0;

    // Parpelleig de les línies completes
    for (int b = 0; b < 3; b++) {
        uint16_t col = (b % 2 == 0) ? TFT_WHITE : TFT_BLACK;
        for (int r = 0; r < ROWS; r++)
            if (full[r])
                for (int c = 0; c < COLS; c++)
                    tft.fillRect(BOARD_X + c*CELL, BOARD_Y + r*CELL, CELL, CELL, col);
        delay(60);
    }

    // Reconstrueix el tauler sense les files plenes
    uint8_t nb[ROWS][COLS];
    memset(nb, 0, sizeof(nb));
    int dst = ROWS - 1;
    for (int r = ROWS - 1; r >= 0; r--) {
        if (!full[r]) {
            for (int c = 0; c < COLS; c++) nb[dst][c] = board[r][c];
            dst--;
        }
    }
    memcpy(board, nb, sizeof(board));
    memset(displayed, -1, sizeof(displayed));   // força redibuix complet
    return cnt;
}

// ── Genera una peça nova ──────────────────────────────────────
void spawnPiece() {
    curType = nextType;
    nextType = random(7);
    curRot = 0;
    curX = 3;
    curY = 0;
}

// ── Intenta girar amb "wall kicks" senzills ───────────────────
void tryRotate(int dir) {
    int nr = (curRot + dir + 4) % 4;
    const int kicks[5] = {0, -1, 1, -2, 2};
    for (int k = 0; k < 5; k++) {
        if (!collides(curType, nr, curX + kicks[k], curY)) {
            curX += kicks[k];
            curRot = nr;
            playTone(440.0f, 12, 0.08f);
            return;
        }
    }
}

void moveH(int dx) {
    if (!collides(curType, curRot, curX + dx, curY)) curX += dx;
}

// ── Fixa la peça, neteja línies, puntua i genera la següent ───
void lockAndAdvance() {
    playTone(196.0f, 16, 0.10f);             // cop sord
    lockPiece();
    int n = clearLinesAnimated();
    if (n > 0) {
        static const int pts[5] = {0, 100, 300, 500, 800};
        tScore += (long)pts[n] * tLevel;
        tLines += n;
        tLevel = tLines / 10 + 1;
        dropInterval = max(70, 600 - (tLevel - 1) * 45);
        for (int i = 0; i < n; i++) playTone(523.3f + i*60, 50, 0.12f);
    }
    spawnPiece();
    if (collides(curType, curRot, curX, curY)) gameOver = true;
}

// ── Vista prèvia de la peça següent ───────────────────────────
void drawNextPreview() {
    tft.fillRect(SX, 322, 80, 56, TFT_BLACK);
    for (int i = 0; i < 4; i++) {
        int cx = PIECES[nextType][0][i][0];
        int cy = PIECES[nextType][0][i][1];
        int px = SX + 8 + cx * 13;
        int py = 326 + cy * 13;
        tft.fillRect(px, py, 12, 12, pieceColor[nextType + 1]);
        tft.drawRect(px, py, 12, 12, TFT_BLACK);
    }
}

// ── Etiquetes fixes de la barra lateral i marc del tauler ─────
void drawSidebarStatic() {
    uint16_t O = tft.color565(255, 80, 0);
    tft.drawRect(BOARD_X-2, BOARD_Y-2, COLS*CELL+4, ROWS*CELL+4, O);
    tft.setTextSize(1);
    tft.setTextColor(O, TFT_BLACK);
    tft.setCursor(SX, 40);  tft.print("PUNTS");
    tft.setCursor(SX, 104); tft.print("NIVELL");
    tft.setCursor(SX, 168); tft.print("LINIES");
    tft.setCursor(SX, 232); tft.print("RECORD");
    tft.setCursor(SX, 300); tft.print("SEGUENT");
    tft.setTextColor(tft.color565(120,120,120), TFT_BLACK);
    tft.setCursor(SX, 400); tft.print("Joy: moure");
    tft.setCursor(SX, 414); tft.print("Joy v: baixar");
    tft.setCursor(SX, 428); tft.print("A: girar");
    tft.setCursor(SX, 442); tft.print("Joy btn: cau");
    tft.setCursor(SX, 456); tft.print("B: sortir");
}

// ── Refresca només els valors que canvien ─────────────────────
void updateSidebar() {
    tft.setTextSize(2);
    if (pvScore != tScore) {
        tft.fillRect(SX, 60, 80, 18, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(SX, 60); tft.printf("%ld", tScore);
        pvScore = tScore;
    }
    if (pvLevel != tLevel) {
        tft.fillRect(SX, 124, 80, 18, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(SX, 124); tft.printf("%d", tLevel);
        pvLevel = tLevel;
    }
    if (pvLines != tLines) {
        tft.fillRect(SX, 188, 80, 18, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(SX, 188); tft.printf("%d", tLines);
        pvLines = tLines;
    }
    if (pvBest != tBest) {
        tft.fillRect(SX, 252, 80, 18, TFT_BLACK);
        tft.setTextColor(tft.color565(255,215,0), TFT_BLACK);
        tft.setCursor(SX, 252); tft.printf("%d", tBest);
        pvBest = tBest;
    }
    if (pvNext != nextType) {
        drawNextPreview();
        pvNext = nextType;
    }
}

// ── Pantalla de Game Over (guarda el rècord) ──────────────────
void showGameOver() {
    if (xSemaphoreTake(recordMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        saveRecord((int)tScore);
        xSemaphoreGive(recordMutex);
    }
    playTone(329.6f, 150, 0.12f);
    playTone(261.6f, 150, 0.12f);
    playTone(196.0f, 300, 0.13f);

    int bw = 240, bh = 160;
    int bx = (SCREEN_W - bw) / 2, by = (SCREEN_H - bh) / 2;
    tft.fillRect(bx, by, bw, bh, TFT_BLACK);
    tft.drawRect(bx, by, bw, bh, tft.color565(255,80,0));
    tft.drawRect(bx+2, by+2, bw-4, bh-4, tft.color565(255,80,0));

    tft.setTextColor(tft.color565(255,80,0), TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(bx + (bw - tft.textWidth("GAME"))/2, by + 18); tft.print("GAME");
    tft.setCursor(bx + (bw - tft.textWidth("OVER"))/2, by + 50); tft.print("OVER");

    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    String s = "Punts: " + String(tScore);
    tft.setCursor(bx + (bw - tft.textWidth(s.c_str()))/2, by + 92); tft.print(s);
    String b = "Record: " + String(loadRecord());
    tft.setCursor(bx + (bw - tft.textWidth(b.c_str()))/2, by + 114); tft.print(b);

    tft.setTextSize(1);
    tft.setTextColor(tft.color565(180,180,180), TFT_BLACK);
    const char* msg = "Prem A o B per continuar";
    tft.setCursor(bx + (bw - tft.textWidth(msg))/2, by + 140); tft.print(msg);

    delay(300);
    while (true) {
        if (digitalRead(BTN_A_PIN) == LOW || digitalRead(BTN_B_PIN) == LOW) {
            delay(300);
            return;
        }
        delay(20);
    }
}

// ============================================================
//  LÒGICA DEL JOC
// ============================================================
void runGame() {
    tBest = loadRecord();

    // Colors de les peces
    pieceColor[1] = tft.color565(0,   240, 240);  // I  cian
    pieceColor[2] = tft.color565(245, 220, 0);    // O  groc
    pieceColor[3] = tft.color565(170, 0,   235);  // T  lila
    pieceColor[4] = tft.color565(0,   220, 40);   // S  verd
    pieceColor[5] = tft.color565(235, 40,  40);   // Z  vermell
    pieceColor[6] = tft.color565(40,  90,  255);  // J  blau
    pieceColor[7] = tft.color565(255, 140, 0);    // L  taronja

    randomSeed(micros() ^ (analogRead(JOY_X_PIN) << 6) ^ analogRead(JOY_Y_PIN));

    memset(board, 0, sizeof(board));
    memset(displayed, -1, sizeof(displayed));
    tScore = 0; tLines = 0; tLevel = 1; dropInterval = 600; gameOver = false;
    pvScore = -1; pvLevel = -1; pvLines = -1; pvBest = -1; pvNext = -1;

    nextType = random(7);
    spawnPiece();

    tft.fillScreen(TFT_BLACK);
    drawSidebarStatic();

    unsigned long lastFrame = millis();
    unsigned long lastDrop  = millis();
    int  prevDirX = 0;
    bool prevA = false, prevUp = false, prevJoyBtn = false;
    unsigned long hRepeatAt = 0;

    while (true) {
        unsigned long now = millis();
        if (now - lastFrame < 16) { delay(1); continue; }
        lastFrame = now;

        // --- Llegir controls ---
        int rawX = analogRead(JOY_X_PIN);
        int rawY = analogRead(JOY_Y_PIN);
        int dirX = (rawX < 1748) ? -1 : (rawX > 2348) ? 1 : 0;
        bool down = (rawY > 2348);
        bool up   = (rawY < 1748);
        bool btnA = (digitalRead(BTN_A_PIN)  == LOW);
        bool btnB = (digitalRead(BTN_B_PIN)  == LOW);
        bool joyB = (digitalRead(JOY_SW_PIN) == LOW);

        // --- Sortir al menú (botó B) ---
        if (btnB) {
            if (xSemaphoreTake(recordMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                saveRecord((int)tScore);
                xSemaphoreGive(recordMutex);
            }
            delay(200);
            return;
        }

        // --- Girar (botó A o joystick amunt, per flanc) ---
        if (btnA && !prevA) tryRotate(1);
        // --- Moviment horitzontal amb auto-repetició ---
        if (dirX != 0) {
            if (dirX != prevDirX)      { moveH(dirX); hRepeatAt = now + 170; }
            else if (now >= hRepeatAt) { moveH(dirX); hRepeatAt = now + 85;  }
        }

        // --- Caiguda instantània (botó del joystick, per flanc) ---
        bool dropped = false;
        if (joyB && !prevJoyBtn) {
            int d = 0;
            while (!collides(curType, curRot, curX, curY + 1)) { curY++; d++; }
            tScore += (long)d * 2;
            lockAndAdvance();
            dropped = true;
        }

        // --- Gravetat ---
        if (!dropped && !gameOver) {
            int interval = down ? 45 : dropInterval;
            if (now - lastDrop >= interval) {
                lastDrop = now;
                if (!collides(curType, curRot, curX, curY + 1)) {
                    curY++;
                    if (down) tScore += 1;
                } else {
                    lockAndAdvance();
                }
            }
        }

        // --- Dibuixar ---
        if (!gameOver) renderBoard();
        if (tBest < tScore) tBest = tScore;   // rècord en viu
        updateSidebar();

        prevDirX = dirX; prevA = btnA; prevUp = up; prevJoyBtn = joyB;

        // --- Fi de partida ---
        if (gameOver) {
            renderBoard();
            updateSidebar();
            showGameOver();
            return;
        }
    }
}

// ============================================================
//  SETUP — NO MODIFICAR
// ============================================================
void setup() {
    Serial.begin(115200);
    pinMode(JOY_X_PIN,  INPUT);
    pinMode(JOY_Y_PIN,  INPUT);
    pinMode(JOY_SW_PIN, INPUT_PULLUP);
    pinMode(BTN_A_PIN,  INPUT_PULLUP);
    pinMode(BTN_B_PIN,  INPUT_PULLUP);

    tft.init();
    tft.setRotation(2);
    tft.setBrightness(255);

    recordMutex = xSemaphoreCreateMutex();
    audioInit();

    // Tasca WiFi — core 0, prioritat 1
    xTaskCreatePinnedToCore(wifiTask, "wifi", 4096, NULL, 1, NULL, 0);

    // Iniciar WiFi en segon pla
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.softAPConfig(
        IPAddress(192,168,4,1),
        IPAddress(192,168,4,1),
        IPAddress(255,255,255,0)
    );
    server.on("/",        HTTP_GET,  handleRoot);
    server.on("/records", HTTP_GET,  handleRecords);
    server.on("/update",  HTTP_POST, handleUpdate, handleUpdateUpload);
    server.on("/mcp/tools",      HTTP_GET, handleMcpTools);
    server.on("/mcp/tools/call", HTTP_GET, handleMcpCall);
    server.begin();
    wifiActiu = true;

    showSplash();
}

// ============================================================
//  LOOP — NO MODIFICAR
// ============================================================
void loop() {
    tft.fillScreen(TFT_BLACK);
    int best = loadRecord();
    drawMenu(best);

    while (true) {
        if (digitalRead(BTN_A_PIN) == LOW) {
            delay(50);
            runGame();
            break;
        }
        if (digitalRead(BTN_B_PIN) == LOW) {
            delay(50);
            runGameLoader();
            break;
        }
        delay(20);
    }
}