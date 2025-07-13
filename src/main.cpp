#include <cstdio>
#include <cstring>
#include <string>
#include "stm32f1xx.h"
#include <vector>

#define RX_BUF_SIZE 2048

volatile char rx_buffer[RX_BUF_SIZE];
volatile uint16_t rx_head = 0;
volatile uint16_t rx_tail = 0;
volatile uint32_t systick_ms = 0;

void extractData(char*);
void requestTime();

extern "C" void SysTick_Handler(void)
{
    systick_ms++;
}

void SysTick_Init()
{
    // 8 MHz HSI → 1 kHz SysTick
    SysTick->LOAD = 8000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk
                  | SysTick_CTRL_TICKINT_Msk
                  | SysTick_CTRL_ENABLE_Msk;
}

void delay(uint32_t ms)
{
    uint32_t start = systick_ms;

    while ((systick_ms - start) < ms)
    {
        __NOP();
    }
}

extern "C" void USART2_IRQHandler(void)
{
    if (USART2->SR & USART_SR_RXNE)
    {
        char c = static_cast<char>(USART2->DR);
        uint16_t next = (rx_head + 1) % RX_BUF_SIZE;

        if (next != rx_tail)
        {
            rx_buffer[rx_head] = c;
            rx_head = next;
        }
    }
}

bool uart2_available()
{
    return rx_head != rx_tail;
}

char uart2_read()
{
    if (!uart2_available()) return 0;
    char c = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;

    return c;
}

int uart2_read_all(char* dest, int maxlen)
{
    int i = 0;

    while (uart2_available() && i < (maxlen - 1))
    {
        dest[i++] = uart2_read();
    }

    dest[i] = '\0';

    return i;
}

void UART2_Init(uint32_t baud)
{
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

void sendU2(const char* str)
{
    while (*str)
    {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = *str++;
    }
}

void sendU3(const char* str)
{
    while (*str) {
        while (!(USART3->SR & USART_SR_TXE));
        USART3->DR = *str++;
    }
}

void UART3_Init(void)
{
    // 1) Clock für GPIOB + AFIO + USART3
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;

    // 2) PB10 = TX: AF Push-Pull, 50 MHz
    GPIOB->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
    GPIOB->CRH |= (0b11 << GPIO_CRH_MODE10_Pos) | (0b10 << GPIO_CRH_CNF10_Pos);

    // 3) PB11 = RX: Floating Input
    GPIOB->CRH &= ~(GPIO_CRH_MODE11 | GPIO_CRH_CNF11);
    GPIOB->CRH |= (0b01 << GPIO_CRH_CNF11_Pos);

    // 4) Baudrate 115200 @ 8 MHz APB1
    USART3->BRR = 8000000 / 115200; // ≈ 69

    // 5) TE + RE + UE
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

typedef struct
{
    uint8_t hour;
    uint8_t minute;
    uint8_t day;
    uint8_t month;
    uint8_t year;
} Time;

typedef struct
{
    // ------------ location ------------
    Time localTime;

    std::string name;
    std::string region;
    std::string country;

    // ------------ current ------------
    Time lastUpdated;
    float tempC;
    bool isDay;

    // ------------ condition ------------
    std::string weather;
} Data;

void requestWeatherData(char* dest)
{
    const char* host = "api.weatherapi.com";
    const char* path = "/v1/current.json?key=b27d29b85f7b43d9993215104252906&q=Retz&aqi=no";

    char httpReq[256];
    char cipsend[32];

    // --- 1) Vorherige Verbindung schließen ---
    sendU2("AT+CIPCLOSE\r\n");
    delay(500);
    uart2_read_all(dest, RX_BUF_SIZE); // Clear buffer

    // --- 2) NTP aktivieren (optional) ---
    sendU2("AT+CIPSNTPCFG=1,2,\"pool.ntp.org\"\r\n");
    delay(500);
    uart2_read_all(dest, RX_BUF_SIZE);

    // --- 3) TCP-Verbindung aufbauen ---
    sendU2("AT+CIPSTART=\"TCP\",\"api.weatherapi.com\",80\r\n");
    delay(500); // längeres Delay!
    uart2_read_all(dest, RX_BUF_SIZE);

    // --- 4) HTTP‑Request zusammenbauen ---
    sprintf(httpReq, "GET %s HTTP/1.1\r\n" "Host: %s\r\n" "Connection: close\r\n" "\r\n", path, host);
    int len = strlen(httpReq);

    // --- 5) CIPSEND senden ---
    sprintf(cipsend, "AT+CIPSEND=%d\r\n", len);
    sendU2(cipsend);
    delay(500); // auf '>' warten
    uart2_read_all(dest, RX_BUF_SIZE); // Prompt abfangen

    // --- 6) HTTP‑Request senden ---
    sendU2(httpReq);

    // --- 7) Antwort lesen ---
    delay(500); // auf komplette Antwort warten
    uart2_read_all(dest, RX_BUF_SIZE);
}

char* extract_first_json_object(const char* input, char* outBuf, int bufSize)
{
    const char* p = strchr(input, '{');
    if (!p) return NULL;

    int depth = 0;
    int len = 0;

    while (*p && len < bufSize - 1)
    {
        if (*p == '{') depth++;
        if (*p == '}') depth--;

        outBuf[len++] = *p++;

        if (depth == 0) break;
    }

    outBuf[len] = '\0';
    return (depth == 0) ? outBuf : NULL;
}

char* extractAllJSON(char* src, size_t length)
{
    sendU2("Anfang:\r\n");
    sendU3(src);
    sendU2("Ende:\r\n");

    char result[length];
    int startPos = 0;

    for (size_t i = 0; i < length; ++i)
    {
        if (src[i] == '{')
        {
            startPos = i;
        }

        else if (src[i] == '}')
        {
            for (int j = startPos; j <= i; j++)
            {
                strcat(src, result);
            }

            result[i - startPos + 1] = '\0';
        }
    }

    if (strlen(result) <= 0)
    {
        sendU2("Kein JSON-Objekt gefunden.\r\n");
    }

    return result;
}

void extractData(std::string content)
{
    Data data;
    std::string temp;

    if (content.length() <= 0)
    {
        sendU2("Kein Inhalt zum Extrahieren.\r\n");
        return;
    }

    std::vector <std::string> types = {
        "location",
        "localtime",
        "name",
        "region",
        "country",
        "last_updated",
        "temp_c",
        "is_day",
        "condition"
    };

    for (int i = 0; i < types.size(); i++)
    {
        i = content.find(types[i]) + types[i].length() + 3;

    while (content[i] != '"')
    {
        temp = temp + content[i];
        i++;
    }
}








}

int main()
{
    SysTick_Init();
    UART2_Init(115200);
    char UART2Response[RX_BUF_SIZE];

    requestWeatherData(UART2Response);
    delay(100);

    char* jsonStart = strstr(UART2Response, "\r\n\r\n");

    jsonStart += 4;


    char* extracted = extract_first_json_object(jsonStart, UART2Response, RX_BUF_SIZE);
    sendU2("Extrahiertes JSON:\r\n");

    delay(100);

    //extractData(extracted);
    sendU2(extracted);
    sendU2("Ende:\r\n");
}