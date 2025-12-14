#include <SoftwareSerial.h>
#include <SD.h> 
#include <string.h> 
#include <Wire.h>   
#include "RTClib.h" 

// --- I. DEFINICIÓN DE PINES Y HARDWARE ---
SoftwareSerial bluetooth(2, 3); 
const int CHIP_SELECT_SD = 10; 

// Feedback LEDs
const int BLUE_LED_STATUS = 7;
const int RED_LED_ERROR = 6;  
const int GREEN_LED_SUCCESS = 5; 

// Object for the DS3231 RTC module
RTC_DS3231 rtc; 

// --- II. VARIABLES DE ESTADO Y DATOS OPTIMIZADAS (RAM) ---
bool isConnected = false;  
long previousMillisBlink = 0;       
const long intervalBlink = 250;     
int ledState = LOW;

// Buffer OPTIMIZADO
char btBuffer[31]; 
int btBufferIndex = 0;

// ⬅️ VARIABLE GLOBAL PARA MEDIR LATENCIA
unsigned long startTime = 0; 

// VARIABLES FOR VALIDATION AND REGISTRATION
const int MAX_STUDENTS = 15; 
const int ID_LENGTH = 10;  
char studentIDsList[MAX_STUDENTS][ID_LENGTH]; 
int totalStudents = 0;       

const char* STUDENTS_FILE = "ALUMNOS.TXT"; 
char dateTimeBuffer[20];   
char fileNameBuffer[13];   
char receivedStudentID[ID_LENGTH]; 

// ----------------------------------------------------------------------------------
// CORE FUNCTION: CHECK IF ALREADY REGISTERED (SD READ)
// ----------------------------------------------------------------------------------
bool checkIfAlreadyRegistered(char* id, char* filename) {
    File logFile = SD.open(filename);
    if (!logFile) { return false; }
    char lineBuffer[40]; int lineIndex = 0;
    while (logFile.available()) {
        char c = logFile.read();
        if (c != ',' && c != '\n' && c != '\r' && lineIndex < 39) {
            lineBuffer[lineIndex++] = c;
        } else if (c == ',' || c == '\n' || c == '\r') {
            lineBuffer[lineIndex] = '\0';
            if (lineIndex > 0 && strcmp(id, lineBuffer) == 0) {
                logFile.close();
                return true;
            }
            lineIndex = 0;
        }
    }
    logFile.close();
    return false;
} 

// ----------------------------------------------------------------------------------
// UTILITY FUNCTIONS (GET RTC DATA, LOAD LIST, VALIDATE, LOG) [Resto de funciones]
// ----------------------------------------------------------------------------------

char* getRTCFilename() {
    DateTime now = rtc.now();
    sprintf(fileNameBuffer, "%04d%02d%02d.CSV", now.year(), now.month(), now.day());
    return fileNameBuffer;
}

char* getRTCDateTime() {
    DateTime now = rtc.now();
    sprintf(dateTimeBuffer, "%04d/%02d/%02d,%02d:%02d:%02d", 
        now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    return dateTimeBuffer;
}

bool loadStudentList(const char* filename) {
  File dataFile = SD.open(filename);
  if (!dataFile) { return false; }
  Serial.print(F("Cargando IDs..."));
  totalStudents = 0;
  int currentIDIndex = 0; 
  while (dataFile.available() && totalStudents < MAX_STUDENTS) {
    char c = dataFile.read();
    if (c >= '0' && c <= '9') {
      if (currentIDIndex < ID_LENGTH - 1) { studentIDsList[totalStudents][currentIDIndex++] = c; }
    } else if (c == '\n' || c == '\r') {
      if (currentIDIndex > 0) { studentIDsList[totalStudents][currentIDIndex] = '\0'; totalStudents++; }
      currentIDIndex = 0; 
      if (totalStudents >= MAX_STUDENTS) { break; }
    } 
  }
  if (currentIDIndex > 0 && totalStudents < MAX_STUDENTS) {
      studentIDsList[totalStudents][currentIDIndex] = '\0';
      totalStudents++;
  }
  dataFile.close();
  Serial.print(F("Total cargado: ")); Serial.println(totalStudents);
  return (totalStudents > 0); 
}

bool validateID(char* id) {
    if (strlen(id) < 8) { return false; }
    for (int i = 0; i < totalStudents; i++) {
        if (strcmp(id, studentIDsList[i]) == 0) { return true; }
    }
    return false;
}

bool logAttendance(char* id, char* dateTime, char* filename) {
    File logFile = SD.open(filename, FILE_WRITE); 
    if (logFile) {
        logFile.print(id);
        logFile.print(F(","));
        logFile.println(dateTime); 
        logFile.close();
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------------
// SETUP FUNCTION
// ----------------------------------------------------------------------------------
void setup() {
    pinMode(BLUE_LED_STATUS, OUTPUT); pinMode(RED_LED_ERROR, OUTPUT);
    pinMode(GREEN_LED_SUCCESS, OUTPUT); pinMode(CHIP_SELECT_SD, OUTPUT); 
    
    Serial.begin(9600); bluetooth.begin(9600); Wire.begin(); 
    
    digitalWrite(RED_LED_ERROR, LOW); digitalWrite(GREEN_LED_SUCCESS, LOW);
    
    Serial.println(F("Sistema de Asistencia v11.0 - Latency Test"));
    
    if (!rtc.begin()) {
        Serial.println(F("RTC [ERROR]"));
        while (true) { digitalWrite(RED_LED_ERROR, HIGH); delay(100); digitalWrite(RED_LED_ERROR, LOW); delay(100); }
    } Serial.println(F("RTC [OK]"));
    
    if (!SD.begin(CHIP_SELECT_SD)) {
        Serial.println(F("SD [ERROR]"));
        while (true) { digitalWrite(RED_LED_ERROR, HIGH); delay(200); digitalWrite(RED_LED_ERROR, LOW); delay(200); }
    } Serial.println(F("SD [OK]"));
    
    if (loadStudentList(STUDENTS_FILE)) {
        digitalWrite(GREEN_LED_SUCCESS, HIGH); delay(1000); digitalWrite(GREEN_LED_SUCCESS, LOW);
    } else {
        Serial.println(F("List [ERROR]"));
        while (true) { digitalWrite(RED_LED_ERROR, HIGH); delay(150); digitalWrite(RED_LED_ERROR, LOW); delay(150); }
    }
    
    Serial.println(F("\nWaiting for Bluetooth connection\n"));
}

// ----------------------------------------------------------------------------------
// AUXILIARY FUNCTION: BLUETOOTH STATUS LED CONTROL
// ----------------------------------------------------------------------------------
void controlBluetoothStatus() {
    if (!isConnected) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillisBlink >= intervalBlink) {
            previousMillisBlink = currentMillis;
            ledState = !ledState;
            digitalWrite(BLUE_LED_STATUS, ledState);
        }
    } else {
        digitalWrite(BLUE_LED_STATUS, LOW);
    }
}

// ----------------------------------------------------------------------------------
// MAIN CONTROL FUNCTION (LOOP)
// ----------------------------------------------------------------------------------
void loop() {
    
    controlBluetoothStatus();
    
    if (bluetooth.available()) {
        
        // T1 CAPTURE: Detect the start of a new transaction
        if (startTime == 0) { 
            startTime = millis(); 
        }

        char incomingChar = bluetooth.read();
        Serial.write(incomingChar);
        
        // 1. Bluetooth Buffer Management
        if (btBufferIndex < (sizeof(btBuffer) - 1)) {
            btBuffer[btBufferIndex++] = incomingChar;
            btBuffer[btBufferIndex] = '\0'; 
        } else {
            btBufferIndex = 0; 
        }
        
        // 2. STATUS DETECTION (CONEXIÓN/DESCONEXIÓN) - Lógica de control de LED
        if (strstr(btBuffer, "+CONNECTED") != NULL) { 
            isConnected = true; btBufferIndex = 0; 
            Serial.println(F("\n[STATUS: CONNECTED]")); return;
        } else if (strstr(btBuffer, "+PAIRABLE") != NULL) { 
            isConnected = false; btBufferIndex = 0; 
            Serial.println(F("\n[STATUS: PAIRABLE]")); return;
        }
        
        // 3. ATTENDANCE LOGIC: Process ID
        if (isConnected && (incomingChar == '\n' || incomingChar == '\r')) {
            
            // Isolate and clean the received ID
            strncpy(receivedStudentID, btBuffer, ID_LENGTH - 1);
            receivedStudentID[ID_LENGTH - 1] = '\0'; 
            
            int idLen = 0;
            while(idLen < ID_LENGTH - 1 && receivedStudentID[idLen] >= '0' && receivedStudentID[idLen] <= '9') { idLen++; }
            receivedStudentID[idLen] = '\0'; 

            // Critical Check
            if (strlen(receivedStudentID) < 8 || receivedStudentID[0] == '\0') { 
                btBufferIndex = 0; startTime = 0; // ⬅️ RESET POR BASURA
                return; 
            }
            
            // 3.1. VALIDACIÓN y REGISTRO
            char* currentFilename = getRTCFilename();
            
            if (validateID(receivedStudentID)) {
                
                if (checkIfAlreadyRegistered(receivedStudentID, currentFilename)) {
                    // FALLO 1: DOBLE REGISTRO
                    Serial.println(F("\nAlready registered today."));
                    digitalWrite(RED_LED_ERROR, HIGH); delay(1000); digitalWrite(RED_LED_ERROR, LOW);
                    
                    btBufferIndex = 0; startTime = 0; // ⬅️ RESET POR DOBLE REGISTRO
                } else {
                    // REGISTRO EXITOSO
                    char* currentTime = getRTCDateTime();
                    if (logAttendance(receivedStudentID, currentTime, currentFilename)) {
                        
                        // CÁLCULO FINAL Y CONFIRMACIÓN DE ÉXITO
                        unsigned long endTime = millis();
                        unsigned long latency = endTime - startTime;

                        Serial.print(F("\nSUCCESS! Latency: "));
                        Serial.print(latency);
                        Serial.println(F(" ms."));
                        
                        digitalWrite(GREEN_LED_SUCCESS, HIGH); delay(500); digitalWrite(GREEN_LED_SUCCESS, LOW);
                    } else {
                        // FALLO 2: ESCRITURA SD
                        Serial.println(F("SD Write Error!"));
                        digitalWrite(RED_LED_ERROR, HIGH);
                        
                        btBufferIndex = 0; startTime = 0; // ⬅️ RESET POR FALLO SD
                    }
                }
            } else {
                // FALLO 3: ID INVÁLIDA
                Serial.println(F("\nInvalid ID."));
                digitalWrite(RED_LED_ERROR, HIGH); delay(500); digitalWrite(RED_LED_ERROR, LOW);
                
                btBufferIndex = 0; startTime = 0; // ⬅️ RESET POR ID INVÁLIDA
            }

            btBufferIndex = 0; 
            startTime = 0; // ⬅️ RESET POR ÉXITO (Redundante, pero seguro)
        }

    } 
    
    // Forward data from PC to HC-06
    if (Serial.available()) {
        bluetooth.write(Serial.read());
    }
}
