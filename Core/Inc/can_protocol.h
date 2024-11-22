/**
 * @file can_protocol.h
 *
 *  Created on: @date Sep 26, 2024
 *      @author Jerry Ukwela (jeu6@case.edu)
 *      @brief  header file for can_protocol.c
 */

#ifndef INC_CAN_PROTOCOL_H_
#define INC_CAN_PROTOCOL_H_

#define MAX_OFFSET_ID 0x7F0
#define ERROR_SECURITY  0x00

// * Protocol Commands * //
#define CAN_ID_SELECT_NODE            0x00
#define CAN_ID_PROG_START             0x01
#define CAN_ID_PROG_DATA              0x02
#define CAN_ID_DISPLAY_DATA           0x03
#define CAN_ID_START_APP              0x04
#define CAN_ID_CHANGE_NODE            0x05
#define CAN_ID_SELECT_MEM_PAGE        0x06
#define CAN_ID_ERROR_SECURITY         0x06
#define CAN_ID_CALC_APP_CHECKSUM      0x08
#define CAN_ID_SET_APP_CHECKSUM       0x09
#define CAN_ID_DEFAULT_OD             0x0A
#define CAN_ID_BROADCAST_START_APP    0x0F

#define COMMAND_OK        0x00
#define COMMAND_FAIL      0x01
#define COMMAND_NEW_DATA  0x02
#define COMMAND_SECURITY  0x03

// Declare Functions
volatile void JumpToApplication();
extern void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);
extern void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan);
extern uint8_t enableWRP();

// Declare Variables
extern CAN_TxHeaderTypeDef  TxHeader;
extern CAN_RxHeaderTypeDef  RxHeader;
extern uint8_t              TxData[8];
extern uint8_t              RxData[8];
extern uint32_t             TxMailbox;
extern CAN_HandleTypeDef	hcan1;
extern char 				f_communication_open; 	//0 = Closed, 1 = Open and Unique
extern char 				f_frame_to_send;
extern uint32_t				pagenum;
extern uint16_t				offset_id_copy;			// Working copy of offset ID
extern char					nnb_copy;				// Working copy of Node ID
extern uint32_t  			nb_byte_to_write;
extern uint32_t  			nb_byte_to_read;
extern uint32_t   	  		pt_buffer;
extern uint32_t 			address;
extern uint32_t 			start_address;
extern uint32_t				end_address;
extern uint16_t				chksum;
extern uint32_t				sizeApp;
extern char					buffer[64];
extern volatile uint8_t 	f_can_jump_to_app;

#endif /* INC_CAN_PROTOCOL_H_ */
