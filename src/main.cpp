#include <cstdio>
#include <string.h>
#include "stm32f1xx.h"   // CMSIS-Header für STM32F1


/// ------ Delay ------------------------------

volatile uint32_t systick_ms = 0;

// SysTick-Handler (wird alle 1 ms aufgerufen)
extern "C" void SysTick_Handler(void) {
    systick_ms++;
}

// Initialisierung des SysTick-Timers für 1 ms Takt
void SysTick_Init(void) {
    SysTick->LOAD = 8000 - 1; // 8 MHz / 8000 = 1 kHz = 1 ms
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

// Sichere Delay-Funktion (blockierend, aber interrupt-freundlich)
void delay(uint32_t ms) {
    uint32_t start = systick_ms;
    while ((systick_ms - start) < ms) {
        __NOP();
    }
}

/// -------------------------------------------------------

// UART2 (PA2=TX, PA3=RX) – dein bestehendes Setup
void UART2_Init(void)
{
    // 1) Clock für GPIOA + AFIO + USART2
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    // 2) PA2 = AF Push‑Pull, 50 MHz
    GPIOA->CRL &= ~(GPIO_CRL_MODE2 | GPIO_CRL_CNF2);
    GPIOA->CRL |= (0b11 << GPIO_CRL_MODE2_Pos) | (0b10 << GPIO_CRL_CNF2_Pos);
    // 3) PA3 = Floating Input
    GPIOA->CRL &= ~(GPIO_CRL_MODE3 | GPIO_CRL_CNF3);
    GPIOA->CRL |= (0b01 << GPIO_CRL_CNF3_Pos);
    // 4) Baudrate 115200 @ 8 MHz HSI (APB1 = 8 MHz)
    USART2->BRR = 8000000 / 115200; // ≈69
    // 5) TE + RE + UE
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

// Sende ein Zeichen über UART3
static inline void UART3_SendChar(char c)
{
    while (!(USART3->SR & USART_SR_TXE));
    USART3->DR = c;
}

// Lese ein Zeichen von UART2 (polling)
static inline char UART2_ReadChar(void)
{
    while (!(USART2->SR & USART_SR_RXNE));
    return (char)(USART2->DR & 0xFF);
}

const char* host = "api.weatherapi.com";
const char* path = "/v1/current.json?key=b27d29b85f7b43d9993215104252906&q=Retz&aqi=no";

int main(void) {
    SysTick_Init();
    UART2_Init();

    // 1. TCP-Verbindung aufbauen
    sendAT("AT+CIPSTART=\"TCP\",\"api.weatherapi.com\",80\r\n");
    delay(2000); // Warte auf CONNECT & OK

    // 2. Länge des HTTP-Requests berechnen
    char httpRequest[256];
    sprintf(httpRequest,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n", path, host);

    int len = strlen(httpRequest);

    // 3. CIPSEND mit korrekter Länge senden
    char cipsendCmd[30];
    sprintf(cipsendCmd, "AT+CIPSEND=%d\r\n", len);
    sendAT(cipsendCmd);

    // 4. Auf '>' warten (muss implementiert werden, hier Delay als Platzhalter)
    delay(500);

    // 5. HTTP-Request senden
    sendAT(httpRequest);

    // 6. Antwort vom Server lesen und parsen (muss noch implementiert werden)
}


