#include "stm32f1xx.h"
#include <cstdio>
#include <cstring>

#include "Cortex_STM32F103.h"
#include "stm32f1xx_hal.h"

UART_HandleTypeDef huart1;

#define RX_LEN 100
uint8_t rxBuffer[RX_LEN];


void sendCommand(const char* cmd)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)cmd, strlen(cmd), HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // HSI einschalten
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    // PLL als System Clock
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}


void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // PA9 = TX, PA10 = RX
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = AIN2;  // TX
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = AIN3; // RX
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&huart1);
}



const char* receive()
{
    memset(rxBuffer, 0, sizeof(rxBuffer));
    HAL_UART_Receive(&huart1, rxBuffer, sizeof(rxBuffer), HAL_MAX_DELAY);
    return (const char*)rxBuffer;
}


static inline void ITM_SendChar(uint8_t ch)
{
    while (!(ITM->PORT[0].u32 & 1));
    ITM->PORT[0].u8 = ch;
}

void ITM_SendString(const char* str)
{
    while (*str) ITM_SendChar((uint8_t)*str++);
}

int main(void)
{
    HAL_Init();             // HAL initialisieren
    SystemClock_Config();   // Systemtakt setzen (je nach Projekt)

    MX_USART1_UART_Init();  // UART1 konfigurieren

    while (true) {
        sendCommand("AT");
        HAL_Delay(500);         // Kurze Wartezeit

        const char* response = receive();

        ITM_SendString(response);

        // Optional: hier debuggen
        snprintf((char*)rxBuffer, RX_LEN, "%s", response);
    }

}

