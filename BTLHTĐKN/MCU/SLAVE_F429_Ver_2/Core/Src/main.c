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
CAN_HandleTypeDef hcan1;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */
volatile uint8_t motor_running = 0;
volatile int16_t g_encoder_count = 0;
volatile uint8_t g_motor_duty = 0;
volatile uint8_t g_motor_dir = 0;

// Biến điều khiển PWM mềm trên IN1/IN2 (dùng trong ngắt TIM3)
volatile uint8_t motor_cmd = 0; // 0=dừng, 1=thuận, 2=ngược

// Cấu hình Encoder
volatile uint16_t g_cpr = 1500; // Counts Per Revolution (có thể chỉnh từ GUI)

// ======================== PID CONTROLLER ========================
typedef struct {
    float Kp, Ki, Kd;
    float setpoint;        // Target RPM
    float integral;
    float prev_error;
    float output;          // -100 to +100 (%)
    uint8_t enabled;
    float integral_limit;  // Anti-windup limit
} PID_Controller;

volatile PID_Controller pid = {
    .Kp = 0.05f, .Ki = 0.01f, .Kd = 0.00f,
    .setpoint = 0, .integral = 0, .prev_error = 0,
    .output = 0, .enabled = 0, .integral_limit = 5000.0f
};

volatile int16_t g_actual_rpm = 0; // RPM thực tế (được cập nhật mỗi 10ms)
float g_filtered_rpm = 0.0f;       // Bộ lọc thông thấp (Low-pass filter) cho RPM
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include "stm32f4xx_hal_tim.h"

// ======================== KHỞI TẠO PHẦN CỨNG MOTOR ========================
// Chiến lược: Giữ ENA (PA6) luôn HIGH bằng GPIO.
// Dùng TIM3 làm timebase + ngắt Update/CC1 để tạo PWM mềm trên IN1/IN2 (PE2/PE3).
// --> Khi PWM ON:  IN1/IN2 đặt theo hướng quay → motor được cấp điện.
// --> Khi PWM OFF: IN1=0, IN2=0 → coast qua H-bridge (KHÔNG qua body diode).
// --> Loại bỏ hoàn toàn hiện tượng freewheeling gây sáng 2 đèn.
static void Motor_Init(void)
{
    // === 1. Chuyển PA6 thành GPIO Output, kéo HIGH vĩnh viễn (ENA = luôn bật) ===
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET); // ENA = 1 vĩnh viễn

    // === 2. TIM3: Dùng làm timebase cho PWM mềm ===
    // MX_TIM3_Init() đã cấu hình Period=1249 (10kHz) với Prescaler=0.
    // Ta bật ngắt Update (đầu chu kỳ) và CC1 (điểm duty) để toggle IN1/IN2.
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0); // Duty ban đầu = 0
    HAL_TIM_PWM_Start_IT(&htim3, TIM_CHANNEL_1);     // Bật PWM + ngắt CC1
    __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_UPDATE);       // Bật thêm ngắt Update (đầu chu kỳ)

    // === 3. TIM4 Encoder đã được cấu hình bởi MX_TIM4_Init() ===
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
}

// ======================== HÀM ĐIỀU KHIỂN MOTOR ========================
static void Motor_Coast(void);
static void Motor_SetPWM(uint8_t duty_percent);

static void Motor_SetPWM(uint8_t duty_percent)
{
    if (duty_percent > 100) duty_percent = 100;
    g_motor_duty = duty_percent;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim3);
    uint32_t ccr = (uint32_t)duty_percent * arr / 100;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ccr);
}

static void Motor_Forward(uint8_t duty)
{
    if (duty == 0) {
        Motor_Coast();
        return;
    }
    g_motor_dir = 0;
    motor_cmd = 1;
    Motor_SetPWM(duty);
    motor_running = 1;
}

static void Motor_Reverse(uint8_t duty)
{
    if (duty == 0) {
        Motor_Coast();
        return;
    }
    g_motor_dir = 1;
    motor_cmd = 2;
    Motor_SetPWM(duty);
    motor_running = 1;
}

static void Motor_Brake(void)
{
    motor_cmd = 0;
    Motor_SetPWM(0);
    // Phanh cứng: IN1=1, IN2=1 (short motor qua H-bridge)
    GPIOE->BSRR = GPIO_PIN_2 | GPIO_PIN_3;
    motor_running = 0;
}

static void Motor_Coast(void)
{
    motor_cmd = 0;
    Motor_SetPWM(0);
    // Dừng mềm: IN1=0, IN2=0
    GPIOE->BSRR = (GPIO_PIN_2 | GPIO_PIN_3) << 16;
    motor_running = 0;
}

static int16_t Motor_GetEncoder(void)
{
    return (int16_t)__HAL_TIM_GET_COUNTER(&htim4);
}

// ======================== NGẮT TIM3: PWM MỀM TRÊN IN1/IN2 ========================
// Callback đầu chu kỳ PWM (Update Event) → Đặt IN1/IN2 theo hướng quay
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        if (motor_cmd == 1) // Thuận: IN1=1, IN2=0
        {
            GPIOE->BSRR = GPIO_PIN_2 | (GPIO_PIN_3 << 16);
        }
        else if (motor_cmd == 2) // Ngược: IN1=0, IN2=1
        {
            GPIOE->BSRR = (GPIO_PIN_2 << 16) | GPIO_PIN_3;
        }
    }
}

// Callback khi counter đạt CCR1 (hết phần ON) → Tắt cả 2 IN (coast, không freewheeling)
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        if (motor_cmd != 0)
        {
            // Coast: IN1=0, IN2=0 → dòng tuần hoàn qua H-bridge, không qua body diode
            GPIOE->BSRR = (GPIO_PIN_2 | GPIO_PIN_3) << 16;
        }
    }
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
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  // Khởi tạo Motor (PWM + Encoder + Direction)
  Motor_Init();
  
  CAN_FilterTypeDef sFilterConfig;
  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x111 << 5; // Bank 0: Nhận tin nhắn ID 0x111 từ SLAVE_F407
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0xFFFF; // Lọc chính xác tuyệt đối
  sFilterConfig.FilterMaskIdLow = 0xFFFF;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;
  HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig);

  sFilterConfig.FilterBank = 1;
  sFilterConfig.FilterIdHigh = 0x020 << 5; // Bank 1: Nhận tin nhắn ID 0x020 từ MASTER_H7
  HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig);

  // Khởi động CAN
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

  // === CAN TX Header cho nút bấm (DLC=1) ===
  CAN_TxHeaderTypeDef TxHeader_Btn;
  TxHeader_Btn.StdId = 0x112;
  TxHeader_Btn.ExtId = 0x00;
  TxHeader_Btn.RTR = CAN_RTR_DATA;
  TxHeader_Btn.IDE = CAN_ID_STD;
  TxHeader_Btn.DLC = 1;
  TxHeader_Btn.TransmitGlobalTime = DISABLE;
  uint8_t TxData_Btn[1] = {0x01};
  
  // === CAN TX Header cho telemetry motor cơ bản (DLC=6, ID=0x112) ===
  CAN_TxHeaderTypeDef TxHeader_Motor;
  TxHeader_Motor.StdId = 0x112;
  TxHeader_Motor.ExtId = 0x00;
  TxHeader_Motor.RTR = CAN_RTR_DATA;
  TxHeader_Motor.IDE = CAN_ID_STD;
  TxHeader_Motor.DLC = 6;
  TxHeader_Motor.TransmitGlobalTime = DISABLE;
  
  // === CAN TX Header cho PID telemetry (DLC=8, ID=0x113) ===
  CAN_TxHeaderTypeDef TxHeader_PID;
  TxHeader_PID.StdId = 0x113;
  TxHeader_PID.ExtId = 0x00;
  TxHeader_PID.RTR = CAN_RTR_DATA;
  TxHeader_PID.IDE = CAN_ID_STD;
  TxHeader_PID.DLC = 8;
  TxHeader_PID.TransmitGlobalTime = DISABLE;
  
  uint32_t TxMailbox;
  
  // Biến đo vận tốc encoder
  uint32_t last_enc_time = HAL_GetTick();
  int16_t last_enc_val = Motor_GetEncoder();
  
  // Biến PID timing
  uint32_t last_pid_time = HAL_GetTick();
  uint32_t last_pid_tx_time = HAL_GetTick();
  uint32_t last_motor_tx_time = HAL_GetTick();
  
  // Biến dùng để chống rung (Debounce)
  uint32_t last_press_time = 0;
  uint8_t last_button_state = GPIO_PIN_RESET;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();
    
    // ============ ĐỌC ENCODER & TÍNH RPM MỖI 10ms ============
    if ((now - last_enc_time) >= 10)
    {
      int16_t cur_enc = Motor_GetEncoder();
      // Đảo dấu delta vì chiều quay Forward đang làm Encoder đếm lùi
      int16_t delta = -(cur_enc - last_enc_val);
      last_enc_val = cur_enc;
      uint32_t dt_ms = now - last_enc_time;
      last_enc_time = now;
      
      // Tính RPM tức thời
      int16_t raw_rpm = (int16_t)((int32_t)delta * 60000 / ((int32_t)g_cpr * (int32_t)dt_ms));
      
      // Lọc Low-pass (EMA) để chống nhiễu lượng tử hóa ở tốc độ thấp
      // alpha = 0.3 (30% giá trị mới, 70% giá trị cũ)
      g_filtered_rpm = (0.7f * g_filtered_rpm) + (0.3f * (float)raw_rpm);
      g_actual_rpm = (int16_t)g_filtered_rpm;
      
      g_encoder_count = cur_enc;
    }
    
    // ============ VÒNG LẶP PID MỖI 10ms ============
    if (pid.enabled && (now - last_pid_time) >= 10)
    {
      float dt = (float)(now - last_pid_time) / 1000.0f; // Chuyển sang giây
      last_pid_time = now;
      
      float error = pid.setpoint - (float)g_actual_rpm;
      
      // Integral với anti-windup
      pid.integral += error * dt;
      if (pid.integral > pid.integral_limit) pid.integral = pid.integral_limit;
      if (pid.integral < -pid.integral_limit) pid.integral = -pid.integral_limit;
      
      // Derivative
      float derivative = (error - pid.prev_error) / dt;
      pid.prev_error = error;
      
      // PID output
      pid.output = pid.Kp * error + pid.Ki * pid.integral + pid.Kd * derivative;
      
      // Clamp output: -100 to +100
      if (pid.output > 100.0f) pid.output = 100.0f;
      if (pid.output < -100.0f) pid.output = -100.0f;
      
      // Áp dụng PWM + hướng quay
      if (pid.output >= 0)
      {
        Motor_Forward((uint8_t)pid.output);
      }
      else
      {
        Motor_Reverse((uint8_t)(-pid.output));
      }
    }
    
    // ============ GỬI PID TELEMETRY MỖI 50ms (ID=0x113) ============
    if (pid.enabled && (now - last_pid_tx_time) >= 50)
    {
      last_pid_tx_time = now;
      
      int16_t target_i = (int16_t)pid.setpoint;
      int16_t actual_i = g_actual_rpm;
      int16_t error_i = target_i - actual_i;
      int8_t  output_i = (int8_t)pid.output;
      
      uint8_t TxData_PID[8];
      TxData_PID[0] = (uint8_t)(target_i >> 8);
      TxData_PID[1] = (uint8_t)(target_i & 0xFF);
      TxData_PID[2] = (uint8_t)(actual_i >> 8);
      TxData_PID[3] = (uint8_t)(actual_i & 0xFF);
      TxData_PID[4] = (uint8_t)(error_i >> 8);
      TxData_PID[5] = (uint8_t)(error_i & 0xFF);
      TxData_PID[6] = (uint8_t)output_i;
      TxData_PID[7] = 0x00;
      
      HAL_CAN_AddTxMessage(&hcan1, &TxHeader_PID, TxData_PID, &TxMailbox);
    }
    
    // ============ GỬI MOTOR TELEMETRY MỖI 100ms (ID=0x112) ============
    if ((now - last_motor_tx_time) >= 100)
    {
      last_motor_tx_time = now;
      
      int16_t rpm16 = g_actual_rpm;
      int16_t enc16 = g_encoder_count;
      uint8_t TxData_Motor[6];
      TxData_Motor[0] = (uint8_t)(rpm16 >> 8);
      TxData_Motor[1] = (uint8_t)(rpm16 & 0xFF);
      TxData_Motor[2] = g_motor_dir;
      TxData_Motor[3] = g_motor_duty;
      TxData_Motor[4] = (uint8_t)(enc16 >> 8);
      TxData_Motor[5] = (uint8_t)(enc16 & 0xFF);
      
      HAL_CAN_AddTxMessage(&hcan1, &TxHeader_Motor, TxData_Motor, &TxMailbox);
    }
    
    // ============ XỬ LÝ NÚT BẤM (Debounce) ============
    static uint8_t debounced_state = GPIO_PIN_RESET;
    uint8_t raw_state = HAL_GPIO_ReadPin(USER_BTN_GPIO_Port, USER_BTN_Pin);
    
    if (raw_state != last_button_state)
    {
      last_press_time = HAL_GetTick();
    }
    
    if ((HAL_GetTick() - last_press_time) > 50)
    {
      if (raw_state != debounced_state)
      {
        debounced_state = raw_state;
        if (debounced_state == GPIO_PIN_SET)
        {
          HAL_CAN_AddTxMessage(&hcan1, &TxHeader_Btn, TxData_Btn, &TxMailbox);
        }
      }
    }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

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
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
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
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1249;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 5;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 5;
  if (HAL_TIM_Encoder_Init(&htim4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, DIR_IN1_Pin|DIR_IN2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, LED_GREEN_Pin|LED_RED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : DIR_IN1_Pin DIR_IN2_Pin */
  GPIO_InitStruct.Pin = DIR_IN1_Pin|DIR_IN2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : USER_BTN_Pin */
  GPIO_InitStruct.Pin = USER_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(USER_BTN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_GREEN_Pin LED_RED_Pin */
  GPIO_InitStruct.Pin = LED_GREEN_Pin|LED_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
// Hàm ngắt CAN - xử lý lệnh từ Master và tín hiệu từ F407
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef RxHeader;
  uint8_t RxData[8];
  
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
  {
    // Lệnh từ F407 (ID 0x111): Toggle LED xanh
    if (RxHeader.StdId == 0x111 && RxData[0] == 0x01)
    {
      HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    }
    // Lệnh từ Master K2 (ID 0x020): Toggle LED hoặc Điều khiển Motor
    else if (RxHeader.StdId == 0x020)
    {
      uint8_t cmd = RxData[0];
      
      switch (cmd)
      {
        case 0x01: // Toggle LED từ nút K2
          HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
          break;
        case 0x10: // Quay thuận (Forward) - manual
        {
          if (!pid.enabled) { // Chỉ cho phép manual khi PID tắt
            uint8_t duty = RxData[1];
            Motor_Forward(duty);
            HAL_GPIO_WritePin(GPIOG, LED_GREEN_Pin, GPIO_PIN_SET);
          }
          break;
        }
        case 0x11: // Quay ngược (Reverse) - manual
        {
          if (!pid.enabled) {
            uint8_t duty = RxData[1];
            Motor_Reverse(duty);
            HAL_GPIO_WritePin(GPIOG, LED_GREEN_Pin, GPIO_PIN_SET);
          }
          break;
        }
        case 0x12: // Phanh (Brake)
          pid.enabled = 0;
          pid.integral = 0;
          Motor_Brake();
          HAL_GPIO_WritePin(GPIOG, LED_GREEN_Pin, GPIO_PIN_RESET);
          break;
        case 0x13: // Dừng mềm (Coast)
          pid.enabled = 0;
          pid.integral = 0;
          Motor_Coast();
          HAL_GPIO_WritePin(GPIOG, LED_GREEN_Pin, GPIO_PIN_RESET);
          break;
          
        // ============ LỆNH PID ============
        case 0x20: // Set PID params: [cmd, Kp_H, Kp_L, Ki_H, Ki_L, Kd_H, Kd_L]
        {
          int16_t kp_i = (int16_t)((RxData[1] << 8) | RxData[2]);
          int16_t ki_i = (int16_t)((RxData[3] << 8) | RxData[4]);
          int16_t kd_i = (int16_t)((RxData[5] << 8) | RxData[6]);
          pid.Kp = (float)kp_i / 100.0f;
          pid.Ki = (float)ki_i / 100.0f;
          pid.Kd = (float)kd_i / 100.0f;
          break;
        }
        case 0x21: // Set target RPM: [cmd, RPM_H, RPM_L]
        {
          int16_t target = (int16_t)((RxData[1] << 8) | RxData[2]);
          pid.setpoint = (float)target;
          break;
        }
        case 0x22: // Start PID
          pid.integral = 0;
          pid.prev_error = 0;
          pid.enabled = 1;
          HAL_GPIO_WritePin(GPIOG, LED_GREEN_Pin, GPIO_PIN_SET);
          break;
        case 0x23: // Stop PID
          pid.enabled = 0;
          pid.integral = 0;
          Motor_Coast();
          HAL_GPIO_WritePin(GPIOG, LED_GREEN_Pin, GPIO_PIN_RESET);
          break;
        case 0x24: // Set CPR: [cmd, CPR_H, CPR_L]
        {
          uint16_t cpr = (uint16_t)((RxData[1] << 8) | RxData[2]);
          if (cpr > 0) g_cpr = cpr;
          break;
        }
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
