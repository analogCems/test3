/**
 * WavReader.h
 * 
 * Implementación optimizada para lectura y procesamiento de archivos WAV
 * en ESP32-C3 XIAO.
 * 
 * Fecha: 2025-04-14
 * Actualizado para Arduino core 3.0
 */

#ifndef WAV_READER_H
#define WAV_READER_H

#include "Config.h"
#include <Arduino.h>
#include <SD.h>

// Estructura para almacenar información de archivos WAV
struct WavInfo {
    uint32_t sampleRate;       // Frecuencia de muestreo
    uint16_t numChannels;      // Número de canales
    uint16_t bitsPerSample;    // Bits por muestra
    uint32_t dataSize;         // Tamaño de los datos de audio
    uint32_t dataOffset;       // Offset a los datos de audio
    uint32_t bytesPerSample;   // Bytes por muestra (calculado)
    
    // Constructor por defecto
    WavInfo() : 
        sampleRate(0), 
        numChannels(0), 
        bitsPerSample(0), 
        dataSize(0), 
        dataOffset(0), 
        bytesPerSample(0) {
    }
    
    // Calcula bytes por muestra
    void calculateBytesPerSample() {
        bytesPerSample = (bitsPerSample * numChannels) / 8;
    }
    
    // Verifica si el formato es compatible
    bool isCompatible() const {
        // Verificar si es compatible con nuestras configuraciones
        // Permitimos cualquier frecuencia de muestreo para mayor flexibilidad
        return (bitsPerSample == BITS_PER_SAMPLE && 
                (numChannels == 1 || numChannels == 2));
    }
    
    // Imprime información del archivo WAV
    void printInfo() {
        DEBUG_PRINTLN("Información del archivo WAV:");
        DEBUG_PRINTF("  Frecuencia de muestreo: %d Hz\n", sampleRate);
        DEBUG_PRINTF("  Canales: %d\n", numChannels);
        DEBUG_PRINTF("  Bits por muestra: %d\n", bitsPerSample);
        DEBUG_PRINTF("  Bytes por muestra: %d\n", bytesPerSample);
        DEBUG_PRINTF("  Tamaño de datos: %d bytes\n", dataSize);
        DEBUG_PRINTF("  Offset de datos: %d\n", dataOffset);
        DEBUG_PRINTF("  Compatible: %s\n", isCompatible() ? "Sí" : "No");
    }
};

class WavReader {
private:
    File wavFile;              // Archivo WAV actual
    WavInfo wavInfo;           // Información del archivo WAV
    bool fileOpen;             // Indica si hay un archivo abierto
    char fileName[MAX_FILENAME_LENGTH]; // Nombre del archivo actual

public:
    WavReader() : fileOpen(false) {
        fileName[0] = '\0';
    }
    
    ~WavReader() {
        close();
    }
    
    // Abre un archivo WAV y lee su cabecera
    bool open(const char* path) {
        // Cerrar archivo anterior si está abierto
        if (fileOpen) {
            close();
        }
        
        DEBUG_PRINTF("Intentando abrir archivo: %s\n", path);
        
        // Abrir nuevo archivo
        wavFile = SD.open(path);
        if (!wavFile) {
            DEBUG_PRINTF("Error: No se pudo abrir el archivo %s\n", path);
            return false;
        }
        
        DEBUG_PRINTF("Archivo abierto, tamaño: %d bytes\n", wavFile.size());
        
        // Leer y verificar cabecera WAV
        if (!readHeader()) {
            DEBUG_PRINTLN("Error: Formato de archivo WAV inválido");
            wavFile.close();
            return false;
        }
        
        // Verificar compatibilidad
        if (!wavInfo.isCompatible()) {
            DEBUG_PRINTLN("Error: Formato de audio no compatible");
            wavInfo.printInfo();
            wavFile.close();
            return false;
        }
        
        // Guardar nombre de archivo
        strncpy(fileName, path, MAX_FILENAME_LENGTH - 1);
        fileName[MAX_FILENAME_LENGTH - 1] = '\0';
        
        // Posicionar al inicio de los datos
        wavFile.seek(wavInfo.dataOffset);
        fileOpen = true;
        
        DEBUG_PRINTF("Archivo WAV abierto: %s\n", fileName);
        wavInfo.printInfo();
        
        return true;
    }
    
    // Cierra el archivo WAV
    void close() {
        if (fileOpen && wavFile) {
            wavFile.close();
            fileOpen = false;
            DEBUG_PRINTLN("Archivo WAV cerrado");
        }
    }
    
    // Lee un bloque de datos del archivo WAV
    // Retorna el número de bytes leídos
    uint16_t readBlock(uint8_t* buffer, uint16_t size) {
        if (!fileOpen || !wavFile || !buffer) {
            return 0;
        }
        
        uint16_t bytesRead = wavFile.read(buffer, size);
        DEBUG_PRINTF("Leídos %d bytes del archivo WAV\n", bytesRead);
        return bytesRead;
    }
    
    // Salta a una posición específica en los datos de audio
    // position es en bytes desde el inicio de los datos
    bool seekToPosition(uint32_t position) {
        if (!fileOpen || !wavFile) {
            return false;
        }
        
        // Asegurar que la posición esté dentro de los límites
        if (position > wavInfo.dataSize) {
            position = wavInfo.dataSize;
        }
        
        // Alinear a límite de muestra
        if (position % wavInfo.bytesPerSample != 0) {
            position -= (position % wavInfo.bytesPerSample);
        }
        
        // Posicionar en el archivo
        return wavFile.seek(wavInfo.dataOffset + position);
    }
    
    // Verifica si se ha llegado al final del archivo
    bool isEndOfFile() const {
        return !fileOpen || !wavFile || wavFile.position() >= (wavInfo.dataOffset + wavInfo.dataSize);
    }
    
    // Obtiene la información del archivo WAV
    const WavInfo& getWavInfo() const {
        return wavInfo;
    }
    
    // Obtiene el nombre del archivo actual
    const char* getFileName() const {
        return fileName;
    }
    
private:
    // Imprime los primeros bytes de un archivo para diagnóstico
    void dumpFileHeader() {
        if (!wavFile) return;
        
        uint32_t originalPos = wavFile.position();
        wavFile.seek(0);
        
        DEBUG_PRINTLN("Primeros 64 bytes del archivo:");
        for (int i = 0; i < 64; i += 16) {
            uint8_t buffer[16];
            int bytesRead = wavFile.read(buffer, 16);
            
            if (bytesRead <= 0) break;
            
            String hexLine = "";
            String asciiLine = "  ";
            
            for (int j = 0; j < bytesRead; j++) {
                char hex[4];
                sprintf(hex, "%02X ", buffer[j]);
                hexLine += hex;
                
                if (buffer[j] >= 32 && buffer[j] <= 126) {
                    asciiLine += (char)buffer[j];
                } else {
                    asciiLine += ".";
                }
            }
            
            DEBUG_PRINTF("%04X: %s%s\n", i, hexLine.c_str(), asciiLine.c_str());
        }
        
        wavFile.seek(originalPos);
    }
    
    // Lee y procesa la cabecera WAV
    bool readHeader() {
        if (!wavFile) {
            return false;
        }
        
        // Volver al inicio del archivo
        wavFile.seek(0);
        
        // Imprimir los primeros bytes para diagnóstico
        dumpFileHeader();
        
        // Leer cabecera WAV (44 bytes estándar)
        uint8_t header[WAV_HEADER_SIZE];
        if (wavFile.read(header, WAV_HEADER_SIZE) != WAV_HEADER_SIZE) {
            DEBUG_PRINTLN("Error: No se pudo leer la cabecera WAV completa");
            return false;
        }
        
        // Verificar firma RIFF/WAVE
        if (header[0] != 'R' || header[1] != 'I' || header[2] != 'F' || header[3] != 'F' ||
            header[8] != 'W' || header[9] != 'A' || header[10] != 'V' || header[11] != 'E') {
            DEBUG_PRINTLN("Error: Firma RIFF/WAVE no encontrada");
            DEBUG_PRINTF("Primeros 12 bytes: %c%c%c%c....%c%c%c%c\n", 
                        header[0], header[1], header[2], header[3],
                        header[8], header[9], header[10], header[11]);
            return false;
        }
        
        // Extraer información del formato
        wavInfo.numChannels = header[22] | (header[23] << 8);
        wavInfo.sampleRate = header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24);
        wavInfo.bitsPerSample = header[34] | (header[35] << 8);
        
        DEBUG_PRINTLN("Información básica extraída:");
        DEBUG_PRINTF("  Canales: %d\n", wavInfo.numChannels);
        DEBUG_PRINTF("  Frecuencia: %d Hz\n", wavInfo.sampleRate);
        DEBUG_PRINTF("  Bits por muestra: %d\n", wavInfo.bitsPerSample);
        
        // Buscar el chunk 'data'
        uint32_t pos = 12;  // Después de la cabecera RIFF/WAVE
        bool dataChunkFound = false;
        
        while (pos < 2000) {  // Límite para evitar bucles infinitos
            // Leer ID y tamaño del chunk
            uint8_t chunkHeader[8];
            wavFile.seek(pos);
            if (wavFile.read(chunkHeader, 8) != 8) {
                DEBUG_PRINTF("Error: No se pudo leer la cabecera del chunk en posición %d\n", pos);
                break;
            }
            
            // Tamaño del chunk
            uint32_t chunkSize = chunkHeader[4] | (chunkHeader[5] << 8) | 
                                (chunkHeader[6] << 16) | (chunkHeader[7] << 24);
            
            DEBUG_PRINTF("Chunk encontrado en pos %d: %c%c%c%c, tamaño: %d\n", 
                        pos, chunkHeader[0], chunkHeader[1], chunkHeader[2], chunkHeader[3], chunkSize);
            
            // Verificar si es el chunk 'data'
            if (chunkHeader[0] == 'd' && chunkHeader[1] == 'a' && 
                chunkHeader[2] == 't' && chunkHeader[3] == 'a') {
                wavInfo.dataOffset = pos + 8;  // Después de la cabecera del chunk
                wavInfo.dataSize = chunkSize;
                dataChunkFound = true;
                DEBUG_PRINTLN("Chunk 'data' encontrado");
                break;
            }
            
            // Avanzar al siguiente chunk
            pos += 8 + chunkSize;
        }
        
        if (!dataChunkFound) {
            DEBUG_PRINTLN("Error: Chunk 'data' no encontrado");
            return false;
        }
        
        // Calcular bytes por muestra
        wavInfo.calculateBytesPerSample();
        
        return true;
    }
};

#endif // WAV_READER_H
