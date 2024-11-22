/**
 * @file can_protocol.c
 *
 *  Created on: @date Sep 26, 2024
 *      @author Jerry Ukwela (jeu6@case.edu)
 *      @brief 	Protocol switch for ISP commands. Modified for NNP.
 *
 *  @defgroup FESCAN FESCAN
 *  @defgroup bootloader Bootloader
 */

#include "config.h"
#include "main.h"
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_can.h"
#include "can_protocol.h"
#include <string.h>

// Private typedef
typedef void (*pFunction)(void);

// Private define
#define LE16(a) (((uint16_t)((a) & 0x00FF) << 8) | (((uint16_t)(a) & 0xFF00) >> 8)) //Convert to network endian-ness

/**
 * @brief Structure to convert raw data to parameter set
 * @ingroup bootloader
 */
union convertParam{
	uint64_t  rawData;				///< Raw data from flash
    struct{
        char       nnb;            	///< Node ID
        char       checksum;		///< Checksum
        char       as1;            	///< App Size LSB
        char       as2;            	///< App Size Middle Byte
        char       as3;            	///< App Size MSB
        char       checksum_clk; 	///< Checksum Clock (Unused)
        char       unused[2];    	///< Padding for 64 bits
    } parameter;
};

/**
 * @brief Union structure to convert between words,
 * 		  half words, and quarter words
 * @ingroup bootloader
 */
union Union32{
	uint32_t 	word;
	uint16_t 	hword[2];
	uint8_t 	qword[4];
} u32;

// Private variables
static FLASH_EraseInitTypeDef eraseInitStruct;

volatile pFunction	jumpAddress;
union convertParam 	moduleParameters = {0};
volatile uint8_t 	f_can_jump_to_app = 0;

/**
 * @brief Configure CAN message to send when SELECT_MEM_PAGE is used
 * @ingroup FESCAN
 */
void send_error_security (void)
{
	f_frame_to_send = 1;
	TxHeader.DLC = 1;
	TxHeader.StdId = CAN_ID_ERROR_SECURITY + offset_id_copy;
	TxData[0] = ERROR_SECURITY;
}

/**
 * @brief 	Initializes global address range from received CAN message
 * 			in preparation for can_display_data
 * @ingroup FESCAN bootloader
 */
void can_init_display_data(void){
 	start_address     	= RxData[1] << 8 | RxData[2];
	end_address 		= RxData[3] << 8 | RxData[4];

	nb_byte_to_read 	= (uint32_t)(end_address - start_address) + 1;
	start_address += APP_START_ADDR + (pagenum * 0x10000);
	end_address += APP_START_ADDR + (pagenum * 0x10000);
	address = start_address;
}

/**
 * @brief Initializes global address range from received CAN message
 *        in preparation for can_write_data
 * @ingroup FESCAN bootloader
 */
void can_init_write_data(void)
{
 	start_address     		= RxData[1] << 8 | RxData[2];
	end_address 			= RxData[3] << 8 | RxData[4];

	nb_byte_to_write 	= (uint32_t)(end_address - start_address) + 1;

	if(nb_byte_to_write % 8 != 0) // Divisible by 8 check
		send_error_security();

	start_address += APP_START_ADDR + (pagenum * 0x10000);
	end_address += APP_START_ADDR + (pagenum * 0x10000);
	address = start_address;
	memset(buffer, 0xFF, sizeof buffer);
	pt_buffer = 0;
}

/**
 * @brief Displays requested data over the CAN network
 * @ingroup FESCAN bootloader
 */
void can_display_data (void)
{
	uint16_t cpt;
	uint16_t state;

	do {
		if(nb_byte_to_read >= NB_DATA_MAX)
		{
			for (cpt = 0; cpt < NB_DATA_MAX; cpt++, address++)
			{
				state = *(uint8_t *) address;
				if((state >> 8))
				{
					send_error_security();
					return;
				}
				TxData[cpt] = state & 0x00FF;
			}
			nb_byte_to_read -= NB_DATA_MAX;
		}
		else
		{
			for (cpt = 0; cpt < nb_byte_to_read; cpt++, address++)
			{
				state = *(uint8_t *) address;
				if((state >> 8))
				{
					send_error_security();
					return;
				}
				TxData[cpt] = state & 0x00FF;
			}
			nb_byte_to_read = 0;
		}

		TxHeader.StdId = RxHeader.StdId + 0x10;
		TxHeader.DLC = cpt;

		while(HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) != 3); // Ensure bytes sent in order

	    if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) != HAL_OK)
	    {
	  	    Error_Handler();
	    }

	    f_frame_to_send = 0;
	    memset(TxData, 0, sizeof TxData); // Reset array to all 0;
	    TxHeader.DLC = 0;

	} while (nb_byte_to_read);
}

/**
 * @brief Copy data to write to flash into a buffer (max of 64 bytes).
 * 		  When nb_byte_to_write reaches 0, commit data to flash
 * @retval char Response to CAN network
 * @ingroup FESCAN bootloader
 */
char can_write_data(void)
{
  char status = COMMAND_SECURITY;

  if((address + nb_byte_to_write) > APP_END_ADDR) // Make sure the application doesn't overwrite EEPROM or parameter space
	  return (status);

  HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn); // Disable CAN interrupt until write completes

  // New Implementation (Doesn't require cpt_var)
  // Number of bytes to write should never exceecd the size of the buffer
  if(nb_byte_to_write)
  {
	  memcpy(&buffer[pt_buffer], RxData, RxHeader.DLC);
	  pt_buffer += RxHeader.DLC;
	  nb_byte_to_write -= RxHeader.DLC;
  }

  if (nb_byte_to_write)
  {
    status = COMMAND_NEW_DATA;
  }
  else if (nb_byte_to_write == 0 && pt_buffer != 0)
  {
	  HAL_FLASH_Unlock();

	  for (int i = 0; i < pt_buffer; i += 8) // Program into flash in double word increments (8 bytes)
	  {
		  if (HAL_FLASH_Program(TYPEPROGRAM_DOUBLEWORD, address + i, *(uint64_t *)&buffer[i]) != HAL_OK)
		  {
			  status = COMMAND_SECURITY;
		  }
		  else
		  {
			  status = COMMAND_OK;
		  }
	  }

	  HAL_FLASH_Lock(); // Re-lock the flash afterwards

	  memset(buffer, 0xFF, sizeof buffer); // Clear the buffer
  }

  HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn); // Enable CAN interrupt
  return (status);
}

/**
 * @brief Checks for first blank location of flash between two addresses,
 * 		  and returns that location, otherwise, return 0
 * @param start start address
 * @param end	end address
 * @retval uint32_t Address of first non-empty location, 0 if none
 * @ingroup bootloader
 */
uint32_t blank_flash_check(uint32_t start, uint32_t end){
	do
	{
		if(*(char *) start != 0xFF){
			return (uint32_t) (start) & 0x0000FFFF;
		}
	} while (start++ < end);

	return 0;
}

/**
 * @brief Calculates a checksum across application code
 * @param size length of application code to sum over
 * @retval uint8_t Checksum
 * @ingroup bootloader
 */
uint8_t calc_app_checksum(uint32_t size)
{
	char *data;
	uint8_t sum = 0;

	//Do Checksum
	data = (char *) APP_START_ADDR;

	while(size--)
	{
	  sum += *data++;
	}

	return sum;
}

/**
 * @brief Transfer control of microcontroller to user application after
 * 		  deinitializing peripherals and moving relevant pointers
 * @note  If APP_SIZE is defined in the parameter space, and the checksum matches what is calculated,
 * 		  then the application starts, otherwise if size is 0 or greater than MAX_APP_SIZE, the check is overridden.
 * @note  As per AN2606, CAN interface may not work correctly if RTC is not reset in application
 *  	  prior to loading bootloader, else a power cycle is required.
 * @ingroup bootloader
 */
volatile void JumpToApplication()
{
	f_can_jump_to_app = 0;
	sizeApp = (uint32_t)(*(APP_SIZE))<<16 | (uint32_t)(*(APP_SIZE + 1))<<8 | *(APP_SIZE + 2);
	if( sizeApp > 0 && sizeApp <= MAX_APP_SIZE )
	{
	  chksum = calc_app_checksum(sizeApp);
	}

	if(sizeApp == 0 || sizeApp == 0x00FFFFFF || chksum == CHECKSUM)
	{
		if(((*(uint32_t*) APP_START_ADDR) & 0x2FFE0000) == 0x20000000){ // If there's actually an application loaded
			volatile uint32_t jA = *(__IO uint32_t*)(APP_START_ADDR + 0x04);
			jumpAddress = (__IO pFunction) jA;

			// Release peripherals before jumping into application code
			HAL_CAN_DeactivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
			HAL_CAN_DeactivateNotification(&hcan1, CAN_IT_LAST_ERROR_CODE);
			HAL_CAN_DeactivateNotification(&hcan1, CAN_IT_ERROR);
			HAL_CAN_DeactivateNotification(&hcan1, CAN_IT_BUSOFF);
			HAL_CAN_DeactivateNotification(&hcan1, CAN_IT_ERROR_WARNING);
			HAL_CAN_DeactivateNotification(&hcan1, CAN_IT_ERROR_PASSIVE);
			HAL_CAN_Stop(&hcan1);
			HAL_CAN_DeInit(&hcan1);
			HAL_RCC_DeInit();
			HAL_DeInit();

			SCB->VTOR = (__IO uint32_t)APP_START_ADDR;		// Transfer vector table to the applcation
			__set_MSP(*(__IO uint32_t*) APP_START_ADDR);	// Set main stack pointer to point to application

			jumpAddress();									// Jump to app
		}
	}
	else
	{
		TxData[0] = chksum;
		TxData[1] = CHECKSUM;
		TxData[2] = *APP_SIZE;
		TxData[3] = *(APP_SIZE + 1);
		TxData[4] = *(APP_SIZE + 2);
		TxData[5] = CHECKSUM_CLK;
		TxHeader.DLC = 6;

		TxHeader.StdId = RxHeader.StdId + 0x10;

		if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) != HAL_OK)
		{
			Error_Handler();
		}

		f_frame_to_send = 0;
		memset(TxData, 0, sizeof TxData); // Reset array to all 0;
		TxHeader.DLC = 0;
	} //if (sizeApp == 0 || sizeApp == 0x00FFFFFF || chksum == CHECKSUM)
}

/**
 * @brief CAN message callback that handles errors with communication
 * @ingroup FESCAN
 */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
	Error_Handler();
}

/**
 * @brief CAN message callback that handles communication with the network
 * @ingroup FESCAN
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  /* Get RX message */
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) {Error_Handler();}

  // ISP Implementation
  switch(RxHeader.StdId - offset_id_copy){

  	  case CAN_ID_SELECT_MEM_PAGE:
  		  if (f_communication_open) {
  			//RX CAN message:
			//byte 0: 0x01 memory, 0x02 page, 0x03 both
			//byte 1: memory
			//byte 2: page

			send_error_security();
			if (RxData[0] & 0x01)
			{
				address = RxData[1];
				address += APP_START_ADDR;
			}

			if (RxData[0] & 0x02)
			{
				pagenum = RxData[2];
				if(pagenum > 3)
					pagenum = 3;
			}
  		  } // if (f_communication_open)
  		  break;

  	  case CAN_ID_PROG_START:
  		  if (f_communication_open) {
			//RX CAN message:
			//byte 0: 0x80 erase, otherwise write
			//byte 1-2: start address
			//byte 3-4: end address

  			  f_frame_to_send = 1;
  			  TxHeader.DLC = 0x00;

  			  can_init_write_data(); //set addresses for writing or erasing

  			  if (RxData[0]==0x80) {        // Erase selected memory
				HAL_FLASH_Unlock();

				uint32_t PageError = 0;

				eraseInitStruct.TypeErase 	= TYPEERASE_PAGES;	// Not a full erase, want to keep the parameter section of flash
				eraseInitStruct.Page 		= APP_PAGE_NUMBER;
				eraseInitStruct.NbPages 	= NUM_OF_PAGES; // Erase application section of flash

				if (HAL_FLASHEx_Erase(&eraseInitStruct, &PageError) != HAL_OK) {
					send_error_security();
					Error_Handler();
				} else {
					TxHeader.DLC 	= 0x01;
					TxData[0] 		= 0x00;
				}

				HAL_FLASH_Lock(); // Re-lock the flash afterwards
  			  }
  		  }// if (f_communication_open)
  		  break;

  	  case CAN_ID_PROG_DATA:
  		  if (f_communication_open) {
  			//RX CAN message:
			//byte 0-7: data

			f_frame_to_send = 1;
			TxHeader.DLC = 0x01;

			// now that the address is in place, write the data from the can_data[]
			// and provide status back.
			TxData[0] = can_write_data();
  		  } // if (f_communication_open)
  		  break;

  	  case CAN_ID_DISPLAY_DATA:
  		  if (f_communication_open){
  			//RX CAN message:
			//byte 0: 0x00 to read, 0x80 to blank check
			if (RxData[0] == 0x00)
			{
			  can_init_display_data();
			  can_display_data();
			}
			else if (RxData[0] == 0x80)
			{
			  can_init_display_data();
			  u32.word = blank_flash_check(start_address, end_address); // Address is relative to the start of flash addressing (0x08000000)
			  if (u32.word){ // Blank check failed
				*(uint16_t*)&TxData[0] = LE16(u32.hword[0]);
				TxHeader.DLC = 0x02;
			  } else {
				TxHeader.DLC = 0x00;
			  }

			  f_frame_to_send = 1;
			}
  		  } // if (f_communication_open)
  		  break;

  	  case CAN_ID_SELECT_NODE:
  		  //RX CAN message:
		  //byte 0: Node
		  //byte 1-2: Serial Number or 0x0000 to get node Info
		  if (RxData[0] == nnb_copy)
		  {
			if ( RxData[1] == 0 && RxData[2] == 0 )
			{
			  //don't do anything with node, just send info in response
			}
			else if( RxData[1] == SN_HIGH && RxData[2] == SN_LOW )
			{
			  if (f_communication_open == 0)
			  {
				f_communication_open = 1;  //open Node
				address = MEM_DEFAULT;
				pagenum = PAGE_DEFAULT;
			  }
			  else
				f_communication_open = 0;  //close Node
			}
			else //message was intended for another module, don't send response
			  break;

			f_frame_to_send = 1;
			TxData[0] = BOOT_VERSION;
			TxData[1] = f_communication_open;
			TxData[2] = SN_LOW;
			TxData[3] = SN_HIGH;
			TxData[4] = GIT_REVISION;
			TxData[5] = MODTYPE;
			TxData[6] = nnb_copy;
			TxHeader.DLC = 7;

		  }// if (RxData[0] == nnb_copy)
  		  break;

  	  case CAN_ID_CHANGE_NODE:
  		  if (f_communication_open) {
			//RX CAN message:
			//byte 0: New Node
			f_frame_to_send = 1;
			TxHeader.DLC = 0x01;

			nnb_copy = RxData[0];

			if( nnb_copy > 0 && nnb_copy <= 0x7F )
			{
			  uint64_t* paramDWORD = (uint64_t * ) (PARAM_START_ADDR);

			  moduleParameters.rawData = *paramDWORD;
			  moduleParameters.parameter.nnb = nnb_copy;

			  HAL_FLASH_Unlock();

			  uint32_t PageError = 0;

			  eraseInitStruct.TypeErase = TYPEERASE_PAGES;	// Just erase one page
			  eraseInitStruct.Page 		= PARAM_PAGE_NUMBER; // Last page in flash, where parameters are stored
			  eraseInitStruct.NbPages 	= 1;

			  if (HAL_FLASHEx_Erase(&eraseInitStruct, &PageError) != HAL_OK) {Error_Handler();}

			  if (HAL_FLASH_Program(TYPEPROGRAM_DOUBLEWORD, PARAM_START_ADDR, moduleParameters.rawData) != HAL_OK){Error_Handler();}

			  HAL_FLASH_Lock(); // Re-lock the flash afterwards
			}

			nnb_copy = NNB; //copy node number from Flash to active program

			TxData[0] = nnb_copy; //return new node number

		  }// if (f_communication_open)
  		  break;

  	  case CAN_ID_START_APP:
          if (f_communication_open) {
              //RX CAN message:
              //byte 0: "0x03"
              //byte 1: 0x80 to start App, 0x00 to Reset
              //byte 2-3: address to Start (0x0000)
              if (RxData[0] == 0x03) // Start application
              {
            	  if(RxData[1] == 0x80)
            		  f_can_jump_to_app = 1;
            	  else if(RxData[1] == 0x00)
            		  HAL_NVIC_SystemReset();
              }
          }// if (f_communication_open)
  		  break;

  	  case CAN_ID_BROADCAST_START_APP:
		  //RX CAN message:
		  //byte 0: "0x03"
		  //byte 1: 0x80 to start App, 0x00 to Reset
		  //byte 2-3: address to Start (0x0000)
		  if (RxData[0] == 0x03) // Start application
		  {
			  if(RxData[1] == 0x80)
				  f_can_jump_to_app = 1;
			  else if(RxData[1] == 0x00)
				  HAL_NVIC_SystemReset();
		  }
  		  break;

  	  case CAN_ID_CALC_APP_CHECKSUM:
		  if (f_communication_open)
		  {
			f_frame_to_send = 1;

			//RX CAN message:
			//byte 0-2: App size
			sizeApp = (uint32_t)RxData[0] << 16 |  (uint32_t)RxData[1] << 8 | RxData[2];
			if( sizeApp > 0 && sizeApp <= MAX_APP_SIZE )
			{
			  chksum = calc_app_checksum(sizeApp);

			  TxData[0] 	= chksum;
			  TxData[1] 	= CHECKSUM;
			  TxData[2] 	=  *APP_SIZE;
			  TxData[3] 	=  *(APP_SIZE + 1);
			  TxData[4] 	=  *(APP_SIZE + 2);
			  TxData[5] 	=  CHECKSUM_CLK;
			  TxHeader.DLC 	= 0x06;
			}
			else
			{
			  TxData[0] = 0;
			  TxHeader.DLC  = 0x01;
			}
		  } // if (f_communication_open)
  		break;

  	  case CAN_ID_SET_APP_CHECKSUM:
  		  if (f_communication_open)
		  {
			f_frame_to_send = 1;

			//RX CAN message:
			//byte 0: CheckSum
			//byte 1-3: App size
			//byte 4: Clk
			sizeApp = (uint32_t)RxData[1]<<16 | (uint32_t)RxData[2]<<8 | RxData[3];
			if( sizeApp > 0 && sizeApp <= MAX_APP_SIZE ) {

			  uint64_t* paramDWORD = (uint64_t * ) PARAM_START_ADDR;

			  moduleParameters.rawData = *paramDWORD;
			  moduleParameters.parameter.checksum = RxData[0];
			  moduleParameters.parameter.as1 = RxData[3];
			  moduleParameters.parameter.as2 = RxData[2];
			  moduleParameters.parameter.as3 = RxData[1];
			  moduleParameters.parameter.checksum_clk = RxData[4];

			  HAL_FLASH_Unlock();

			  uint32_t PageError = 0;

			  eraseInitStruct.TypeErase = TYPEERASE_PAGES;	// Just erase one page
			  eraseInitStruct.Page 		= PARAM_PAGE_NUMBER; // Last page in flash, where parameters are stored
			  eraseInitStruct.NbPages 	= 1;

			  if (HAL_FLASHEx_Erase(&eraseInitStruct, &PageError) != HAL_OK) {Error_Handler();}

			  HAL_FLASH_Program(TYPEPROGRAM_DOUBLEWORD, PARAM_START_ADDR, moduleParameters.rawData);

			  HAL_FLASH_Lock(); // Re-lock the flash afterwards

			  //return values from Flash
			  TxData[0] =  CHECKSUM;
			  TxData[1] =  *APP_SIZE;
			  TxData[2] =  *(APP_SIZE + 1);
			  TxData[3] =  *(APP_SIZE + 2);
			  TxData[4] =  CHECKSUM_CLK;

			  TxHeader.DLC = 0x05;
			}
			else
			{
			  TxData[0] = 0;
			  TxHeader.DLC = 1;
			}
		  }// if (f_communication_open)
  		break;

  	  case CAN_ID_DEFAULT_OD:
  		  if (f_communication_open) {

			f_frame_to_send = 1;
			TxHeader.DLC = 0x00;

			HAL_FLASH_Unlock();

			uint32_t PageError = 0;

			//Erase the emulated EEPROM
			eraseInitStruct.TypeErase 	= TYPEERASE_PAGES;			// Just erase pages
			eraseInitStruct.Page 		= 127 - EEPROM_PAGE_SIZE; 	// Erase the EEPROM pages, leaving parameters untouched
			eraseInitStruct.NbPages 	= EEPROM_PAGE_SIZE;

			if (HAL_FLASHEx_Erase(&eraseInitStruct, &PageError) != HAL_OK) {}

			HAL_FLASH_Lock(); // Re-lock the flash afterwards

		  } // if (f_communication_open)
  		break;

  	  default:
  		break;

  } // switch (RxHeader.StdId - offset_id_copy)

  if (f_frame_to_send)
  {
	  TxHeader.StdId = RxHeader.StdId + 0x10; // CRIS + 0x10 + Code

	  if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) != HAL_OK)
	  {
	  	  Error_Handler();
	  }

	  f_frame_to_send = 0;
	  memset(TxData, 0, sizeof TxData); // Reset array to all 0;
	  TxHeader.DLC = 0;
  } // if (f_frame_to_send)
} // void HAL_CAN_RxFifo0MsgPendingCallback



