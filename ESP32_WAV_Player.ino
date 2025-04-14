/**
 * ESP32_WAV_Player.ino
 * 
 * Reproductor de archivos WAV optimizado para ESP32-C3 XIAO con DAC8551
 * Basado en el proyecto Radio Music de Tom Whitwell, adaptado para ESP32-C3
 * 
 * Características:
 * - Reproducción de archivos WAV 44.1kHz 16-bit desde tarjeta SD
 * - Salida de audio a través de DAC8551
 * - Optimizado para memoria y manejo de buffers
 * 
 * Conexiones:
 * - SPI MOSI: Pin 10
 * - SPI MISO: Pin 9
 * - SPI SCK: Pin 8
 * - SD CS: Pin 7
 * - DAC CS: Pin 20
 * 
 * Fecha: 2025-04-14
 * Actualizado para Arduino core 3.0
 */

#include "Config.h"
#include "AudioBuffer.h"
#include "DAC8551.h"
#include "WavReader.h"
#include "SDManager.h"
#include <Arduino.h>
#include <SPI.h>

// Instancias globales
AudioBuffer audioBuffer;
DAC8551 dac;
WavReader wavReader;
SDManager sdManager;

// Variables de estado
volatile bool isPlaying = false;
volatile bool bufferUnderrun = false;
volatile uint32_t sampleCounter = 0;
int16_t lastSample = 0;

// Variables para estadísticas
uint32_t bufferUnderrunCount = 0;
uint32_t lastReportTime = 0;
uint32_t startTime = 0;

// Timer para reproducción de audio
hw_timer_t *audioTimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Rutina de servicio de interrupción para reproducción de audio
void IRAM_ATTR onAudioTimer() {
    portENTER_CRITICAL_ISR(&timerMux);
    
    int16_t sample = 0;
    
    // Leer muestra del buffer
    if (audioBuffer.readSample(sample)) {
        // Reproducir muestra
        dac.writeWithOffset(sample);
        lastSample = sample;
        sampleCounter++;
        bufferUnderrun = false;
    } else {
        // Buffer underrun - mantener último valor
        dac.writeWithOffset(lastSample);
        bufferUnderrun = true;
    }
    
    portEXIT_CRITICAL_ISR(&timerMux);
}

// Configuración del timer de hardware para Arduino core 3.0
void setupAudioTimer() {
    // En Arduino core 3.0, timerBegin solo acepta frecuencia
    // Usamos 1MHz como base para el timer (1 microsegundo de resolución)
    audioTimer = timerBegin(TIMER_BASE_FREQ);
    
    // En Arduino core 3.0, timerAttachInterrupt no tiene el parámetro edge-triggered
    timerAttachInterrupt(audioTimer, &onAudioTimer);
    
    // Configurar alarma para disparar cada ~22.67 microsegundos (44.1kHz)
    // Usamos timerAlarm en lugar de timerAlarmWrite y timerAlarmEnable
    timerAlarm(audioTimer, TIMER_ALARM_VALUE, true, 0);
    
    DEBUG_PRINTLN("Timer de audio configurado:");
    DEBUG_PRINTF("  Frecuencia base: %d Hz\n", TIMER_BASE_FREQ);
    DEBUG_PRINTF("  Valor de alarma: %d\n", TIMER_ALARM_VALUE);
    DEBUG_PRINTF("  Frecuencia estimada: %.2f Hz\n", 
                (float)TIMER_BASE_FREQ / TIMER_ALARM_VALUE);
}

// Inicializa el hardware
bool initHardware() {
    // Configurar pines SPI
    SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
    
    // Inicializar DAC
    dac.begin();
    dac.setSilence();
    
    // Inicializar tarjeta SD
    if (!sdManager.begin()) {
        DEBUG_PRINTLN("Error: No se pudo inicializar la tarjeta SD");
        return false;
    }
    
    // Inicializar buffer de audio
    if (!audioBuffer.init(BUFFER_SIZE)) {
        DEBUG_PRINTLN("Error: No se pudo inicializar el buffer de audio");
        return false;
    }
    
    // Configurar timer de audio
    setupAudioTimer();
    
    return true;
}

// Escanea archivos WAV en la tarjeta SD
void scanWavFiles() {
    uint16_t fileCount = sdManager.scanWavFiles();
    
    if (fileCount == 0) {
        DEBUG_PRINTLN("No se encontraron archivos WAV compatibles");
    } else {
        sdManager.printFileList();
    }
}

// Inicia la reproducción de un archivo WAV
bool startPlayback(const char* filename) {
    // Detener reproducción actual
    stopPlayback();
    
    DEBUG_PRINTF("Iniciando reproducción: %s\n", filename);
    
    // Abrir archivo WAV
    if (!wavReader.open(filename)) {
        DEBUG_PRINTLN("Error: No se pudo abrir el archivo WAV");
        return false;
    }
    
    // Limpiar buffer
    audioBuffer.clear();
    
    // Llenar buffer inicial
    fillBuffer();
    
    if (audioBuffer.isEmpty()) {
        DEBUG_PRINTLN("Error: No se pudo llenar el buffer inicial");
        wavReader.close();
        return false;
    }
    
    // Iniciar reproducción
    sampleCounter = 0;
    bufferUnderrunCount = 0;
    startTime = millis();
    isPlaying = true;
    
    // Habilitar timer (en Arduino core 3.0 usamos timerStart)
    timerStart(audioTimer);
    
    DEBUG_PRINTLN("Reproducción iniciada");
    return true;
}

// Detiene la reproducción
void stopPlayback() {
    // Deshabilitar timer (en Arduino core 3.0 usamos timerStop)
    timerStop(audioTimer);
    
    // Cerrar archivo
    wavReader.close();
    
    // Limpiar buffer
    audioBuffer.clear();
    
    // Silenciar DAC
    dac.setSilence();
    
    isPlaying = false;
    DEBUG_PRINTLN("Reproducción detenida");
}

// Llena el buffer de audio desde el archivo WAV
void fillBuffer() {
    if (wavReader.isEndOfFile()) {
        return;
    }
    
    // Calcular espacio disponible en el buffer
    uint16_t freeSpace = audioBuffer.getFreeSpace();
    if (freeSpace < SD_BLOCK_SIZE) {
        return;  // No hay suficiente espacio
    }
    
    // Ajustar tamaño de lectura al espacio disponible
    // Usamos safe_min para evitar problemas de tipos
    uint16_t bytesToRead = safe_min(freeSpace, SD_BLOCK_SIZE);
    
    // Leer datos del archivo
    uint8_t tempBuffer[SD_BLOCK_SIZE];
    uint16_t bytesRead = wavReader.readBlock(tempBuffer, bytesToRead);
    
    if (bytesRead > 0) {
        // Escribir datos al buffer de audio
        uint16_t bytesWritten = audioBuffer.write(tempBuffer, bytesRead);
        
        if (bytesWritten < bytesRead) {
            DEBUG_PRINTLN("Advertencia: No se pudieron escribir todos los datos al buffer");
        }
    }
}

// Muestra estadísticas de reproducción
void showPlaybackStats() {
    uint32_t currentTime = millis();
    uint32_t elapsedTime = currentTime - startTime;
    
    if (currentTime - lastReportTime >= 1000) {  // Cada segundo
        lastReportTime = currentTime;
        
        // Calcular estadísticas
        float playTime = (float)sampleCounter / SAMPLE_RATE;
        float bufferLevel = (float)audioBuffer.getPercentFull();
        
        DEBUG_PRINTLN("Estadísticas de reproducción:");
        DEBUG_PRINTF("  Tiempo de reproducción: %.2f s\n", playTime);
        DEBUG_PRINTF("  Nivel de buffer: %.1f%%\n", bufferLevel);
        DEBUG_PRINTF("  Buffer underruns: %d\n", bufferUnderrunCount);
        DEBUG_PRINTF("  Memoria libre: %d bytes\n", ESP.getFreeHeap());
    }
    
    // Contar buffer underruns
    if (bufferUnderrun) {
        bufferUnderrunCount++;
        bufferUnderrun = false;
    }
}

void setup() {
    // Inicializar Serial para debug
    DEBUG_BEGIN(115200);
    delay(500);  // Tiempo para estabilizar
    
    DEBUG_PRINTLN("\n--- ESP32-C3 WAV Player ---");
    DEBUG_PRINTLN("Fecha: 2025-04-14");
    DEBUG_PRINTLN("Adaptado para Arduino core 3.0");
    
    // Mostrar información del sistema
    DEBUG_PRINTF("CPU: %d MHz\n", CPU_FREQ_MHZ);
    DEBUG_PRINTF("Memoria libre: %d bytes\n", ESP.getFreeHeap());
    
    // Inicializar hardware
    if (!initHardware()) {
        DEBUG_PRINTLN("Error: Fallo en la inicialización del hardware");
        while (1) {
            delay(1000);
        }
    }
    
    // Escanear archivos WAV
    scanWavFiles();
    
    // Prueba del DAC
    DEBUG_PRINTLN("Ejecutando prueba del DAC...");
    dac.testSine(2);
    
    // Iniciar reproducción del primer archivo si existe
    if (sdManager.getFileCount() > 0) {
        const char* firstFile = sdManager.getFileName(0);
        if (firstFile) {
            startPlayback(firstFile);
        }
    }
}

void loop() {
    // Si está reproduciendo, mantener el buffer lleno
    if (isPlaying) {
        // Llenar buffer si está por debajo del umbral
        if (audioBuffer.isLow() && !wavReader.isEndOfFile()) {
            fillBuffer();
        }
        
        // Mostrar estadísticas
        showPlaybackStats();
        
        // Verificar fin de archivo
        if (wavReader.isEndOfFile() && audioBuffer.isEmpty()) {
            DEBUG_PRINTLN("Fin de archivo alcanzado");
            
            // Reproducir siguiente archivo si existe
            uint16_t fileCount = sdManager.getFileCount();
            if (fileCount > 1) {
                // Buscar el archivo actual
                const char* currentFile = wavReader.getFileName();
                int currentIndex = -1;
                
                for (uint16_t i = 0; i < fileCount; i++) {
                    if (strcmp(currentFile, sdManager.getFileName(i)) == 0) {
                        currentIndex = i;
                        break;
                    }
                }
                
                // Reproducir siguiente archivo
                int nextIndex = (currentIndex + 1) % fileCount;
                const char* nextFile = sdManager.getFileName(nextIndex);
                
                DEBUG_PRINTF("Reproduciendo siguiente archivo: %s\n", nextFile);
                startPlayback(nextFile);
            } else {
                // Detener reproducción
                stopPlayback();
            }
        }
    }
    
    // Procesar comandos seriales si están disponibles
    if (Serial.available()) {
        char cmd = Serial.read();
        
        switch (cmd) {
            case 'p':  // Play/Pause
                if (isPlaying) {
                    stopPlayback();
                } else if (sdManager.getFileCount() > 0) {
                    startPlayback(sdManager.getFileName(0));
                }
                break;
                
            case 'n':  // Next file
                if (sdManager.getFileCount() > 0) {
                    // Buscar el archivo actual
                    const char* currentFile = wavReader.getFileName();
                    int currentIndex = 0;
                    
                    if (isPlaying) {
                        for (uint16_t i = 0; i < sdManager.getFileCount(); i++) {
                            if (strcmp(currentFile, sdManager.getFileName(i)) == 0) {
                                currentIndex = i;
                                break;
                            }
                        }
                    }
                    
                    // Reproducir siguiente archivo
                    int nextIndex = (currentIndex + 1) % sdManager.getFileCount();
                    startPlayback(sdManager.getFileName(nextIndex));
                }
                break;
                
            case 'l':  // List files
                sdManager.printFileList();
                break;
                
            case 's':  // Status
                if (isPlaying) {
                    DEBUG_PRINTF("Reproduciendo: %s\n", wavReader.getFileName());
                    DEBUG_PRINTF("Buffer: %d%% lleno\n", audioBuffer.getPercentFull());
                    DEBUG_PRINTF("Memoria libre: %d bytes\n", ESP.getFreeHeap());
                } else {
                    DEBUG_PRINTLN("Reproducción detenida");
                }
                break;
                
            case 't':  // Test DAC
                dac.testSine(2);
                break;
                
            case 'h':  // Help
                DEBUG_PRINTLN("Comandos disponibles:");
                DEBUG_PRINTLN("  p - Play/Pause");
                DEBUG_PRINTLN("  n - Siguiente archivo");
                DEBUG_PRINTLN("  l - Listar archivos");
                DEBUG_PRINTLN("  s - Estado");
                DEBUG_PRINTLN("  t - Probar DAC");
                DEBUG_PRINTLN("  h - Ayuda");
                break;
        }
        
        // Limpiar buffer serial
        while (Serial.available()) {
            Serial.read();
        }
    }
}
