/*
 * Monitor remoto de glicemia (CGM) com diario de insulina - firmware v2
 * Disciplina: Objetos Inteligentes Conectados - Universidade Presbiteriana Mackenzie
 * Autor: Luiz Rodrigo Alves Vergino
 *
 * v2: acrescenta o controle de atuadores via internet (assinatura MQTT do
 * topico de comando + publicacao de confirmacao), usado tambem para medir
 * o tempo de resposta comando -> atuador.
 *
 * Sensor virtual: reproduz dados reais de CGM (paciente 1006, dataset
 * ShanghaiT1DM - ZHAO et al., 2023). Alertas locais (LED/buzzer/LCD) e
 * remotos (MQTT), setas de tendencia e alerta preditivo de hipoglicemia.
 *
 * Hardware (simulado no Wokwi): ESP32 DevKitC + LCD 16x2 I2C + 2 botoes +
 * potenciometro (dose) + buzzer + LEDs verde/amarelo/vermelho.
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include "cgm_data.h"

// ---------- Configuracoes ----------
const char* WIFI_SSID   = "Wokwi-GUEST";
const char* WIFI_PASS   = "";
const char* MQTT_HOST   = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;
const char* ID_PACIENTE = "paciente-1006";

// Hierarquia aberta de topicos (interoperavel com qualquer sistema)
const char* T_GLICEMIA = "mackenzie/cgm-iot/paciente-1006/glicemia";
const char* T_ALERTA   = "mackenzie/cgm-iot/paciente-1006/alerta";
const char* T_INSULINA = "mackenzie/cgm-iot/paciente-1006/insulina";
const char* T_STATUS   = "mackenzie/cgm-iot/paciente-1006/status";
const char* T_COMANDO  = "mackenzie/cgm-iot/paciente-1006/comando";  // v2
const char* T_ACK      = "mackenzie/cgm-iot/paciente-1006/ack";      // v2

// Tempo acelerado: 1 leitura de CGM (15 min reais) a cada 5 s de simulacao
const unsigned long INTERVALO_AMOSTRA_MS = 5000;

// Limiares clinicos (mg/dL) - consenso internacional de time in range
const int HIPO_URGENTE  = 54;
const int HIPO          = 70;
const int HIPER         = 180;
const int HIPER_URGENTE = 250;

// ---------- Pinos ----------
const int PIN_BTN_RAPIDA = 25;
const int PIN_BTN_BASAL  = 26;
const int PIN_POT_DOSE   = 34;
const int PIN_BUZZER     = 27;
const int PIN_LED_VERDE  = 32;
const int PIN_LED_AMAR   = 33;
const int PIN_LED_VERM   = 14;

LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClient espClient;
PubSubClient mqtt(espClient);

int idxAmostra = 0;
int idxInsulina = 0;
unsigned long tUltimaAmostra = 0;
int historico[4] = {0,0,0,0};
int ultimoAlerta = 0;

// ---------- Setas de tendencia (criterio analogo ao do FreeStyle Libre) ----------
String setaTendencia(float taxa) {
  if (taxa >  2.0) return "^^";
  if (taxa >  1.0) return "^";
  if (taxa < -2.0) return "vv";
  if (taxa < -1.0) return "v";
  return "->";
}

// ---------- v2: comando remoto de atuadores via MQTT ----------
// Payload esperado (JSON): {"id":123,"atuador":"buzzer"} ou {"id":124,"atuador":"led_amarelo"}
void aoReceberComando(char* topic, byte* payload, unsigned int length) {
  unsigned long t0 = millis();
  char msg[160];
  unsigned int n = length < sizeof(msg)-1 ? length : sizeof(msg)-1;
  memcpy(msg, payload, n); msg[n] = '\0';

  // extrai o id (parse simples, sem biblioteca JSON)
  long id = -1;
  char* p = strstr(msg, "\"id\":");
  if (p) id = atol(p + 5);

  const char* atuador = "buzzer";
  if (strstr(msg, "led_verde"))        atuador = "led_verde";
  else if (strstr(msg, "led_amarelo")) atuador = "led_amarelo";
  else if (strstr(msg, "led_vermelho"))atuador = "led_vermelho";

  // aciona o atuador solicitado
  if (strcmp(atuador, "buzzer") == 0) {
    tone(PIN_BUZZER, 1000, 300);
  } else {
    int pino = PIN_LED_VERDE;
    if (strcmp(atuador, "led_amarelo") == 0)  pino = PIN_LED_AMAR;
    if (strcmp(atuador, "led_vermelho") == 0) pino = PIN_LED_VERM;
    digitalWrite(pino, HIGH); delay(300); digitalWrite(pino, LOW);
  }
  unsigned long dt = millis() - t0;  // tempo local comando -> acao concluida

  // publica a confirmacao (ack) ecoando o id para medicao de RTT
  char ack[160];
  snprintf(ack, sizeof(ack),
    "{\"id\":%ld,\"atuador\":\"%s\",\"status\":\"acionado\",\"t_acao_ms\":%lu}",
    id, atuador, dt);
  mqtt.publish(T_ACK, ack);
  Serial.printf("CMD id=%ld atuador=%s acionado em %lu ms\n", id, atuador, dt);
}

void conectarWiFi() {
  lcd.setCursor(0,0); lcd.print("Conectando WiFi ");
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);   // canal 6: conexao imediata no Wokwi
  while (WiFi.status() != WL_CONNECTED) { delay(300); }
  lcd.setCursor(0,0); lcd.print("WiFi OK         ");
}

void conectarMQTT() {
  while (!mqtt.connected()) {
    String cid = "cgm-iot-" + String(random(0xffff), HEX);
    if (mqtt.connect(cid.c_str(), T_STATUS, 1, true, "offline")) {
      mqtt.publish(T_STATUS, "online", true);   // Last Will + retained
      mqtt.subscribe(T_COMANDO);                // v2: controle via internet
    } else { delay(1000); }
  }
}

void alerta(const char* tipo, int glicemia, const char* msg) {
  char payload[160];
  snprintf(payload, sizeof(payload),
    "{\"paciente\":\"%s\",\"tipo\":\"%s\",\"glicemia\":%d,\"msg\":\"%s\"}",
    ID_PACIENTE, tipo, glicemia, msg);
  mqtt.publish(T_ALERTA, payload);
  tone(PIN_BUZZER, 880, 400);
}

void publicarInsulina(float dose, const char* tipo, const char* origem, int glicemia) {
  char payload[160];
  snprintf(payload, sizeof(payload),
    "{\"paciente\":\"%s\",\"dose_UI\":%.1f,\"tipo\":\"%s\",\"origem\":\"%s\",\"glicemia\":%d,\"t_ms\":%lu}",
    ID_PACIENTE, dose, tipo, origem, glicemia, millis());
  mqtt.publish(T_INSULINA, payload);
}

void setup() {
  pinMode(PIN_BTN_RAPIDA, INPUT_PULLUP);
  pinMode(PIN_BTN_BASAL,  INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_AMAR,  OUTPUT);
  pinMode(PIN_LED_VERM,  OUTPUT);
  Serial.begin(115200);
  lcd.init(); lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Monitor CGM v2.0");
  delay(800);
  conectarWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(aoReceberComando);   // v2
  conectarMQTT();
}

void loop() {
  if (!mqtt.connected()) conectarMQTT();
  mqtt.loop();

  int doseSel = map(analogRead(PIN_POT_DOSE), 0, 4095, 1, 30);
  int glicemiaAtual = CGM_DATA[idxAmostra];

  // ---- Diario de insulina: registro manual pelos botoes ----
  if (digitalRead(PIN_BTN_RAPIDA) == LOW) {
    publicarInsulina(doseSel, "rapida", "manual", glicemiaAtual);
    lcd.setCursor(0,1); lcd.print("Rapida "); lcd.print(doseSel); lcd.print("UI ok  ");
    tone(PIN_BUZZER, 1320, 120); delay(400);
  }
  if (digitalRead(PIN_BTN_BASAL) == LOW) {
    publicarInsulina(doseSel, "basal", "manual", glicemiaAtual);
    lcd.setCursor(0,1); lcd.print("Basal "); lcd.print(doseSel); lcd.print("UI ok   ");
    tone(PIN_BUZZER, 1320, 120); delay(400);
  }

  // ---- Nova leitura do sensor (virtual) de CGM ----
  if (millis() - tUltimaAmostra >= INTERVALO_AMOSTRA_MS) {
    tUltimaAmostra = millis();
    int g = CGM_DATA[idxAmostra];

    for (int i=3;i>0;i--) historico[i]=historico[i-1];
    historico[0]=g;
    float taxa = (historico[1]>0) ? (historico[0]-historico[1])/15.0 : 0;
    String seta = setaTendencia(taxa);
    int projecao = g + (int)(taxa*15.0);

    char payload[220];
    snprintf(payload, sizeof(payload),
      "{\"paciente\":\"%s\",\"glicemia\":%d,\"tendencia\":\"%s\",\"taxa\":%.2f,\"projecao15\":%d,\"amostra\":%d,\"t_ms\":%lu}",
      ID_PACIENTE, g, seta.c_str(), taxa, projecao, idxAmostra, millis());
    mqtt.publish(T_GLICEMIA, payload);
    Serial.printf("CGM %d mg/dL %s (proj15=%d)\n", g, seta.c_str(), projecao);

    digitalWrite(PIN_LED_VERDE, LOW); digitalWrite(PIN_LED_AMAR, LOW); digitalWrite(PIN_LED_VERM, LOW);
    if (g < HIPO_URGENTE)      { digitalWrite(PIN_LED_VERM,1); if(ultimoAlerta!=1){alerta("hipo_urgente",g,"Hipoglicemia SEVERA - agir agora");ultimoAlerta=1;} }
    else if (g < HIPO)         { digitalWrite(PIN_LED_VERM,1); if(ultimoAlerta!=2){alerta("hipo",g,"Hipoglicemia - ingerir carboidrato");ultimoAlerta=2;} }
    else if (g > HIPER_URGENTE){ digitalWrite(PIN_LED_VERM,1); if(ultimoAlerta!=3){alerta("hiper_urgente",g,"Hiperglicemia severa - verificar cetonas");ultimoAlerta=3;} }
    else if (g > HIPER)        { digitalWrite(PIN_LED_AMAR,1); if(ultimoAlerta!=4){alerta("hiper",g,"Hiperglicemia");ultimoAlerta=4;} }
    else if (projecao < HIPO && taxa < -1.0) {
      digitalWrite(PIN_LED_AMAR,1);
      if(ultimoAlerta!=5){alerta("hipo_prevista",g,"Queda rapida - hipoglicemia prevista em 15 min");ultimoAlerta=5;}
    }
    else { digitalWrite(PIN_LED_VERDE,1); ultimoAlerta=0; }

    while (idxInsulina < N_INS && INS_EVENTS[idxInsulina][0] == idxAmostra) {
      float dose = INS_EVENTS[idxInsulina][1]/10.0;
      const char* tipo = INS_EVENTS[idxInsulina][2]==0 ? "rapida" : "basal";
      publicarInsulina(dose, tipo, "dataset", g);
      idxInsulina++;
    }

    lcd.setCursor(0,0);
    lcd.print("G:"); lcd.print(g); lcd.print(" "); lcd.print(seta); lcd.print("      ");
    lcd.setCursor(0,1);
    lcd.print("Dose:"); lcd.print(doseSel); lcd.print("UI ");
    if (g<HIPO) lcd.print("HIPO!  "); else if (g>HIPER) lcd.print("HIPER! "); else lcd.print("alvo   ");

    idxAmostra = (idxAmostra + 1) % N_CGM;
    if (idxAmostra == 0) idxInsulina = 0;
  }
}
