#include <cstdio>
#include <cstring>
#include <string>

#include "stm32f1xx.h"
#include "jsmn.h"

/// ------------ SysTick / Delay -----------------

volatile uint32_t systick_ms = 0;

extern "C" void SysTick_Handler(void) {
    systick_ms++;
}

void SysTick_Init() {
    // 8 MHz HSI → 1 kHz SysTick
    SysTick->LOAD = 8000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk
                  | SysTick_CTRL_TICKINT_Msk
                  | SysTick_CTRL_ENABLE_Msk;
}

void delay(uint32_t ms) {
    uint32_t start = systick_ms;
    while ((systick_ms - start) < ms) {
        __NOP();
    }
}

/// ------------ UART2 RX‑Ringpuffer -------------

#define RX_BUF_SIZE 2048

volatile char    rx_buffer[RX_BUF_SIZE];
volatile uint16_t rx_head = 0;
volatile uint16_t rx_tail = 0;

extern "C" void USART2_IRQHandler(void) {
    if (USART2->SR & USART_SR_RXNE) {
        char c = static_cast<char>(USART2->DR);
        uint16_t next = (rx_head + 1) % RX_BUF_SIZE;
        if (next != rx_tail) {
            rx_buffer[rx_head] = c;
            rx_head = next;
        }
        // else Überlauf: Zeichen verwerfen
    }
}

bool uart2_available() {
    return rx_head != rx_tail;
}

// Liest genau **ein** Zeichen (oder 0, wenn leer)
char uart2_read() {
    if (!uart2_available()) return 0;
    char c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
    return c;
}

// Liest **alle** verfügbaren Zeichen in dest[], terminiert mit '\0'
int uart2_read_all(char* dest, int maxlen) {
    int i = 0;
    while (uart2_available() && i < (maxlen - 1)) {
        dest[i++] = uart2_read();
    }
    dest[i] = '\0';
    return i;
}

/// ------------ UART2 init & TX -----------------

void UART2_Init(uint32_t baud) {
    // Clocks
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // PA2 TX (AF‑PP 50 MHz)
    GPIOA->CRL &= ~(GPIO_CRL_MODE2 | GPIO_CRL_CNF2);
    GPIOA->CRL |=  (0b11 << GPIO_CRL_MODE2_Pos)
                 | (0b10 << GPIO_CRL_CNF2_Pos);
    // PA3 RX (Floating Input)
    GPIOA->CRL &= ~(GPIO_CRL_MODE3 | GPIO_CRL_CNF3);
    GPIOA->CRL |=  (0b01 << GPIO_CRL_CNF3_Pos);

    // Baudrate
    USART2->BRR = SystemCoreClock / baud;
    // TE + RE + UE
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    // RXNE‑Interrupt
    USART2->CR1 |= USART_CR1_RXNEIE;

    NVIC_EnableIRQ(USART2_IRQn);
}

void sendU2(const char* str) {
    while (*str) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = *str++;
    }
}

typedef struct
{
    std::string name;
    std::string region;


} WeatherData;

// Findet und kopiert das JSON-Objekt für ein Schlüssel "key" aus jsonStr
// Beispiel: key="location" -> sucht "location":{...}
// outBuf: Zielpuffer, bufSize: Puffergröße
// Return: pointer auf outBuf oder NULL bei Fehler

int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
    return (tok->type == JSMN_STRING &&
            (int)strlen(s) == tok->end - tok->start &&
            strncmp(json + tok->start, s, tok->end - tok->start) == 0) ? 0 : -1;
}


char* extract_named_json_object(const char* jsonStr, const char* key, char* outBuf, int bufSize) {
    char pattern[64];
    sprintf(pattern, "\"%s\":{", key);

    const char* keyPos = strstr(jsonStr, pattern);
    if (!keyPos) return NULL;

    const char* objStart = strchr(keyPos, '{');
    if (!objStart) return NULL;

    // Finde das passende schließende '}' (mit korrektem Matching)
    int depth = 0;
    const char* ptr = objStart;
    while (*ptr) {
        if (*ptr == '{') depth++;
        else if (*ptr == '}') depth--;

        if (depth == 0) break;
        ptr++;
    }

    if (depth != 0) return NULL; // kein vollständiges Objekt gefunden

    int len = ptr - objStart + 1;
    if (len >= bufSize) return NULL;

    memcpy(outBuf, objStart, len);
    outBuf[len] = '\0';

    return outBuf;
}

/// ------------ Main & Hintergrund‑Logging --------

void requestWeatherData()
{
    const char* host = "api.weatherapi.com";
    const char* path = "/v1/current.json?key=b27d29b85f7b43d9993215104252906&q=Retz&aqi=no";

    char  httpReq[256];
    char  cipsend[32];


    // --- 1) NTP (optional) ---
    sendU2("AT+CIPSNTPCFG=1,2,\"pool.ntp.org\"\r\n");
    delay(2000);

    // --- 2) TCP aufbauen ---
    sendU2("AT+CIPSTART=\"TCP\",\"api.weatherapi.com\",80\r\n");
    delay(2000);

    // --- 3) HTTP‑Request bauen ---
    sprintf(httpReq,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host);
    int len = strlen(httpReq);

    // --- 4) CIPSEND senden ---
    sprintf(cipsend, "AT+CIPSEND=%d\r\n", len);
    sendU2(cipsend);

    // Kleines Delay, damit ESP das '>' prompt ausgeben kann
    delay(500);

    // --- 5) HTTP senden ---
    sendU2(httpReq);
}

int main() {
    SysTick_Init();
    UART2_Init(115200);          // oder 9600, je nach ESP‑Setup
    char  fullResponse[RX_BUF_SIZE];

    // Pufferpositionen (Head/Tail) bleiben erhalten,
    // wir loggen **immer** alles mit.

    requestWeatherData();

    // --- 6) Warte, bis alles eingetroffen ist ---
    // (z. B. 5 s warten; passt je nach Datenmenge an)
    delay(5000);

    // --- 7) Ringpuffer → Flat-Array kopieren ---
    uart2_read_all(fullResponse, sizeof(fullResponse));

    char* objStart = strchr(fullResponse, '{');

    if (!objStart)
    {
        sendU2("Keine JSON Antwort gefunden");
        while (1);
    }

    objStart += 4; // hinter Header

    char jsonOnly[1024];
    strncpy(jsonOnly, objStart, sizeof(jsonOnly) - 1);
    jsonOnly[sizeof(jsonOnly) - 1] = '\0';

    jsmn_parser parser;
    jsmntok_t tokens[256];
    jsmn_init(&parser);

    int token_count = jsmn_parse(&parser, jsonOnly, strlen(jsonOnly), tokens, 256);

    if (token_count < 0) {
        sendU2("Parse-Fehler\r\n");
        return 1;
    }

    for (int i = 1; i < token_count; i++) {
        if (jsoneq(jsonStart, &tokens[i], "current") == 0 && tokens[i+1].type == JSMN_OBJECT) {
            int start = tokens[i+1].start;
            int end   = tokens[i+1].end;
            int len   = end - start;

            char locationJson[512];
            if (len < sizeof(locationJson)) {
                strncpy(locationJson, jsonStart + start, len);
                locationJson[len] = '\0';
                sendU2("Location JSON:\r\n");
                sendU2(locationJson);
            } else {
                sendU2("location JSON zu groß\r\n");
            }
            break;
        }
    }



}
