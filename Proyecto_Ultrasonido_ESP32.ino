#include <WiFi.h>
#include <PubSubClient.h>

// ==========================================
// 1. CONFIGURACIÓN DE RED WI-FI (2.4 GHz) Y MQTT
// ==========================================
const char* ssid = "Redmi A5";    // Pon aquí tu red 2.4 GHz o Hotspot
const char* password = "hola1234";   // Pon aquí tu contraseña

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "mi_proyecto/ultrasonido";

// ==========================================
// 2. DEFINICIÓN DE PINES Y CONSTANTES
// ==========================================
const int PIN_TRIG = 18;  // GPIO 18 a TRIG
const int PIN_ECHO = 19;  // GPIO 19 a ECHO (con divisor de voltaje 3.3V)

const float DISTANCIA_MIN_CM = 25.0;  // Punto ciego del JSN-SR04T
const float DISTANCIA_MAX_CM = 450.0;

unsigned long tiempoAnteriorMedicion = 0;
const unsigned long INTERVALO_MEDICION_MS = 2000;

unsigned long tiempoAnteriorReconexion = 0;
const unsigned long INTERVALO_RECONEXION_MS = 5000;

WiFiClient espClient;
PubSubClient client(espClient);

// ==========================================
// 3. FUNCIONES AUXILIARES
// ==========================================
void configurarWiFi() {
  Serial.println();
  Serial.print("[WiFi] Conectando a la red: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    unsigned long t = millis();
    while (millis() - t < 500) { yield(); }
  }

  Serial.println("\n[WiFi] ¡Conectado exitosamente!");
  Serial.print("[WiFi] Dirección IP asignada: ");
  Serial.println(WiFi.localIP());
}

bool intentarReconexionMQTT() {
  String clientId = "ESP32_JSNSR04T_" + String(random(0xffff), HEX);
  Serial.print("[MQTT] Intentando conexión como ");
  Serial.print(clientId);
  Serial.print("... ");

  if (client.connect(clientId.c_str())) {
    Serial.println("¡Conectado al broker!");
    return true;
  } else {
    Serial.print("Fallo, rc=");
    Serial.print(client.state());
    Serial.println(" -> Se reintentará en 5 segundos.");
    return false;
  }
}

float medirDistanciaCM() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(20);
  digitalWrite(PIN_TRIG, LOW);

  long duracion_us = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duracion_us == 0) return -1.0;

  float distancia_cm = (duracion_us * 0.0343) / 2.0;
  if (distancia_cm < DISTANCIA_MIN_CM || distancia_cm > DISTANCIA_MAX_CM) {
    return -1.0;
  }
  return distancia_cm;
}

// ==========================================
// 4. SETUP Y LOOP PRINCIPAL
// ==========================================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  randomSeed(micros());
  configurarWiFi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  unsigned long tiempoActual = millis();

  if (!client.connected()) {
    if (tiempoActual - tiempoAnteriorReconexion >= INTERVALO_RECONEXION_MS) {
      tiempoAnteriorReconexion = tiempoActual;
      intentarReconexionMQTT();
    }
  } else {
    client.loop();
  }

  if (tiempoActual - tiempoAnteriorMedicion >= INTERVALO_MEDICION_MS) {
    tiempoAnteriorMedicion = tiempoActual;
    float distancia = medirDistanciaCM();

    if (distancia > 0) {
      char payload[16];
      dtostrf(distancia, 4, 1, payload);

      Serial.print("[Sensor] Distancia válida: ");
      Serial.print(payload);
      Serial.println(" cm");

      if (client.connected()) {
        client.publish(mqtt_topic, payload);
        Serial.print("[MQTT] Publicado en '");
        Serial.print(mqtt_topic);
        Serial.print("': ");
        Serial.println(payload);
      }
    } else {
      Serial.println("[Sensor] Lectura descartada: Objeto en punto ciego (< 25 cm) o fuera de alcance.");
    }
  }
}