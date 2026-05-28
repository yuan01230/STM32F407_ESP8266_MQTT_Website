/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include "../../Library/led/led.h"
#include "../../Library/key/key.h"
#include "../../Library/beep/beep.h"
#include "../../Library/delay/delay.h"
#include "../../Library/tftlcd/tftlcd.h"
#include "../../Library/DHT11/dht11.h"
#include "../../Library/Light_Sensor/light_sensor.h"
#include "../../Library/mpu6050/MPU6050.h"
#include "../../Library/aliyun_iot/aliyun_iot.h"
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
ADC_HandleTypeDef hadc3;

SD_HandleTypeDef hsd;
DMA_HandleTypeDef hdma_sdio_rx;
DMA_HandleTypeDef hdma_sdio_tx;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

SRAM_HandleTypeDef hsram1;

/* USER CODE BEGIN PV */
static uint32_t g_uart_test_count = 0U;
static uint8_t g_mpu_ready = 0U;
static uint8_t g_mpu_dmp_ready = 0U;
static delay_nb_timer_t g_sensor_refresh_timer;
static uint16_t g_light_adc_value = 0U;
static float g_light_voltage_value = 0.0f;
static float g_dht_temperature_value = 0.0f;
static float g_dht_humidity_value = 0.0f;
static float g_pitch_value = 0.0f;
static float g_roll_value = 0.0f;
static float g_yaw_value = 0.0f;
static char g_dht_temp_text[24] = "";
static char g_dht_humi_text[24] = "";
static char g_light_adc_text[24] = "";
static char g_light_volt_text[24] = "";
static char g_light_level_text[24] = "";
static char g_ax_text[16] = "";
static char g_ay_text[16] = "";
static char g_az_text[16] = "";
static char g_gx_text[16] = "";
static char g_gy_text[16] = "";
static char g_gz_text[16] = "";
static char g_mpu_temp_text[24] = "";
static char g_pitch_text[24] = "";
static char g_roll_text[24] = "";
static char g_yaw_text[24] = "";
static char g_status_left_text[32] = "";
static char g_status_right_text[16] = "";
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_FSMC_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC3_Init(void);
static void MX_SDIO_SD_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
static void LCD_Test_ShowWelcome(void);
static void LCD_ShowBootScreen(void);
static void LCD_UpdateBootStatus(const char *line1, const char *line2, uint16_t color);
static void LCD_Test_Run(void);
static void LCD_UpdateValueLine(uint16_t x, uint16_t y, uint16_t width, char *cache, const char *text, uint16_t color);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void LCD_UpdateValueLine(uint16_t x, uint16_t y, uint16_t width, char *cache, const char *text, uint16_t color)
{
  char padded_text[24];
  size_t max_chars = 0U;
  size_t text_len = 0U;

  if ((cache == NULL) || (text == NULL))
  {
    return;
  }

  max_chars = width / 8U;
  if (max_chars == 0U)
  {
    max_chars = 1U;
  }
  if (max_chars > (sizeof(padded_text) - 1U))
  {
    max_chars = sizeof(padded_text) - 1U;
  }

  memset(padded_text, ' ', max_chars);
  padded_text[max_chars] = '\0';
  text_len = strlen(text);
  if (text_len > max_chars)
  {
    text_len = max_chars;
  }
  memcpy(padded_text, text, text_len);

  if (strcmp(cache, padded_text) == 0)
  {
    return;
  }

  FRONT_COLOR = color;
  LCD_ShowString(x, y, width, 20U, 16U, (uint8_t *)padded_text);
  strncpy(cache, padded_text, 23U);
  cache[23] = '\0';
}

static void LCD_Test_ShowWelcome(void)
{
  BACK_COLOR = WHITE;
  FRONT_COLOR = BLACK;
  LCD_Clear(WHITE);

  LCD_Fill(0U, 0U, 319U, 34U, BLUE);
  FRONT_COLOR = WHITE;
  LCD_ShowString(8U, 8U, 180U, 24U, 16U, (uint8_t *)"Sensor Dashboard");
  LCD_ShowString(220U, 8U, 90U, 24U, 16U, (uint8_t *)"500ms");

  FRONT_COLOR = BLACK;
  LCD_DrawRectangle(6U, 42U, 313U, 110U);
  LCD_DrawRectangle(6U, 118U, 313U, 196U);
  LCD_DrawRectangle(6U, 204U, 313U, 332U);
  LCD_DrawRectangle(6U, 340U, 313U, 438U);
  LCD_DrawRectangle(6U, 446U, 313U, 474U);

  FRONT_COLOR = BLUE;
  LCD_ShowString(14U, 48U, 120U, 24U, 16U, (uint8_t *)"DHT11");
  LCD_ShowString(14U, 124U, 120U, 24U, 16U, (uint8_t *)"Light Sensor");
  LCD_ShowString(14U, 210U, 150U, 24U, 16U, (uint8_t *)"MPU6050 Raw");
  LCD_ShowString(14U, 346U, 150U, 24U, 16U, (uint8_t *)"MPU6050 DMP");
  LCD_ShowString(14U, 452U, 80U, 20U, 16U, (uint8_t *)"Status");

  FRONT_COLOR = BLACK;
  LCD_ShowString(14U, 72U, 100U, 20U, 16U, (uint8_t *)"Temp:");
  LCD_ShowString(14U, 92U, 100U, 20U, 16U, (uint8_t *)"Humi:");

  LCD_ShowString(14U, 148U, 100U, 20U, 16U, (uint8_t *)"ADC:");
  LCD_ShowString(14U, 168U, 100U, 20U, 16U, (uint8_t *)"Volt:");
  LCD_ShowString(168U, 168U, 80U, 20U, 16U, (uint8_t *)"Level:");

  LCD_ShowString(14U, 236U, 40U, 20U, 16U, (uint8_t *)"AX:");
  LCD_ShowString(110U, 236U, 40U, 20U, 16U, (uint8_t *)"AY:");
  LCD_ShowString(206U, 236U, 40U, 20U, 16U, (uint8_t *)"AZ:");
  LCD_ShowString(14U, 264U, 40U, 20U, 16U, (uint8_t *)"GX:");
  LCD_ShowString(110U, 264U, 40U, 20U, 16U, (uint8_t *)"GY:");
  LCD_ShowString(206U, 264U, 40U, 20U, 16U, (uint8_t *)"GZ:");
  LCD_ShowString(14U, 292U, 60U, 20U, 16U, (uint8_t *)"Temp:");

  LCD_ShowString(14U, 372U, 60U, 20U, 16U, (uint8_t *)"Pitch:");
  LCD_ShowString(14U, 396U, 60U, 20U, 16U, (uint8_t *)"Roll:");
  LCD_ShowString(14U, 420U, 60U, 20U, 16U, (uint8_t *)"Yaw:");
}

static void LCD_ShowBootScreen(void)
{
  BACK_COLOR = WHITE;
  FRONT_COLOR = BLACK;
  LCD_Clear(WHITE);

  LCD_Fill(0U, 0U, 319U, 54U, BLUE);
  FRONT_COLOR = WHITE;
  LCD_ShowString(14U, 16U, 220U, 24U, 16U, (uint8_t *)"Sensor Dashboard");

  FRONT_COLOR = BLACK;
  LCD_DrawRectangle(18U, 90U, 302U, 230U);
  LCD_ShowString(34U, 108U, 220U, 24U, 16U, (uint8_t *)"System Booting...");
  LCD_ShowString(34U, 150U, 240U, 24U, 16U, (uint8_t *)"Init peripherals");
  LCD_ShowString(34U, 178U, 240U, 24U, 16U, (uint8_t *)"Please wait");
}

static void LCD_UpdateBootStatus(const char *line1, const char *line2, uint16_t color)
{
  char text1[32];
  char text2[32];

  memset(text1, ' ', sizeof(text1) - 1U);
  memset(text2, ' ', sizeof(text2) - 1U);
  text1[sizeof(text1) - 1U] = '\0';
  text2[sizeof(text2) - 1U] = '\0';

  if (line1 != NULL)
  {
    strncpy(text1, line1, sizeof(text1) - 1U);
  }
  if (line2 != NULL)
  {
    strncpy(text2, line2, sizeof(text2) - 1U);
  }

  FRONT_COLOR = color;
  LCD_ShowString(34U, 150U, 240U, 24U, 16U, (uint8_t *)text1);
  LCD_ShowString(34U, 178U, 240U, 24U, 16U, (uint8_t *)text2);
}

static void LCD_Test_Run(void)
{
  char line[64];
  DHT11_Data_t dht11_sample;
  uint16_t light_adc = 0U;
  float light_voltage = 0.0f;
  LightLevel_t light_level = LIGHT_LEVEL_DARK;
  short ax = 0;
  short ay = 0;
  short az = 0;
  short gx = 0;
  short gy = 0;
  short gz = 0;
  float pitch = 0.0f;
  float roll = 0.0f;
  float yaw = 0.0f;
  AliyunIoT_SensorData_t sensor_data;

  BACK_COLOR = WHITE;
  if (DHT11_ReadData(&dht11_sample) == 0U)
  {
    g_dht_temperature_value = (float)dht11_sample.temp_int + ((float)dht11_sample.temp_dec / 10.0f);
    g_dht_humidity_value = (float)dht11_sample.humi_int + ((float)dht11_sample.humi_dec / 10.0f);
    snprintf(line, sizeof(line), "%u.%u C", (unsigned int)dht11_sample.temp_int, (unsigned int)dht11_sample.temp_dec);
    LCD_UpdateValueLine(120U, 72U, 180U, g_dht_temp_text, line, BLACK);
    snprintf(line, sizeof(line), "%u.%u %%RH", (unsigned int)dht11_sample.humi_int, (unsigned int)dht11_sample.humi_dec);
    LCD_UpdateValueLine(120U, 92U, 180U, g_dht_humi_text, line, BLACK);
  }
  else
  {
    LCD_UpdateValueLine(120U, 72U, 180U, g_dht_temp_text, "Read failed", RED);
    LCD_UpdateValueLine(120U, 92U, 180U, g_dht_humi_text, "--", RED);
  }

  light_adc = LightSensor_ReadADCAverage(3U);
  light_voltage = LightSensor_ConvertToVoltage(light_adc);
  light_level = LightSensor_GetLevelByADC(light_adc);
  g_light_adc_value = light_adc;
  g_light_voltage_value = light_voltage;
  snprintf(line, sizeof(line), "%u", (unsigned int)light_adc);
  LCD_UpdateValueLine(80U, 148U, 80U, g_light_adc_text, line, BLACK);
  snprintf(line, sizeof(line), "%.3f V", light_voltage);
  LCD_UpdateValueLine(80U, 168U, 78U, g_light_volt_text, line, BLACK);
  LCD_UpdateValueLine(220U, 168U, 82U, g_light_level_text, LightSensor_LevelToString(light_level), BLACK);

  if ((g_mpu_ready != 0U) &&
      (MPU_Get_Accelerometer(&ax, &ay, &az) == 0U) &&
      (MPU_Get_Gyroscope(&gx, &gy, &gz) == 0U))
  {
    snprintf(line, sizeof(line), "%d", ax);
    LCD_UpdateValueLine(42U, 236U, 56U, g_ax_text, line, BLACK);
    snprintf(line, sizeof(line), "%d", ay);
    LCD_UpdateValueLine(138U, 236U, 56U, g_ay_text, line, BLACK);
    snprintf(line, sizeof(line), "%d", az);
    LCD_UpdateValueLine(234U, 236U, 72U, g_az_text, line, BLACK);
    snprintf(line, sizeof(line), "%d", gx);
    LCD_UpdateValueLine(42U, 264U, 56U, g_gx_text, line, BLACK);
    snprintf(line, sizeof(line), "%d", gy);
    LCD_UpdateValueLine(138U, 264U, 56U, g_gy_text, line, BLACK);
    snprintf(line, sizeof(line), "%d", gz);
    LCD_UpdateValueLine(234U, 264U, 72U, g_gz_text, line, BLACK);
    snprintf(line, sizeof(line), "%.2f C", ((float)MPU_Get_Temperature()) / 100.0f);
    LCD_UpdateValueLine(78U, 292U, 90U, g_mpu_temp_text, line, BLACK);
  }
  else
  {
    LCD_UpdateValueLine(78U, 292U, 90U, g_mpu_temp_text, "Raw failed", RED);
  }

  if ((g_mpu_dmp_ready != 0U) && (mpu_dmp_get_data(&pitch, &roll, &yaw) == 0U))
  {
    g_pitch_value = pitch;
    g_roll_value = roll;
    g_yaw_value = yaw;
    snprintf(line, sizeof(line), "%.2f deg", pitch);
    LCD_UpdateValueLine(78U, 372U, 130U, g_pitch_text, line, BLACK);
    snprintf(line, sizeof(line), "%.2f deg", roll);
    LCD_UpdateValueLine(78U, 396U, 130U, g_roll_text, line, BLACK);
    snprintf(line, sizeof(line), "%.2f deg", yaw);
    LCD_UpdateValueLine(78U, 420U, 130U, g_yaw_text, line, BLACK);
  }
  else
  {
    if (g_pitch_text[0] == '\0')
    {
      LCD_UpdateValueLine(78U, 372U, 130U, g_pitch_text, "--", RED);
    }
    if (g_roll_text[0] == '\0')
    {
      LCD_UpdateValueLine(78U, 396U, 130U, g_roll_text, "--", RED);
    }
    if (g_yaw_text[0] == '\0')
    {
      LCD_UpdateValueLine(78U, 420U, 130U, g_yaw_text, "--", RED);
    }
  }

  sensor_data.temperature = g_dht_temperature_value;
  sensor_data.humidity = g_dht_humidity_value;
  sensor_data.light_adc = g_light_adc_value;
  sensor_data.light_voltage = g_light_voltage_value;
  sensor_data.pitch = g_pitch_value;
  sensor_data.roll = g_roll_value;
  sensor_data.yaw = g_yaw_value;
  AliyunIoT_UpdateSensorData(&sensor_data);

  snprintf(line, sizeof(line), "ESP:%.18s", AliyunIoT_GetLastLine());
  LCD_UpdateValueLine(70U, 452U, 170U, g_status_left_text, line, BLACK);
  snprintf(line, sizeof(line), "S%u O%lu", (unsigned int)AliyunIoT_GetStep(), AliyunIoT_GetOkCount());
  LCD_UpdateValueLine(244U, 452U, 60U, g_status_right_text, line, BLACK);
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
  MX_DMA_Init();
  MX_FSMC_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_ADC3_Init();
  MX_SDIO_SD_Init();
  MX_FATFS_Init();
  MX_TIM2_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  LED_Init();
  Key_Init();
  BEEP_Init();
  delay_init();
  delay_nb_init();
  TFTLCD_Init();
  LCD_ShowBootScreen();
  LCD_UpdateBootStatus("LCD init done", "Init sensors", BLACK);
  HAL_GPIO_WritePin(ESP8266EST_GPIO_Port, ESP8266EST_Pin, GPIO_PIN_SET);
  DHT11_Init();
  LightSensor_Init();
  LCD_UpdateBootStatus("Sensor init done", "Init ESP8266", BLACK);
  AliyunIoT_Init(&huart3);
  LCD_UpdateBootStatus("ESP8266 init done", "Init MPU6050", BLACK);
  g_mpu_ready = (MPU_Init() == 0U) ? 1U : 0U;
  LCD_UpdateBootStatus((g_mpu_ready != 0U) ? "MPU6050 init ok" : "MPU6050 init fail",
                       "Init DMP",
                       (g_mpu_ready != 0U) ? BLACK : RED);
  g_mpu_dmp_ready = ((g_mpu_ready != 0U) && (mpu_dmp_init() == 0U)) ? 1U : 0U;
  LCD_UpdateBootStatus((g_mpu_dmp_ready != 0U) ? "DMP init ok" : "DMP init fail",
                       "Load dashboard",
                       (g_mpu_dmp_ready != 0U) ? BLACK : RED);
  LCD_Test_ShowWelcome();
  LCD_Test_Run();
  delay_nb_start_ms(&g_sensor_refresh_timer, 500U);

  printf("\r\n===== Sensor Dashboard =====\r\n");
  printf("Single page refresh interval: 500 ms\r\n");
  printf("MPU base init = %u, DMP init = %u\r\n", (unsigned int)g_mpu_ready, (unsigned int)g_mpu_dmp_ready);
  printf("ESP8266 phase-1 IoT flow via USART3 started\r\n");
  printf("=============================\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (delay_nb_is_expired(&g_sensor_refresh_timer) == 1U)
    {
      g_uart_test_count++;
      LCD_Test_Run();
      delay_nb_start_ms(&g_sensor_refresh_timer, 500U);
    }

    AliyunIoT_Task();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */
  /* USER CODE END ADC3_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */
  /* USER CODE END ADC3_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.ScanConvMode = DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DMAContinuousRequests = DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */
  /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief SDIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDIO_SD_Init(void)
{

  /* USER CODE BEGIN SDIO_Init 0 */
  /* USER CODE END SDIO_Init 0 */

  /* USER CODE BEGIN SDIO_Init 1 */
  /* USER CODE END SDIO_Init 1 */
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 0;
  /* USER CODE BEGIN SDIO_Init 2 */
  /* USER CODE END SDIO_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */
  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */
  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */
  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  /* DMA2_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ESP8266EN_GPIO_Port, ESP8266EN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, BEEP_Pin|LED0_Pin|LED1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ESP8266EST_GPIO_Port, ESP8266EST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SPI1_CS_Pin|LCD_BL_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : KEY2_Pin KEY1_Pin KEY0_Pin */
  GPIO_InitStruct.Pin = KEY2_Pin|KEY1_Pin|KEY0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : ESP8266EN_Pin */
  GPIO_InitStruct.Pin = ESP8266EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ESP8266EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BEEP_Pin */
  GPIO_InitStruct.Pin = BEEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BEEP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED0_Pin LED1_Pin */
  GPIO_InitStruct.Pin = LED0_Pin|LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : ESP8266EST_Pin */
  GPIO_InitStruct.Pin = ESP8266EST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ESP8266EST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY_UP_Pin */
  GPIO_InitStruct.Pin = KEY_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(KEY_UP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI1_CS_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SPI1_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_BL_Pin */
  GPIO_InitStruct.Pin = LCD_BL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_BL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DHT11_Pin */
  GPIO_InitStruct.Pin = DHT11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* FSMC initialization function */
static void MX_FSMC_Init(void)
{

  /* USER CODE BEGIN FSMC_Init 0 */
  /* USER CODE END FSMC_Init 0 */

  FSMC_NORSRAM_TimingTypeDef Timing = {0};
  FSMC_NORSRAM_TimingTypeDef ExtTiming = {0};

  /* USER CODE BEGIN FSMC_Init 1 */
  /* USER CODE END FSMC_Init 1 */

  /** Perform the SRAM1 memory initialization sequence
  */
  hsram1.Instance = FSMC_NORSRAM_DEVICE;
  hsram1.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
  /* hsram1.Init */
  hsram1.Init.NSBank = FSMC_NORSRAM_BANK4;
  hsram1.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
  hsram1.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
  hsram1.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
  hsram1.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
  hsram1.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
  hsram1.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
  hsram1.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
  hsram1.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
  hsram1.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
  hsram1.Init.ExtendedMode = FSMC_EXTENDED_MODE_ENABLE;
  hsram1.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
  hsram1.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
  hsram1.Init.PageSize = FSMC_PAGE_SIZE_NONE;
  /* Timing */
  Timing.AddressSetupTime = 8;
  Timing.AddressHoldTime = 15;
  Timing.DataSetupTime = 15;
  Timing.BusTurnAroundDuration = 0;
  Timing.CLKDivision = 16;
  Timing.DataLatency = 17;
  Timing.AccessMode = FSMC_ACCESS_MODE_A;
  /* ExtTiming */
  ExtTiming.AddressSetupTime = 15;
  ExtTiming.AddressHoldTime = 15;
  ExtTiming.DataSetupTime = 15;
  ExtTiming.BusTurnAroundDuration = 15;
  ExtTiming.CLKDivision = 16;
  ExtTiming.DataLatency = 17;
  ExtTiming.AccessMode = FSMC_ACCESS_MODE_A;

  if (HAL_SRAM_Init(&hsram1, &Timing, &ExtTiming) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FSMC_Init 2 */
  /* USER CODE END FSMC_Init 2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  AliyunIoT_RxCpltCallback(huart);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
