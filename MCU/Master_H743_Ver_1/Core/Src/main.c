/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

FDCAN_HandleTypeDef hfdcan1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
uint8_t RxDataUART[1];
char UartBuffer[128];
uint16_t UartBufferIndex = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  // --- Cấu hình FDCAN Sniffer ---
  // Nhận TẤT CẢ gói tin CAN vào RxFifo0 (Không dùng Filter)
  HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
  
  // Kích hoạt ngắt nhận FDCAN
  HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);

  // Khởi động giao diện FDCAN
  HAL_FDCAN_Start(&hfdcan1);
  
  // Khởi động ngắt nhận UART để giao tiếp với C#
  HAL_UART_Receive_IT(&huart1, RxDataUART, 1);

  // Cấu hình gói tin gửi đi cho K1 (Gửi cho F407)
  FDCAN_TxHeaderTypeDef TxHeader1;
  TxHeader1.Identifier = 0x010; 
  TxHeader1.IdType = FDCAN_STANDARD_ID;
  TxHeader1.TxFrameType = FDCAN_DATA_FRAME;
  TxHeader1.DataLength = FDCAN_DLC_BYTES_1;
  TxHeader1.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  TxHeader1.BitRateSwitch = FDCAN_BRS_OFF; // KHÔNG dùng FD, dùng Classic
  TxHeader1.FDFormat = FDCAN_CLASSIC_CAN;  // Ép về chuẩn CAN 2.0 để giao tiếp với F4
  TxHeader1.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  TxHeader1.MessageMarker = 0;

  // Cấu hình gói tin gửi đi cho K2 (Gửi cho F429)
  FDCAN_TxHeaderTypeDef TxHeader2 = TxHeader1;
  TxHeader2.Identifier = 0x020; 

  uint8_t TxData[1] = {0x01}; // Lệnh đổi trạng thái LED

  // Khai báo biến chống rung cho K1 và K2 (Mặc định bằng 1 vì đã Pull-Up)
  uint32_t last_press_time_K1 = 0;
  uint32_t last_press_time_K2 = 0;
  uint8_t last_btn_state_K1 = GPIO_PIN_SET; 
  uint8_t last_btn_state_K2 = GPIO_PIN_SET;
  uint8_t debounced_K1 = GPIO_PIN_SET;
  uint8_t debounced_K2 = GPIO_PIN_SET;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // --- XỬ LÝ NÚT K1 (PE3) ---
    uint8_t raw_K1 = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3);
    if (raw_K1 != last_btn_state_K1) {
      last_press_time_K1 = HAL_GetTick();
    }
    if ((HAL_GetTick() - last_press_time_K1) > 50) {
      if (raw_K1 != debounced_K1) {
        debounced_K1 = raw_K1;
        if (debounced_K1 == GPIO_PIN_RESET) { // Nhấn thì trạng thái bằng 0 (Pull-up)
          HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader1, TxData); // Gửi 0x010
        }
      }
    }
    last_btn_state_K1 = raw_K1;

    // --- XỬ LÝ NÚT K2 (PC5) ---
    uint8_t raw_K2 = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_5);
    if (raw_K2 != last_btn_state_K2) {
      last_press_time_K2 = HAL_GetTick();
    }
    if ((HAL_GetTick() - last_press_time_K2) > 50) {
      if (raw_K2 != debounced_K2) {
        debounced_K2 = raw_K2;
        if (debounced_K2 == GPIO_PIN_RESET) { // Nhấn thì trạng thái bằng 0
          HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader2, TxData); // Gửi 0x020
        }
      }
    }
    last_btn_state_K2 = raw_K2;

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 50;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */
  // === QUAN TRỌNG: Ép buộc RxFifo0 có 3 phần tử ===
  // CubeMX luôn sinh RxFifo0ElmtsNbr = 0 (bug/config sai).
  // Phải ghi đè ở đây TRƯỚC khi HAL_FDCAN_Init() được gọi.
  // Nếu không, FDCAN sẽ không có chỗ chứa gói tin nhận được!
  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = DISABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 10;
  hfdcan1.Init.NominalSyncJumpWidth = 1;
  hfdcan1.Init.NominalTimeSeg1 = 63;
  hfdcan1.Init.NominalTimeSeg2 = 16;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 1;
  hfdcan1.Init.DataTimeSeg1 = 1;
  hfdcan1.Init.DataTimeSeg2 = 1;
  hfdcan1.Init.MessageRAMOffset = 0;
  hfdcan1.Init.StdFiltersNbr = 0;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.RxFifo0ElmtsNbr = 0;
  hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxFifo1ElmtsNbr = 0;
  hfdcan1.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxBuffersNbr = 3;
  hfdcan1.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.TxEventsNbr = 0;
  hfdcan1.Init.TxBuffersNbr = 0;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 2;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_QUEUE_OPERATION;
  hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */
  // === V\u00c1 L\u1ed6I CUBEMX: RxFifo0ElmtsNbr = 0 ===
  // CubeMX \u0111\u1ec3 gi\u00e1 tr\u1ecb = 0, khi\u1ebfn FDCAN kh\u00f4ng th\u1ec3 nh\u1eadn b\u1ea5t k\u1ef3 g\u00f3i tin n\u00e0o.
  // \u00c9p bu\u1ed9c l\u1ea1i th\u00e0nh 3 v\u00e0 Init l\u1ea1i l\u1ea7n n\u1eefa.
  hfdcan1.Init.RxFifo0ElmtsNbr = 3;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END FDCAN1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : PE3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : PC5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* Code tay đã được dọn dẹp vì CubeMX đã tự động cấu hình phần cứng ở trên */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// Gửi chuỗi qua UART
void UartSendString(const char* str) {
    HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 100);
}

// Xử lý ngắt FDCAN: Chuyển dữ liệu lên PC
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != RESET)
    {
        FDCAN_RxHeaderTypeDef RxHeader;
        uint8_t RxData[8] = {0};
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
        {
            char msg[100];
            uint32_t id = RxHeader.Identifier;
            
            if (id == 0x11A) {
                uint16_t ch1 = ((uint16_t)RxData[0] << 8) | RxData[1];
                uint16_t ch2 = ((uint16_t)RxData[2] << 8) | RxData[3];
                uint16_t ch3 = ((uint16_t)RxData[4] << 8) | RxData[5];
                
                uint32_t v1 = (uint32_t)ch1 * 330 / 4095;
                uint32_t v2 = (uint32_t)ch2 * 330 / 4095;
                uint32_t v3 = (uint32_t)ch3 * 330 / 4095;
                
                sprintf(msg, "<ADC_3CH:%lu.%02lu,%lu.%02lu,%lu.%02lu>\r\n",
                        v1/100, v1%100, v2/100, v2%100, v3/100, v3%100);
            } else if (id == 0x112 && RxHeader.DataLength >= FDCAN_DLC_BYTES_6) {
                // Telemetry motor từ F429: RPM, Dir, Duty, Encoder
                int16_t rpm = (int16_t)((RxData[0] << 8) | RxData[1]);
                uint8_t dir = RxData[2];
                uint8_t duty = RxData[3];
                int16_t enc = (int16_t)((RxData[4] << 8) | RxData[5]);
                
                sprintf(msg, "<MOTOR_SPD:%d,%u,%u,%d>\r\n", rpm, dir, duty, enc);
            } else if (id == 0x113) {
                // PID Telemetry từ F429: Target, Actual, Error, Output
                int16_t target = (int16_t)((RxData[0] << 8) | RxData[1]);
                int16_t actual = (int16_t)((RxData[2] << 8) | RxData[3]);
                int16_t error  = (int16_t)((RxData[4] << 8) | RxData[5]);
                int8_t  output = (int8_t)RxData[6];
                
                sprintf(msg, "<PID_DATA:%d,%d,%d,%d>\r\n", target, actual, error, output);
            } else {
                sprintf(msg, "[CAN_RX] ID: 0x%03lX, DATA: %02X\r\n", id, RxData[0]);
            }
            
            UartSendString(msg);
        }
    }
}

// Xử lý ngắt UART: Nhận lệnh từ C# và chuyển xuống CAN
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (RxDataUART[0] == '\n') // Kết thúc chuỗi
        {
            UartBuffer[UartBufferIndex] = '\0'; // Kết thúc chuỗi C
            
            // --- ECHO LOG ĐỂ DEBUG ---
            char debug_msg[150];
            sprintf(debug_msg, "[DEBUG-RX] I received: %s\n", UartBuffer);
            UartSendString(debug_msg);

            FDCAN_TxHeaderTypeDef TxHdr = {0};
            TxHdr.IdType = FDCAN_STANDARD_ID;
            TxHdr.TxFrameType = FDCAN_DATA_FRAME;
            TxHdr.DataLength = FDCAN_DLC_BYTES_1;
            TxHdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
            TxHdr.BitRateSwitch = FDCAN_BRS_OFF;
            TxHdr.FDFormat = FDCAN_CLASSIC_CAN;
            TxHdr.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
            TxHdr.MessageMarker = 0;

            // Xử lý chuỗi (Dùng strstr để chống nhiễu đầu chuỗi khi mở cổng COM)
            if (strstr(UartBuffer, "<CMD_PING>") != NULL)
            {
                UartSendString("\n====================================\n");
                UartSendString("[SYSTEM] STM32H743 MASTER CONNECTED!\n");
                UartSendString("[SYSTEM] CAN BUS 125KBPS IS READY.\n");
                UartSendString("====================================\n");
            }
            else if (strstr(UartBuffer, "<CMD_START_ADC>") != NULL)
            {
                TxHdr.Identifier = 0x010; // Dùng chung ID 0x010 của Slave 1
                uint8_t d[1] = {0x02}; // Mã lệnh Start ADC
                HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHdr, d);
            }
            else if (strstr(UartBuffer, "<CMD_STOP_ADC>") != NULL)
            {
                TxHdr.Identifier = 0x010; // Dùng chung ID 0x010 của Slave 1
                uint8_t d[1] = {0x03}; // Mã lệnh Stop ADC
                HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHdr, d);
            }
            else if (strstr(UartBuffer, "<PWM_FWD_") != NULL)
            {
                // Format: <PWM_FWD_50> → Quay thuận 50%
                char *val_str = strstr(UartBuffer, "<PWM_FWD_") + 9;
                int pwm_val = atoi(val_str);
                if (pwm_val > 100) pwm_val = 100;
                
                TxHdr.Identifier = 0x020;
                TxHdr.DataLength = FDCAN_DLC_BYTES_2;
                uint8_t d[2] = {0x10, (uint8_t)pwm_val}; // cmd=FWD, duty
                HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHdr, d);
                TxHdr.DataLength = FDCAN_DLC_BYTES_1; // Reset về mặc định
            }
            else if (strstr(UartBuffer, "<PWM_REV_") != NULL)
            {
                // Format: <PWM_REV_50> → Quay ngược 50%
                char *val_str = strstr(UartBuffer, "<PWM_REV_") + 9;
                int pwm_val = atoi(val_str);
                if (pwm_val > 100) pwm_val = 100;
                
                TxHdr.Identifier = 0x020;
                TxHdr.DataLength = FDCAN_DLC_BYTES_2;
                uint8_t d[2] = {0x11, (uint8_t)pwm_val}; // cmd=REV, duty
                HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHdr, d);
                TxHdr.DataLength = FDCAN_DLC_BYTES_1;
            }
            else if (strstr(UartBuffer, "<PWM_BRAKE>") != NULL)
            {
                TxHdr.Identifier = 0x020;
                TxHdr.DataLength = FDCAN_DLC_BYTES_2;
                uint8_t d[2] = {0x12, 0x00}; // cmd=BRAKE
                HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHdr, d);
                TxHdr.DataLength = FDCAN_DLC_BYTES_1;
            }
            else if (strstr(UartBuffer, "<PWM_STOP>") != NULL)
            {
                TxHdr.Identifier = 0x020;
                TxHdr.DataLength = FDCAN_DLC_BYTES_2;
                uint8_t d[2] = {0x13, 0x00}; // cmd=COAST
                HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHdr, d);
                TxHdr.DataLength = FDCAN_DLC_BYTES_1;
            }
            // ============ LỆNH PID ============
            else if (strstr(UartBuffer, "<PID_SET:") != NULL)
            {
                // Format: <PID_SET:Kp_100,Ki_100,Kd_100> (int values)
                char *val_str = strstr(UartBuffer, "<PID_SET:") + 9;
                int kp_i = 0, ki_i = 0, kd_i = 0;
                sscanf(val_str, "%d,%d,%d", &kp_i, &ki_i, &kd_i);
                
                TxHdr.Identifier = 0x020;
                TxHdr.DataLength = FDCAN_DLC_BYTES_8; // 7 bytes needed, use 8
                uint8_t d[8] = {0x20,
                    (uint8_t)(kp_i >> 8), (uint8_t)(kp_i & 0xFF),
                    (uint8_t)(ki_i >> 8), (uint8_t)(ki_i & 0xFF),
                    (uint8_t)(kd_i >> 8), (uint8_t)(kd_i & 0xFF), 0x00};
                HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHdr, d);
                TxHdr.DataLength = FDCAN_DLC_BYTES_1;
            }
            else if (strstr(UartBuffer, "<PID_TARGET:") != NULL)
            {
                char *val_str = strstr(UartBuffer, "<PID_TARGET:") + 12;
                int target_rpm = atoi(val_str);
                int16_t rpm_i = (int16_t)target_rpm;
                
                TxHdr.Identifier = 0x020;
                TxHdr.DataLength = FDCAN_DLC_BYTES_3;
                uint8_t d[3] = {0x21, (uint8_t)(rpm_i >> 8), (uint8_t)(rpm_i & 0xFF)};
                HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHdr, d);
                TxHdr.DataLength = FDCAN_DLC_BYTES_1;
            }
            else if (strstr(UartBuffer, "<PID_START>") != NULL)
            {
                TxHdr.Identifier = 0x020;
                uint8_t d[1] = {0x22};
                HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHdr, d);
            }
            else if (strstr(UartBuffer, "<PID_STOP>") != NULL)
            {
                TxHdr.Identifier = 0x020;
                uint8_t d[1] = {0x23};
                HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHdr, d);
            }
            else if (strstr(UartBuffer, "<PID_CPR:") != NULL)
            {
                char *val_str = strstr(UartBuffer, "<PID_CPR:") + 9;
                uint16_t cpr = (uint16_t)atoi(val_str);
                
                TxHdr.Identifier = 0x020;
                TxHdr.DataLength = FDCAN_DLC_BYTES_3;
                uint8_t d[3] = {0x24, (uint8_t)(cpr >> 8), (uint8_t)(cpr & 0xFF)};
                HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHdr, d);
                TxHdr.DataLength = FDCAN_DLC_BYTES_1;
            }

            // Reset buffer
            UartBufferIndex = 0;
        }
        else if (RxDataUART[0] != '\r')
        {
            if (UartBufferIndex < 127) {
                UartBuffer[UartBufferIndex++] = RxDataUART[0];
            }
        }
        
        // Tiếp tục lắng nghe
        HAL_UART_Receive_IT(&huart1, RxDataUART, 1);
    }
}

// Xử lý tự động khôi phục nếu UART bị lỗi (VD: Lỗi Overrun do nhiễu rác)
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        // Xóa mọi cờ lỗi
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        
        // Khởi động lại ngắt nhận
        HAL_UART_Receive_IT(&huart1, RxDataUART, 1);
    }
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
