/**
 * AudioBuffer.h
 * 
 * Implementación de buffer circular optimizado para reproducción de audio
 * en ESP32-C3 XIAO con memoria limitada.
 * 
 * Fecha: 2025-04-14
 * Actualizado para Arduino core 3.0
 */

#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include "Config.h"
#include <Arduino.h>

class AudioBuffer {
private:
    uint8_t* buffer;                // Puntero al buffer de datos
    volatile uint16_t readIndex;    // Índice de lectura
    volatile uint16_t writeIndex;   // Índice de escritura
    volatile uint16_t available;    // Bytes disponibles para lectura
    uint16_t size;                  // Tamaño total del buffer
    bool initialized;               // Flag de inicialización
    portMUX_TYPE bufferMutex;       // Mutex para protección de acceso concurrente

public:
    AudioBuffer() : 
        buffer(nullptr), 
        readIndex(0), 
        writeIndex(0), 
        available(0), 
        size(0), 
        initialized(false),
        bufferMutex(portMUX_INITIALIZER_UNLOCKED) {}

    ~AudioBuffer() {
        if (buffer) {
            free(buffer);
            buffer = nullptr;
        }
    }

    // Inicializa el buffer con el tamaño especificado
    bool init(uint16_t bufferSize) {
        if (initialized) {
            return true;  // Ya inicializado
        }

        buffer = (uint8_t*)malloc(bufferSize);
        if (!buffer) {
            DEBUG_PRINTLN("Error: No se pudo asignar memoria para el buffer");
            return false;
        }

        size = bufferSize;
        readIndex = 0;
        writeIndex = 0;
        available = 0;
        initialized = true;
        
        // Inicializar buffer con silencio (valor medio)
        memset(buffer, 0, size);
        
        DEBUG_PRINTF("Buffer inicializado: %d bytes\n", size);
        return true;
    }

    // Escribe datos en el buffer
    // Retorna el número de bytes escritos
    uint16_t write(const uint8_t* data, uint16_t length) {
        if (!initialized || !data || length == 0) {
            return 0;
        }

        portENTER_CRITICAL(&bufferMutex);
        
        // Limitar a espacio disponible
        uint16_t freeSpace = size - available;
        uint16_t bytesToWrite = (length > freeSpace) ? freeSpace : length;
        
        if (bytesToWrite == 0) {
            portEXIT_CRITICAL(&bufferMutex);
            return 0;  // Buffer lleno
        }

        // Escribir datos en el buffer circular
        uint16_t firstChunk = (writeIndex + bytesToWrite <= size) ? 
                              bytesToWrite : (size - writeIndex);
        uint16_t secondChunk = bytesToWrite - firstChunk;
        
        // Copiar primer bloque (hasta el final del buffer)
        memcpy(buffer + writeIndex, data, firstChunk);
        
        // Si es necesario, copiar segundo bloque (desde el inicio del buffer)
        if (secondChunk > 0) {
            memcpy(buffer, data + firstChunk, secondChunk);
        }
        
        // Actualizar índice de escritura
        writeIndex = (writeIndex + bytesToWrite) % size;
        
        // Actualizar bytes disponibles
        available += bytesToWrite;
        
        portEXIT_CRITICAL(&bufferMutex);
        return bytesToWrite;
    }

    // Lee datos del buffer
    // Retorna el número de bytes leídos
    uint16_t read(uint8_t* data, uint16_t length) {
        if (!initialized || !data || length == 0) {
            return 0;
        }

        portENTER_CRITICAL(&bufferMutex);
        
        if (available == 0) {
            portEXIT_CRITICAL(&bufferMutex);
            return 0;  // Buffer vacío
        }

        // Limitar a datos disponibles
        uint16_t bytesToRead = (length > available) ? available : length;

        // Leer datos del buffer circular
        uint16_t firstChunk = (readIndex + bytesToRead <= size) ? 
                             bytesToRead : (size - readIndex);
        uint16_t secondChunk = bytesToRead - firstChunk;
        
        // Copiar primer bloque (hasta el final del buffer)
        memcpy(data, buffer + readIndex, firstChunk);
        
        // Si es necesario, copiar segundo bloque (desde el inicio del buffer)
        if (secondChunk > 0) {
            memcpy(data + firstChunk, buffer, secondChunk);
        }
        
        // Actualizar índice de lectura
        readIndex = (readIndex + bytesToRead) % size;
        
        // Actualizar bytes disponibles
        available -= bytesToRead;
        
        portEXIT_CRITICAL(&bufferMutex);
        return bytesToRead;
    }

    // Lee un sample de 16 bits del buffer (optimizado para ISR)
    // Retorna true si se pudo leer, false si no hay datos suficientes
    bool readSample(int16_t& sample) {
        if (!initialized) {
            return false;
        }

        portENTER_CRITICAL_ISR(&bufferMutex);
        
        if (available < 2) {
            portEXIT_CRITICAL_ISR(&bufferMutex);
            return false;  // No hay suficientes datos
        }

        // Leer sample de 16 bits (little endian)
        uint8_t low = buffer[readIndex];
        uint8_t high = buffer[(readIndex + 1) % size];
        
        // Actualizar índice de lectura
        readIndex = (readIndex + 2) % size;
        
        // Actualizar bytes disponibles
        available -= 2;
        
        portEXIT_CRITICAL_ISR(&bufferMutex);
        
        // Formar sample de 16 bits
        sample = (high << 8) | low;
        
        return true;
    }

    // Retorna el espacio libre en el buffer
    uint16_t getFreeSpace() const {
        portENTER_CRITICAL((portMUX_TYPE*)&bufferMutex);
        uint16_t freeSpace = size - available;
        portEXIT_CRITICAL((portMUX_TYPE*)&bufferMutex);
        return freeSpace;
    }

    // Retorna los bytes disponibles para lectura
    uint16_t getAvailable() const {
        portENTER_CRITICAL((portMUX_TYPE*)&bufferMutex);
        uint16_t availableBytes = available;
        portEXIT_CRITICAL((portMUX_TYPE*)&bufferMutex);
        return availableBytes;
    }

    // Retorna el porcentaje de llenado del buffer (0-100)
    uint8_t getPercentFull() const {
        portENTER_CRITICAL((portMUX_TYPE*)&bufferMutex);
        uint8_t percent = (available * 100) / size;
        portEXIT_CRITICAL((portMUX_TYPE*)&bufferMutex);
        return percent;
    }

    // Verifica si el buffer está vacío
    bool isEmpty() const {
        portENTER_CRITICAL((portMUX_TYPE*)&bufferMutex);
        bool empty = (available == 0);
        portEXIT_CRITICAL((portMUX_TYPE*)&bufferMutex);
        return empty;
    }

    // Verifica si el buffer está lleno
    bool isFull() const {
        portENTER_CRITICAL((portMUX_TYPE*)&bufferMutex);
        bool full = (available == size);
        portEXIT_CRITICAL((portMUX_TYPE*)&bufferMutex);
        return full;
    }

    // Verifica si el buffer está por debajo del umbral bajo
    bool isLow() const {
        portENTER_CRITICAL((portMUX_TYPE*)&bufferMutex);
        bool low = (available < BUFFER_LOW_THRESHOLD);
        portEXIT_CRITICAL((portMUX_TYPE*)&bufferMutex);
        return low;
    }

    // Limpia el buffer
    void clear() {
        portENTER_CRITICAL(&bufferMutex);
        readIndex = 0;
        writeIndex = 0;
        available = 0;
        portEXIT_CRITICAL(&bufferMutex);
    }
};

#endif // AUDIO_BUFFER_H
