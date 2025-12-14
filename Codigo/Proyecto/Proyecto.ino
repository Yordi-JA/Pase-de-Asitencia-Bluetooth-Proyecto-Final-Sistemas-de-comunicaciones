#include <SoftwareSerial.h>
#include <SD.h> 
#include <string.h> 
#include <Wire.h> 
#include "RTClib.h" 

// I. PIN AND HARDWARE DEFINITIONS ---
SoftwareSerial bluetooth(2, 3); 
const int CHIP_SELECT_SD = 10; 

// Feedback LEDs
const int BLUE_LED_STATUS = 7;  // Indicates Bluetooth connection status
const int RED_LED_ERROR = 6;    // Indicates error (HW fail, Invalid ID, Already Registered)
const int GREEN_LED_SUCCESS = 5; // Indicates successful registration

// Object for the DS3231 RTC module
RTC_DS3231 rtc; 

// II. OPTIMIZED STATE AND DATA VARIABLES (RAM) ---
bool isConnected = false;  
long previousMillisBlink = 0;      
const long intervalBlink = 250;      
int ledState = LOW;

// Optimized buffer for Status and Data (reduced from 51 to 31 bytes for Nano)
char btBuffer[31]; 
int btBufferIndex = 0;

//  VARIABLES FOR VALIDATION AND REGISTRATION 
const int MAX_STUDENTS = 10;   
const int ID_LENGTH = 10; // 9 digits + Null terminator
char studentIDsList[MAX_STUDENTS][ID_LENGTH]; 
int totalStudents = 0;           

const char* STUDENTS_FILE = "ALUMNOS.TXT";  // Master list filename
char dateTimeBuffer[20];    
char fileNameBuffer[13];    
char receivedStudentID[ID_LENGTH];  

// ----------------------------------------------------------------------------------
// CORE FUNCTION: CHECK IF ALREADY REGISTERED (SD READ)
// ----------------------------------------------------------------------------------

bool checkIfAlreadyRegistered(char* id, char* filename) {
    File logFile = SD.open(filename);
    
    if (!logFile) {
        // If the daily file doesn't exist, no one has registered yet.
        return false;
    }
    
    // Buffer to hold one line/ID from the log file
    char lineBuffer[40];    
    int lineIndex = 0;

    while (logFile.available()) {
        char c = logFile.read();

        // Read the ID until the comma (CSV separator) or end of line
        if (c != ',' && c != '\n' && c != '\r' && lineIndex < 39) {
            lineBuffer[lineIndex++] = c;
        } else if (c == ',' || c == '\n' || c == '\r') {
            // Compare the read ID
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
} // <--- MISSING CLOSING BRACE ADDED HERE!

// ----------------------------------------------------------------------------------
// UTILITY FUNCTION: GET FILENAME FROM RTC DATE
// ----------------------------------------------------------------------------------

char* getRTCFilename() {
    DateTime now = rtc.now();
    sprintf(fileNameBuffer, "%04d%02d%02d.CSV", 
        now.year(), now.month(), now.day());
    return fileNameBuffer;
}

// ----------------------------------------------------------------------------------
// UTILITY FUNCTION: GET FULL DATE AND TIME
// ----------------------------------------------------------------------------------

char* getRTCDateTime() {
    DateTime now = rtc.now();
    sprintf(dateTimeBuffer, "%04d/%02d/%02d,%02d:%02d:%02d", 
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second());
    return dateTimeBuffer;
}

// ----------------------------------------------------------------------------------
// CORE FUNCTION: LOAD STUDENT LIST FROM SD
// ----------------------------------------------------------------------------------

bool loadStudentList(const char* filename) {
    File dataFile = SD.open(filename);
    if (!dataFile) { return false; }
    
    totalStudents = 0;
    int currentIDIndex = 0; 
    
    while (dataFile.available() && totalStudents < MAX_STUDENTS) {
        char c = dataFile.read();
        
        if (c >= '0' && c <= '9') {
            if (currentIDIndex < ID_LENGTH - 1) { studentIDsList[totalStudents][currentIDIndex++] = c; }
        } 
        else if (c == '\n' || c == '\r') {
            if (currentIDIndex > 0) {
                studentIDsList[totalStudents][currentIDIndex] = '\0';
                totalStudents++;
            }
            currentIDIndex = 0; 
            if (totalStudents >= MAX_STUDENTS) { break; }
        } 
    }
    if (currentIDIndex > 0 && totalStudents < MAX_STUDENTS) {
        studentIDsList[totalStudents][currentIDIndex] = '\0';
        totalStudents++;
    }
    dataFile.close();
    return (totalStudents > 0); 
}

// ----------------------------------------------------------------------------------
// CORE FUNCTION: VALIDATION
// ----------------------------------------------------------------------------------

bool validateID(char* id) {
    // Check minimum length for robustness
    if (strlen(id) < 8) { return false; } 
    
    for (int i = 0; i < totalStudents; i++) {
        if (strcmp(id, studentIDsList[i]) == 0) { return true; }
    }
    return false; 
}

// ----------------------------------------------------------------------------------
// CORE FUNCTION: LOG ASSISTANCE TO SD
// ----------------------------------------------------------------------------------

bool logAttendance(char* id, char* dateTime, char* filename) {
    // Open the file in "append" mode (FILE_WRITE)
    File logFile = SD.open(filename, FILE_WRITE); 
    if (logFile) {
        // Write in CSV format: ID,DATE/TIME
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
    // Pin Initialization
    pinMode(BLUE_LED_STATUS, OUTPUT); pinMode(RED_LED_ERROR, OUTPUT);
    pinMode(GREEN_LED_SUCCESS, OUTPUT); pinMode(CHIP_SELECT_SD, OUTPUT);  
    
    // Serial Initialization
    Serial.begin(9600); bluetooth.begin(9600); Wire.begin();  
    
    // Initial LED state
    digitalWrite(RED_LED_ERROR, LOW); digitalWrite(GREEN_LED_SUCCESS, LOW);
    
    // 1. RTC VALIDATION
    if (!rtc.begin()) {
        Serial.println(F("RTC [ERROR]"));
        while (true) { digitalWrite(RED_LED_ERROR, HIGH); delay(100); digitalWrite(RED_LED_ERROR, LOW); delay(100); }
    } Serial.println(F("RTC [OK]"));
    
    // 2. SD CARD VALIDATION
    if (!SD.begin(CHIP_SELECT_SD)) {
        Serial.println(F("SD [ERROR]"));
        while (true) { digitalWrite(RED_LED_ERROR, HIGH); delay(200); digitalWrite(RED_LED_ERROR, LOW); delay(200); }
    } Serial.println(F("SD [OK]"));
    
    // 3. LOAD STUDENT LIST (Autonomous)
    if (loadStudentList(STUDENTS_FILE)) {
        Serial.println(F("List [OK]"));
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
        
        char incomingChar = bluetooth.read();
        Serial.write(incomingChar);  
        
        // 1. Bluetooth Buffer Management (Fill Buffer)
        if (btBufferIndex < (sizeof(btBuffer) - 1)) {
            btBuffer[btBufferIndex++] = incomingChar;
            btBuffer[btBufferIndex] = '\0';  
        } else {
            btBufferIndex = 0;  
        }
        
        // 2. STATUS DETECTION (CONNECTION/DISCONNECTION)
        if (strstr(btBuffer, "+CONNECTED") != NULL) {  
            isConnected = true;  
            btBufferIndex = 0;  
            Serial.println(F("\n"));
            return;  
        }  
        else if (strstr(btBuffer, "+PAIRABLE") != NULL) {  
            isConnected = false;  
            btBufferIndex = 0;
            Serial.println(F("\n"));
            return;  
        }
        
        // 3. ATTENDANCE LOGIC: Process ID
        if (isConnected && (incomingChar == '\n' || incomingChar == '\r')) {
            
            // Isolate and clean the received ID
            strncpy(receivedStudentID, btBuffer, ID_LENGTH - 1);
            receivedStudentID[ID_LENGTH - 1] = '\0';  
            
            // Truncate the string to ensure it only contains digits
            int idLen = 0;
            while(idLen < ID_LENGTH - 1 && receivedStudentID[idLen] >= '0' && receivedStudentID[idLen] <= '9') { idLen++; }
            receivedStudentID[idLen] = '\0';  

            // 3.1. JUNK CHECK (Avoid Red LED error for empty/junk strings)
            if (strlen(receivedStudentID) < 8 || receivedStudentID[0] == '\0') { 
                btBufferIndex = 0;  
                return; 
            }
                
            // 3.2. VALIDATION and LOGGING
            char* currentFilename = getRTCFilename();
            
            if (validateID(receivedStudentID)) {
                
                // Double Registration Check (SD Read operation)
                if (checkIfAlreadyRegistered(receivedStudentID, currentFilename)) {
                    Serial.println(F("\nStudent already registered today."));
                    digitalWrite(RED_LED_ERROR, HIGH); delay(1000); digitalWrite(RED_LED_ERROR, LOW); 
                } else {
                    // Successful Registration
                    char* currentTime = getRTCDateTime();
                    Serial.println(F("\nRegistered attendance."));
                    if (logAttendance(receivedStudentID, currentTime, currentFilename)) {
                        digitalWrite(GREEN_LED_SUCCESS, HIGH); delay(500); digitalWrite(GREEN_LED_SUCCESS, LOW);
                    } else {
                        // SD Write Failure
                        Serial.println(F("Error logging attendance to SD."));
                        digitalWrite(RED_LED_ERROR, HIGH); 
                    }
                }
            } else {
                // Failure: Invalid ID or not found in master list
                Serial.println(F("\nInvalid ID or not in the student list."));
                digitalWrite(RED_LED_ERROR, HIGH); delay(500); digitalWrite(RED_LED_ERROR, LOW);
            }

            btBufferIndex = 0; // Clear buffer for the next ID
        }

    }  
    
    // Forward data from PC to HC-06
    if (Serial.available()) {
        bluetooth.write(Serial.read());
    }
}
