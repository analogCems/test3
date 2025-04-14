/**
 * DAC8551.h
 * 
 * Implementación optimizada para comunicación con DAC8551 mediante SPI
 * en ESP32-C3 XIAO.
 * 
 * Fecha: 2025-04-14
 * Actualizado para Arduino core 3.0
 */

#ifndef DAC8551_H
#define DAC8551_H

#include "Config.h"
#include <Arduino.h>
#include <SPI.h>

class DAC8551 {
private:
    uint8_t csPin;         // Pin CS (Chip Select)
    SPISettings spiSettings;  // Configuración SPI

public:
    DAC8551(uint8_t cs = DAC_CS_PIN) : 
        csPin(cs),
        // DAC8551 soporta SPI_MODE0 (CPOL=0, CPHA=0) o SPI_MODE1 (CPOL=0, CPHA=1)
        // Según datasheet, el dato se captura en flanco ascendente (CPHA=0)
        spiSettings(SPI_FREQ_DAC, MSBFIRST, SPI_MODE0) {
    }

    // Inicializa el DAC
    void begin() {
        pinMode(csPin, OUTPUT);
        digitalWrite(csPin, HIGH);  // CS inactivo
        
        // Prueba inicial para verificar comunicación
        setSilence();
        
        DEBUG_PRINTLN("DAC8551 inicializado");
    }

    // Escribe un valor de 16 bits al DAC
    // Optimizado para velocidad en la ISR
    inline void IRAM_ATTR write(uint16_t value) {
        SPI.beginTransaction(spiSettings);
        
        // Tiempo crítico - minimizar ciclos entre estos pasos
        digitalWrite(csPin, LOW);
        SPI.transfer(DAC8551_CMD_WRITE_UPDATE);  // Comando: Write and Update
        SPI.transfer(value >> 8);                // MSB
        SPI.transfer(value & 0xFF);              // LSB
        digitalWrite(csPin, HIGH);
        
        SPI.endTransaction();
    }

    // Escribe un valor de 16 bits al DAC con offset
    // Convierte de -32768..32767 a 0..65535
    inline void IRAM_ATTR writeWithOffset(int16_t value) {
        write((uint16_t)(value + 32768));
    }

    // Establece la salida en el valor medio (silencio para audio)
    void setSilence() {
        write(32768);  // Mitad del rango (0V)
    }

    // Prueba el DAC con una rampa
    void testRamp() {
        DEBUG_PRINTLN("Prueba de DAC: rampa");
        
        for (uint16_t i = 0; i < 65535; i += 256) {
            write(i);
            delayMicroseconds(100);
        }
        
        setSilence();
    }

    // Prueba el DAC con una onda sinusoidal
    void testSine(uint16_t cycles = 5) {
        DEBUG_PRINTLN("Prueba de DAC: onda sinusoidal");
        
        const int samples = 100;
        for (uint16_t c = 0; c < cycles; c++) {
            for (int i = 0; i < samples; i++) {
                float angle = (2.0 * PI * i) / samples;
                int16_t value = 32767 * sin(angle);
                writeWithOffset(value);
                delayMicroseconds(100);
            }
        }
        
        setSilence();
    }
};

#endif // DAC8551_H
