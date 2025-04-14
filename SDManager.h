/**
 * SDManager.h
 * 
 * Gestión optimizada de tarjeta SD y archivos WAV para ESP32-C3 XIAO
 * 
 * Fecha: 2025-04-14
 * Actualizado para Arduino core 3.0
 */

#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include "Config.h"
#include "WavReader.h"
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

class SDManager {
private:
    bool sdInitialized;                     // Estado de inicialización de SD
    uint8_t csPin;                          // Pin CS para SD
    char fileList[MAX_FILES][MAX_FILENAME_LENGTH]; // Lista de archivos WAV
    uint16_t fileCount;                     // Número de archivos encontrados
    uint8_t maxRecursionDepth;              // Profundidad máxima de recursión

public:
    // Constructor simplificado
    SDManager(uint8_t cs = SD_CS_PIN) : 
        sdInitialized(false), 
        csPin(cs), 
        fileCount(0),
        maxRecursionDepth(3) {  // Permitir hasta 3 niveles de profundidad
    }

    // Inicializa la tarjeta SD
    bool begin() {
        // Configurar pin CS
        pinMode(csPin, OUTPUT);
        digitalWrite(csPin, HIGH);
        
        DEBUG_PRINTLN("Inicializando tarjeta SD...");
        DEBUG_PRINTF("  Usando pin CS: %d\n", csPin);
        
        // Pequeña pausa para estabilización
        delay(100);
        
        // Inicializar SD
        if (!SD.begin(csPin)) {
            DEBUG_PRINTLN("Error: No se pudo inicializar la tarjeta SD");
            return false;
        }
        
        sdInitialized = true;
        DEBUG_PRINTLN("Tarjeta SD inicializada correctamente");
        
        // Verificar funcionamiento listando archivos para diagnóstico
        listRootDirectory();
        
        return true;
    }

    // Escanea archivos WAV en la tarjeta SD
    uint16_t scanWavFiles() {
        if (!sdInitialized) {
            DEBUG_PRINTLN("Error: Tarjeta SD no inicializada");
            return 0;
        }
        
        fileCount = 0;
        DEBUG_PRINTLN("Escaneando archivos WAV...");
        
        // Abrir directorio raíz
        File root = SD.open("/");
        if (!root) {
            DEBUG_PRINTLN("Error: No se pudo abrir el directorio raíz");
            return 0;
        }
        
        // Escanear archivos en el directorio raíz y subdirectorios
        scanDirectory(root, "/", 0);
        root.close();
        
        DEBUG_PRINTF("Total de archivos WAV encontrados: %d\n", fileCount);
        return fileCount;
    }

    // Obtiene el número de archivos encontrados
    uint16_t getFileCount() const {
        return fileCount;
    }

    // Obtiene el nombre de un archivo por índice
    const char* getFileName(uint16_t index) const {
        if (index >= fileCount) {
            return nullptr;
        }
        return fileList[index];
    }

    // Imprime la lista de archivos encontrados
    void printFileList() {
        DEBUG_PRINTLN("Lista de archivos WAV:");
        for (uint16_t i = 0; i < fileCount; i++) {
            DEBUG_PRINTF("%3d: %s\n", i + 1, fileList[i]);
        }
    }

private:
    // Lista los archivos en el directorio raíz para diagnóstico
    void listRootDirectory() {
        DEBUG_PRINTLN("Archivos en raíz:");
        
        File root = SD.open("/");
        if (root) {
            int fileCounter = 0;
            while (true) {
                File entry = root.openNextFile();
                if (!entry) break;
                
                DEBUG_PRINTF("  %s (%s)\n", 
                            entry.name(), 
                            entry.isDirectory() ? "DIR" : String(entry.size()).c_str());
                
                fileCounter++;
                if (fileCounter >= 10) {
                    DEBUG_PRINTLN("  ... (más archivos)");
                    break;
                }
                
                entry.close();
            }
            root.close();
        }
    }

    // Verifica si un archivo tiene extensión WAV (insensible a mayúsculas/minúsculas)
    bool isWavFile(const char* filename) {
        size_t nameLen = strlen(filename);
        if (nameLen < 4) return false;
        
        // Convertir los últimos 4 caracteres a minúsculas para comparación
        char ext[5];
        for (int i = 0; i < 4; i++) {
            ext[i] = tolower(filename[nameLen - 4 + i]);
        }
        ext[4] = '\0';
        
        // Verificar si es .wav
        return (strcmp(ext, ".wav") == 0);
    }

    // Escanea archivos WAV en un directorio con soporte para recursión
    void scanDirectory(File dir, const char* path, uint8_t depth) {
        // Limitar profundidad de recursión
        if (depth > maxRecursionDepth) {
            DEBUG_PRINTF("Profundidad máxima alcanzada en: %s\n", path);
            return;
        }
        
        DEBUG_PRINTF("Escaneando directorio: %s (nivel %d)\n", path, depth);
        
        while (fileCount < MAX_FILES) {
            File entry = dir.openNextFile();
            if (!entry) {
                // No hay más archivos
                break;
            }
            
            // Construir ruta completa
            char fullPath[MAX_FILENAME_LENGTH];
            if (strcmp(path, "/") == 0) {
                snprintf(fullPath, MAX_FILENAME_LENGTH, "/%s", entry.name());
            } else {
                snprintf(fullPath, MAX_FILENAME_LENGTH, "%s/%s", path, entry.name());
            }
            
            if (entry.isDirectory()) {
                DEBUG_PRINTF("Encontrado directorio: %s\n", fullPath);
                // Escanear subdirectorio recursivamente
                scanDirectory(entry, fullPath, depth + 1);
            } else {
                // Verificar si es un archivo WAV (usando función mejorada)
                if (isWavFile(entry.name())) {
                    DEBUG_PRINTF("Verificando archivo WAV: %s\n", fullPath);
                    
                    // Verificar si el archivo es compatible
                    WavReader wavReader;
                    if (wavReader.open(fullPath)) {
                        // Archivo WAV compatible, añadir a la lista
                        strncpy(fileList[fileCount], fullPath, MAX_FILENAME_LENGTH - 1);
                        fileList[fileCount][MAX_FILENAME_LENGTH - 1] = '\0';
                        fileCount++;
                        
                        DEBUG_PRINTF("Archivo WAV compatible encontrado: %s\n", fullPath);
                        
                        // Mostrar información del archivo
                        const WavInfo& info = wavReader.getWavInfo();
                        DEBUG_PRINTF("  %dHz, %d bits, %d canales\n", 
                                    info.sampleRate, info.bitsPerSample, info.numChannels);
                    } else {
                        DEBUG_PRINTF("Archivo WAV incompatible: %s\n", fullPath);
                    }
                    
                    wavReader.close();
                    
                    if (fileCount >= MAX_FILES) {
                        DEBUG_PRINTLN("Límite de archivos alcanzado");
                        break;
                    }
                }
            }
            
            entry.close();
        }
    }
};

#endif // SD_MANAGER_H
