/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "can_protocol.h"
#include "stm32l4xx_hal_flash_ex.h"
#include "config.h"

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
/* USER CODE BEGIN PV */
FLASH_OBProgramInitTypeDef 	flashOB = {0}; ///< Flash option bytes for setting write protection
CAN_HandleTypeDef			hcan1;

CAN_TxHeaderTypeDef   TxHeader;
CAN_RxHeaderTypeDef   RxHeader;
uint8_t               TxData[8];
uint8_t               RxData[8];
uint32_t              TxMailbox;

uint16_t	offset_id_copy;	///< Working copy of offset ID
char		nnb_copy;		///< Working copy of Node ID

char 		f_communication_open; ///< 0 = Closed, 1 = Open and Unique
char 		f_frame_to_send;

uint32_t  	nb_byte_to_write = 0;
uint32_t  	nb_byte_to_read = 0;

uint32_t	pagenum;
uint32_t   	pt_buffer;

uint32_t	start_address 	= 0x08000000;
uint32_t	address 		= 0x08000000;
uint32_t	end_address 	= 0x08000000;

uint16_t	chksum = 0;
uint32_t	sizeApp;

char buffer[64] = {0};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief Enables write protection over bootloader
  * @retval uint8_t success
  * @ingroup bootloader
  */
uint8_t enableWRP(){

	// Setup write areas
	flashOB.WRPArea = OB_WRPAREA_BANK1_AREAA;	// These must be assigned to return full OB set
	flashOB.PCROPConfig = FLASH_BANK_1;

	// Get current configuration to modify
	HAL_FLASHEx_OBGetConfig(&flashOB);

	if(flashOB.RDPLevel == OB_RDP_LEVEL_2 || !WRP_ENABLE) // If WRP already enabled, go to app
	{
		return 1;
	}

	// Setup write protection on the booloader
	flashOB.WRPStartOffset 	= 0;
	flashOB.WRPEndOffset 	= APP_PAGE_NUMBER - 1;
	flashOB.RDPLevel 		= OB_RDP_LEVEL_2;

	if (HAL_FLASH_Unlock() != HAL_OK){		// Unlock flash
		return 0;
	}

	if( HAL_FLASH_OB_Unlock() != HAL_OK){	// Unlock Option Bytes
		return 0;
	}

	if (HAL_FLASHEx_OBProgram(&flashOB) != HAL_OK){		// Program Option Bytes into flash
		return 0;
	}

	if (HAL_FLASH_OB_Launch() != HAL_OK ){	// Enable new OB settings
		return 0;
	}

	HAL_FLASH_OB_Lock();
	HAL_FLASH_Lock();

	return 1;
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
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  if(f_can_jump_to_app){
		  if(WRP_ENABLE){
			  if(!enableWRP()){
				  f_can_jump_to_app = 0;
				  continue;
			  }
		  }

		  JumpToApplication();
	  }
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_8;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV4;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

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
  hcan1.Init.Prescaler = 4;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_3TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_6TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_3TQ;
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

  CAN_FilterTypeDef canFilterConfig;
  //canFilterConfig.FilterNumber = 0;
  canFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  canFilterConfig.FilterIdHigh = 0x0000;
  canFilterConfig.FilterIdLow = 0x0000;
  canFilterConfig.FilterMaskIdHigh = 0x0000;
  canFilterConfig.FilterMaskIdLow = 0x0000;
  canFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  canFilterConfig.FilterActivation = ENABLE;
  canFilterConfig.FilterBank = 0;
  canFilterConfig.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan1, &canFilterConfig) != HAL_OK){Error_Handler();}

  f_communication_open = 0;
  f_frame_to_send = 0;

  offset_id_copy = (DEVICE_CAN_ID<< 4);
  if (offset_id_copy > MAX_OFFSET_ID)
  {
	  offset_id_copy = 0;
  }

  // Retrieve Node information
  nnb_copy = NNB;
  if(nnb_copy > 0x7F || nnb_copy == 0) //node number must be between 1-127
	nnb_copy = NNB_DEFAULT;

  if (HAL_CAN_Start(&hcan1) != HAL_OK){Error_Handler();}

  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK){Error_Handler();}
  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_LAST_ERROR_CODE) != HAL_OK){Error_Handler();}
  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_ERROR) != HAL_OK){Error_Handler();}
  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_BUSOFF) != HAL_OK){Error_Handler();}
  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_ERROR_WARNING) != HAL_OK){Error_Handler();}
  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_ERROR_PASSIVE) != HAL_OK){Error_Handler();}

  // Configure additional transmit properties
  TxHeader.StdId = nnb_copy; //Standard ID
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 2; // Length of frame
  TxHeader.TransmitGlobalTime = DISABLE;

  /* USER CODE END CAN1_Init 2 */
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

#ifdef  USE_FULL_ASSERT
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
