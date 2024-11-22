/**
 * @file config.h
 *
 *  Created on: @date Sep 26, 2024
 *      @author Jerry Ukwela (jeu6@case.edu)
 *      @brief 	Configures memory spaces and other boot settings
 *  @ingroup bootloader
 *  @note Order of parameters in flash is NNB, Checksum, 3x App Size, Checksum Clk
 *  @note SN and modType are stored in last 3 bytes of Boot Flash address space (0x08004FFD-0x08004FFF);
 */

#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

// Boot parameters
#define BOOT_VERSION		0x00
#define GIT_REVISION		0

#define NB_DATA_MAX			8 	///< Max number of bytes that can be read out at a time
#define WRP_ENABLE 			0	///< Selects whether or not write protection will be enabled on start of application (permanent)

#define BOOT_SIZE 			0x00005000	///< 20kB
#define EEPROM_PAGE_SIZE	1			///<  Size of the emulated EEPROM in pages
#define EEPROM_SIZE			EEPROM_PAGE_SIZE * FLASH_PAGE_SIZE ///<  Size of the EEPROM in bytes
#define MAX_APP_SIZE		FLASH_SIZE - BOOT_SIZE - (EEPROM_SIZE) - FLASH_PAGE_SIZE  ///< For bootloader in application space

// Flash configuration
#define APP_START_ADDR		0x08005000 ///< Start address of applicaiton in flash @note Flash addressing starts at 0x08000000
#define APP_END_ADDR		APP_START_ADDR + MAX_APP_SIZE - 1 ///< End address of application in flash

#define APP_PAGE_NUMBER		0x0A	///< Flash page that the application starts on
#define PARAM_PAGE_NUMBER 	0x7F	///< Flash page that the parameters start on
#define EEPROM_PAGE_NUMBER	PARAM_PAGE_NUMBER - EEPROM_PAGE_SIZE ///< Flash page that the EEPROM starts on
#define NUM_OF_PAGES		(128 - APP_PAGE_NUMBER - EEPROM_PAGE_SIZE - 1) ///< 128 pages for 256K according to reference RM0394 (2k per page), subtract space for EEPROM, subtract one for parameters

#define PAGE_DEFAULT		0
#define MEM_DEFAULT 		APP_START_ADDR

#define BOOT_START_ADDR     0x08000000								///< Start address of bootloader in flash @note Flash addressing starts at 0x08000000
#define BOOT_END_ADDR       BOOT_START_ADDR + BOOT_SIZE - 1			///< End address of bootloader in flash
#define EEPROM_START_ADDR	APP_END_ADDR + 1						///< Start address of EEPROM in flash
#define EEPROM_END_ADDR		EEPROM_START_ADDR + EEPROM_SIZE - 1		///< End address of EEPROM in flash
#define PARAM_START_ADDR	APP_END_ADDR + (EEPROM_SIZE) + 1		///< Start address of module parameters in flash
#define PARAM_END_ADDR    	PARAM_START_ADDR + FLASH_PAGE_SIZE - 1	///< End address of module parameters in flash
#define NNB_ADDR            PARAM_START_ADDR						///< Address in flash where the node number lives
#define CHECKSUM_ADDR       NNB_ADDR + 1							///< Address in flash where the checksum lives
#define TEST_LOCK_ADDR      BOOT_END_ADDR - 3						///< Address to check for write protection

// CAN identifiers
#define NNB_DEFAULT				0x73								///< Default node address if saved address is invalid
#define DEVICE_CAN_ID_DEFAULT	0x00 	// CRIS_DEFAULT
#define DEVICE_CAN_ID			0x14 	// CRIS

#define NNB           *(char *)(NNB_ADDR) 			///< Node Number
#define CHECKSUM      *(char *)(CHECKSUM_ADDR + 0)
#define APP_SIZE       (char *)(CHECKSUM_ADDR + 1)	///< 3 byte array
#define CHECKSUM_CLK  *(char *)(CHECKSUM_ADDR + 4)  ///<  Unused

// SN and modType stored in last 3 bytes of Boot Flash (0x08004FFD-0x08004FFF);
#define SN_HIGH       *(char *)(BOOT_END_ADDR - 2)	///< Reference to the serial number high byte as defined in the bootloader binary
#define SN_LOW        *(char *)(BOOT_END_ADDR - 1)	///< Reference to the serial number low byte as defined in the bootloader binary
#define MODTYPE       *(char *)(BOOT_END_ADDR - 0) 	///< Reference to the module type as defined in the bootloader binary

//Order of parameters in flash:
// NNB, Checksum, 3x App Size, Checksum Clk

#endif /* INC_CONFIG_H_ */
