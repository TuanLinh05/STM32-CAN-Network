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
ADC_HandleTypeDef hadc1;

CAN_HandleTypeDef hcan1;

/* USER CODE BEGIN PV */
volatile uint8_t is_adc_running = 0;

// === BIẾN GLOBAL 3 KÊNH ADC ===
volatile uint16_t g_adc_ch1 = 0; // PA1
volatile uint16_t g_adc_ch2 = 0; // PA2
volatile uint16_t g_adc_ch3 = 0; // PA3
volatile uint32_t g_adc_send_count = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Hàm đọc 1 kênh ADC bất kỳ bằng cách thay đổi cấu hình Channel trước khi đọc
static uint16_t ReadADC_Channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = 1;
    // Tăng thời gian lấy mẫu lên mức TỐI ĐA để nạp đầy tụ điện (giảm nhiễu do trở kháng)
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    
    // KỸ THUẬT CHỐNG GHOSTING (NHIỄU CHÉO): Đọc bỏ lần 1 (Dummy Read)
    // Việc này giúp xả hết điện áp của kênh trước đó còn sót lại trên tụ lấy mẫu nội
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    HAL_ADC_GetValue(&hadc1); // Không lưu giá trị này
    HAL_ADC_Stop(&hadc1);
    
    // Đọc thật lần 2 để lấy kết quả chính xác tuyệt đối
    HAL_ADC_Start(&hadc1);
    uint16_t val = 0;
    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        val = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);
    return val;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

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
  MX_CAN1_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  // === C\u1ea4U H\u00ccNH TH\u00caM PA2, PA3 L\u00c0M ANALOG INPUT CHO ADC ===
  {
      GPIO_InitTypeDef GPIO_InitStruct = {0};
      GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
      GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
      GPIO_InitStruct.Pull = GPIO_NOPULL;
      HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
  
  CAN_FilterTypeDef sFilterConfig;
  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x112 << 5; // Bank 0: Nhận tin nhắn ID 0x112 từ SLAVE_F429
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0xFFFF; // Lọc chính xác tuyệt đối
  sFilterConfig.FilterMaskIdLow = 0xFFFF;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;
  HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig);

  // Cấu hình Bank 1 để nhận tin nhắn ID 0x010 từ MASTER_H7
  sFilterConfig.FilterBank = 1;
  sFilterConfig.FilterIdHigh = 0x010 << 5; 
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0xFFFF;
  sFilterConfig.FilterMaskIdLow = 0xFFFF;
  HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig);

  // Khởi động CAN
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

  // Khai báo biến truyền dữ liệu
  CAN_TxHeaderTypeDef TxHeader;
  TxHeader.StdId = 0x111; // F407 gửi tin nhắn có ID 0x111
  TxHeader.ExtId = 0x00;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 1;
  TxHeader.TransmitGlobalTime = DISABLE;
  
  uint8_t TxData[1] = {0x01}; // Dữ liệu mang lệnh đổi màu LED
  uint32_t TxMailbox;
  
  // Biến dùng để chống rung (Debounce) không dùng Delay
  uint32_t last_press_time = 0;
  uint8_t last_button_state = GPIO_PIN_RESET;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    static uint32_t last_adc_time = 0;
    
    // --- ĐỌC 3 KÊNH ADC (PA1, PA2, PA3) VÀ GỬI VỀ MASTER ---
    if (is_adc_running)
    {
        if (HAL_GetTick() - last_adc_time > 200)
        {
            last_adc_time = HAL_GetTick();
            
            // Đọc tuần tự 3 kênh
            g_adc_ch1 = ReadADC_Channel(ADC_CHANNEL_1); // PA1
            g_adc_ch2 = ReadADC_Channel(ADC_CHANNEL_2); // PA2
            g_adc_ch3 = ReadADC_Channel(ADC_CHANNEL_3); // PA3
            
            if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
            {
                CAN_TxHeaderTypeDef TxHdrADC;
                TxHdrADC.StdId = 0x11A;
                TxHdrADC.ExtId = 0;
                TxHdrADC.RTR = CAN_RTR_DATA;
                TxHdrADC.IDE = CAN_ID_STD;
                TxHdrADC.DLC = 6; // 3 kênh x 2 byte = 6 byte
                TxHdrADC.TransmitGlobalTime = DISABLE;
                
                uint8_t adc_data[6];
                adc_data[0] = (g_adc_ch1 >> 8) & 0xFF;
                adc_data[1] = g_adc_ch1 & 0xFF;
                adc_data[2] = (g_adc_ch2 >> 8) & 0xFF;
                adc_data[3] = g_adc_ch2 & 0xFF;
                adc_data[4] = (g_adc_ch3 >> 8) & 0xFF;
                adc_data[5] = g_adc_ch3 & 0xFF;
                
                uint32_t mailbox_adc;
                if (HAL_CAN_AddTxMessage(&hcan1, &TxHdrADC, adc_data, &mailbox_adc) == HAL_OK)
                {
                    g_adc_send_count++;
                    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
                }
            }
        }
    }
    static uint8_t debounced_state = GPIO_PIN_RESET;
    uint8_t raw_state = HAL_GPIO_ReadPin(USER_BTN_GPIO_Port, USER_BTN_Pin);
    
    // Nếu trạng thái vật lý của nút bị thay đổi (nhấn, nhả, hoặc nhiễu)
    if (raw_state != last_button_state)
    {
      last_press_time = HAL_GetTick(); // Đặt lại bộ đếm thời gian chống rung
    }
    
    // Nếu trạng thái đã giữ nguyên không đổi trong suốt 50ms (Đã hết rung)
    if ((HAL_GetTick() - last_press_time) > 40)
    {
      // Và nếu trạng thái ổn định này khác với trạng thái đã ghi nhận trước đó
      if (raw_state != debounced_state)
      {
        debounced_state = raw_state; // Xác nhận trạng thái mới
        
        // Nếu trạng thái mới là ĐANG BẤM
        if (debounced_state == GPIO_PIN_SET)
        {
          // Phát lệnh CAN lên Bus
          HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox);
        }
      }
    }
    // Cập nhật lại trạng thái nút bấm vật lý cho vòng lặp sau
    last_button_state = raw_state;
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 50;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 5;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_7TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LED_GREEN_Pin|LED_ORANGE_Pin|GPIO_PIN_14, GPIO_PIN_RESET);

  /*Configure GPIO pin : USER_BTN_Pin */
  GPIO_InitStruct.Pin = USER_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(USER_BTN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_GREEN_Pin LED_ORANGE_Pin PD14 */
  GPIO_InitStruct.Pin = LED_GREEN_Pin|LED_ORANGE_Pin|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// Hàm ngắt này tự động được gọi khi có tin nhắn CAN mới bay tới
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef RxHeader;
  uint8_t RxData[8];
  
  // Đọc tin nhắn ra khỏi bộ đệm
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
  {
    // Kiểm tra đúng ID 0x112 do F429 gửi sang
    if (RxHeader.StdId == 0x112 && RxData[0] == 0x01)
    {
      HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin); // Đảo trạng thái LED xanh lá
    }
    // Kiểm tra đúng ID 0x010 do MASTER H7 gửi sang
    else if (RxHeader.StdId == 0x010)
    {
      if (RxData[0] == 0x01) {
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13); // Đảo trạng thái LED cam
      } else if (RxData[0] == 0x02) {
        is_adc_running = 1; // Bắt đầu đọc ADC
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET); // Sáng LED Đỏ (PD14) để debug
      } else if (RxData[0] == 0x03) {
        is_adc_running = 0; // Dừng đọc ADC
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET); // Tắt LED Đỏ (PD14)
      }
    }
  }
}
/* USER CODE END 4 */

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
