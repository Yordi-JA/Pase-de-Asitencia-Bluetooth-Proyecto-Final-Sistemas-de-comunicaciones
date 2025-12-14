const int LED_AZUL = 7;
const int LED_ROJO = 6;
const int LED_VERDE = 5;

void setup() {
  pinMode(LED_AZUL, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  Serial.begin(9600);
  Serial.println(F("--- PRUEBA UNITARIA: LEDs ---"));
}

void loop() {
  // Secuencia de encendido para verificar cada pin
  
  Serial.println(F("LED Azul ON (D7)"));
  digitalWrite(LED_AZUL, HIGH);
  delay(500);
  digitalWrite(LED_AZUL, LOW);
  
  Serial.println(F("LED Rojo ON (D6)"));
  digitalWrite(LED_ROJO, HIGH);
  delay(500);
  digitalWrite(LED_ROJO, LOW);
  
  Serial.println(F("LED Verde ON (D5)"));
  digitalWrite(LED_VERDE, HIGH);
  delay(500);
  digitalWrite(LED_VERDE, LOW);
}
