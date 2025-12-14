#include <SPI.h> 
#include <SD.h> 

const int CHIP_SELECT_SD = 10;
const char* NOMBRE_ARCHIVO = "PRUEBA.TXT"; 

void setup() {
  Serial.begin(9600);
  pinMode(CHIP_SELECT_SD, OUTPUT);
  
  Serial.print(F("Inicializando SD\n"));
  if (!SD.begin(CHIP_SELECT_SD)) {
    Serial.println(F(" Error. SD NO DETECTADA."));
    while (true); 
  }
  Serial.println(F("SD INICIALIZADA"));
  
  File testFile = SD.open(NOMBRE_ARCHIVO, FILE_WRITE);
  
  if (testFile) {
    testFile.println( String(F("Prueba de escritura: ")));
    testFile.close();
    Serial.println(F("Archivo 'PRUEBA.TXT' escrito y cerrado correctamente."));
  } else {
    Serial.println(F("Error: No se pudo abrir 'PRUEBA.TXT' para escritura."));
  }
}

void loop() {
  // Sin actividad en el loop
}
