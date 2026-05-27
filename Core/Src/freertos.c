/* USER CODE BEGIN Header */
#include "driver_led.h"
#include "driver_lcd.h"
#include "driver_mpu6050.h"
#include "driver_timer.h"
#include "driver_ds18b20.h"
#include "driver_dht11.h"
#include "driver_active_buzzer.h"
#include "driver_passive_buzzer.h"
#include "driver_color_led.h"
#include "driver_ir_receiver.h"
#include "driver_ir_sender.h"
#include "driver_light_sensor.h"
#include "driver_ir_obstacle.h"
#include "driver_ultrasonic_sr04.h"
#include "driver_spiflash_w25q64.h"
#include "driver_rotary_encoder.h"
#include "driver_motor.h"
#include "driver_key.h"
#include "driver_uart.h"

/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app.h"
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
/* USER CODE BEGIN Variables */

/* 队列句柄 (�? app.h �? extern 声明) */
QueueHandle_t xSensorQueue;
QueueHandle_t xStorageQueue;
QueueHandle_t xKeyQueue;
QueueHandle_t xUartTxQueue;

/* 事件组句�? */
EventGroupHandle_t xAlarmEventGroup;

/* SPI Flash 互斥�? (防止 DisplayTask �? StorageTask 同时访问) */
SemaphoreHandle_t xSpiMutex;

/* 任务句柄 (用于健康监控) */
TaskHandle_t xSensorTaskHandle;
TaskHandle_t xKeyTaskHandle;
TaskHandle_t xDisplayTaskHandle;
TaskHandle_t xAlarmTaskHandle;
TaskHandle_t xStorageTaskHandle;
TaskHandle_t xCommsTaskHandle;

/* 报警阈�?�运行时变量 (�? app.h �? extern 声明) */
int g_tempHighThreshold = 35;
int g_tempLowThreshold = 10;
int g_distMinThreshold = 10;
int g_humHighThreshold = 80;
int g_humLowThreshold = 20;
int g_lightLowThreshold = 500;

uint8_t g_alarmModes[ALARM_TYPE_COUNT] = {1, 1, 2, 1, 1, 2};

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* 创建队列 */
  xSensorQueue  = xQueueCreate(3, sizeof(sensor_data_t));
  xStorageQueue = xQueueCreate(5, sizeof(sensor_data_t));
  xKeyQueue     = xQueueCreate(5, sizeof(key_cmd_t));
  xUartTxQueue  = xQueueCreate(3, sizeof(sensor_data_t));
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  xTaskCreate(SensorTask,   "Sensor",   256, NULL, 2, &xSensorTaskHandle);
  xTaskCreate(KeyTask,      "Key",      128, NULL, 2, &xKeyTaskHandle);
  xTaskCreate(DisplayTask,  "Display",  256, NULL, 1, &xDisplayTaskHandle);
  xTaskCreate(AlarmTask,    "Alarm",    128, NULL, 3, &xAlarmTaskHandle);
  xTaskCreate(StorageTask,  "Storage",  256, NULL, 1, &xStorageTaskHandle);
  xTaskCreate(CommsTask,    "Comms",    128, NULL, 1, &xCommsTaskHandle);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* 创建事件�? */
  xAlarmEventGroup = xEventGroupCreate();

  /* 创建 SPI 互斥�? */
  xSpiMutex = xSemaphoreCreateMutex();
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */

/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* defaultTask 已不再使�?, 保留此函数仅用于 CubeMX 兼容�? */
  /* 实际业务逻辑�? SensorTask / KeyTask / DisplayTask / AlarmTask / StorageTask �? */
  for(;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

