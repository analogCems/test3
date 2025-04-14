/**
 * Config.h
 * 
 * Configuración para reproductor de WAV en ESP32-C3 XIAO con DAC8551
 * Basado en el proyecto Radio Music de Tom Whitwell, adaptado para ESP32-C3
 * 
 * Fecha: 2025-04-14
 * Actualizado para Arduino core 3.0
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Configuración de hardware
#define CPU_FREQ_MHZ        80      // ESP32-C3 funciona a 80MHz

// Configuración de pines SPI
#define SPI_MOSI_PIN        10      // Pin MOSI compartido
#define SPI_MISO_PIN        9       // Pin MISO compartido
#define SPI_SCK_PIN         8       // Pin SCK compartido
#define SD_CS_PIN           7       // Chip Select para SD
#define DAC_CS_PIN          20      // Chip Select para DAC8551

// Configuración de velocidad SPI
#define SPI_FREQ_SD         4000000  // 4MHz para SD (conservador para estabilidad)
#define SPI_FREQ_DAC        20000000 // 20MHz para DAC8551 (máximo teórico 30MHz)

// Configuración de audio
#define SAMPLE_RATE         44100   // Frecuencia de muestreo estándar
#define BITS_PER_SAMPLE     16      // Profundidad de bits
#define CHANNELS            1       // Mono

// Configuración de buffer
#define BUFFER_SIZE         2048    // 2KB buffer inicial (1024 muestras de 16 bits)
#define SD_BLOCK_SIZE       512     // Tamaño de bloque para lectura de SD
#define BUFFER_LOW_THRESHOLD (BUFFER_SIZE / 4)  // Umbral para considerar buffer bajo

// Configuración de WAV
#define WAV_HEADER_SIZE     44      // Tamaño de cabecera WAV estándar
#define MAX_FILENAME_LENGTH 64      // Longitud máxima de nombre de archivo

// Configuración de sistema
#define MAX_FILES           32      // Máximo número de archivos a escanear
#define DEBUG_SERIAL        1       // 1 para habilitar debug por Serial, 0 para deshabilitar

// Macros de debug
#if DEBUG_SERIAL
  #define DEBUG_BEGIN(baud) Serial.begin(baud); while(!Serial) {}
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_BEGIN(baud)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// Comandos DAC8551
#define DAC8551_CMD_WRITE_UPDATE 0x10  // Comando para escribir y actualizar DAC

// Constantes de tiempo para Arduino core 3.0
// Configuración del timer para ESP32-C3 a 80MHz
// Usamos 1MHz como base para el timer (1 microsegundo de resolución)
#define TIMER_BASE_FREQ     1000000  // 1MHz
// Para 44.1kHz, necesitamos un evento cada ~22.67 microsegundos
#define TIMER_ALARM_VALUE   23       // 1000000/44100 ≈ 22.67, redondeado a 23

// Función min segura para tipos diferentes
template <typename T, typename U>
inline T safe_min(T a, U b) {
    return (a < static_cast<T>(b)) ? a : static_cast<T>(b);
}

#endif // CONFIG_H
