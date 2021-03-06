/*
 * task_xbee.c
 *
 *  Created on: Jun 15, 2020
 *      Author: Jonas
 */
#include "tasks/task_xbee.h"

command_xbee_t local_command_xbee;

command_e local_command_rx;
command_e local_command;
uint8_t buffer [1];
bool new_command = false;
bool buzzer_on_fsm = false;
bool buzzer_on_telemetry = false;

int uart_counter = 0;


uint8_t calculate_checksum(telemetry_t *cnf);

void vTaskXbee(void *argument) {
	/* For periodic update */
	uint32_t tick_count, tick_update_slow, tick_update_fast;

	/* local Data */
	state_est_data_t state_est_data;
	sb_data_t local_sb_data;

	/* Telemetry struct */
	telemetry_t telemetry_send = { 0 };
	telemetry_send.flight_phase = IDLE;
	telemetry_send.startbyte = 0x17;


	/* Command struct */
	local_command_rx = IDLE_COMMAND;
	local_command = IDLE_COMMAND;

	uint8_t buzzercounter = 0;

	osDelay(700);
	HAL_GPIO_WritePin(PW_HOLD_GPIO_Port, PW_HOLD_Pin, GPIO_PIN_SET);

	/* Camera Variables */
	uint32_t camera_counter = 0;
	bool camera_enabled = false;
	bool camera_wait = false;
	bool camera_trigger = false;
	bool camera_ready = false;


	/* Infinite loop */

	tick_update_slow = osKernelGetTickFreq() / XBEE_SAMPLING_FREQ;
	tick_update_fast = osKernelGetTickFreq() / XBEE_SAMPLING_FREQ_HIGH;
	bool fast_sampling = false;
	tick_count = osKernelGetTickCount();


	while (1) {
		/* Tick Update */
		if(fast_sampling){
			tick_count += tick_update_fast;
		}
		else{
			tick_count += tick_update_slow;
		}
		/* Read Command */
		HAL_UART_Receive_IT(&huart7, (uint8_t*)&local_command_rx, 1);
		UsbPrint("[Telemetry] ts: %u, Received Commmand: %u, Rx_buffer; %u\n",
				telemetry_send.ts, local_command, local_command_rx);

		if(AcquireMutex(&command_mutex) == osOK){
			global_telemetry_command = local_command;
			ReleaseMutex(&command_mutex);
		}

		/* Check if we need to go to fast sampling */
		if(local_command == TELEMETRY_HIGH_SAMPLING){
			fast_sampling = true;
		}

		/* Check if we need to go to low sampling */
		if(local_command == TELEMETRY_LOW_SAMPLING){
			fast_sampling = false;
		}

		/* Enable Buzzer trough Telemetry */
		if(local_command == ENABLE_BUZZER){
			buzzer_on_telemetry = !buzzer_on_telemetry;
		}

		/* Enable Buzzer Trough FSM */
		if(telemetry_send.flight_phase == RECOVERY){
			buzzer_on_fsm = true;
		}

		/* Enable Self Power Hold */
		if(local_command == TELEMETRY_HIGH_SAMPLING){
			HAL_GPIO_WritePin(PW_HOLD_GPIO_Port, PW_HOLD_Pin, GPIO_PIN_RESET);
		}

		/* Go Back to Low Sampling if we are in Recovery */
		if(telemetry_send.flight_phase == RECOVERY){
			fast_sampling = false;
		}

		/* Disable Self Power Hold */
		if(local_command == DISABLE_SELF_HOLD){
			HAL_GPIO_WritePin(PW_HOLD_GPIO_Port, PW_HOLD_Pin, GPIO_PIN_SET);
		}

		/* reset command */
		if(new_command){
			local_command = IDLE_COMMAND;
			new_command = false;
		}

		/* Start Enable Camera Sequence */
		if((local_command == ENABLE_CAMERA) && !camera_enabled){
			camera_enabled = true;
			HAL_GPIO_WritePin(CAMERA1_GPIO_Port, CAMERA1_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(CAMERA2_GPIO_Port, CAMERA2_Pin, GPIO_PIN_SET);
			camera_counter = osKernelGetTickCount() + CAMERA_ON;
		}

		if((camera_counter > osKernelGetTickCount()) && !camera_wait){
			camera_wait = true;
			HAL_GPIO_WritePin(CAMERA1_GPIO_Port, CAMERA1_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(CAMERA2_GPIO_Port, CAMERA2_Pin, GPIO_PIN_RESET);
			camera_counter = osKernelGetTickCount() + CAMERA_WAIT;
		}

		if((camera_counter > osKernelGetTickCount()) && !camera_trigger){
			camera_trigger = true;
			HAL_GPIO_WritePin(CAMERA1_GPIO_Port, CAMERA1_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(CAMERA2_GPIO_Port, CAMERA2_Pin, GPIO_PIN_SET);
			camera_counter = osKernelGetTickCount() + CAMERA_TRIGGER;
		}

		if((camera_counter > osKernelGetTickCount()) && !camera_ready){
			camera_ready = true;
			HAL_GPIO_WritePin(CAMERA1_GPIO_Port, CAMERA1_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(CAMERA2_GPIO_Port, CAMERA2_Pin, GPIO_PIN_RESET);
		}

		/* Camera first enable for some time, then turn off and finally turn on again */


		/* Enable Buzzer */
		if(buzzer_on_fsm ^ buzzer_on_telemetry){
			if(fast_sampling){
				if(buzzercounter > (400/tick_update_fast)){
					HAL_GPIO_TogglePin(BUZZER_GPIO_Port, BUZZER_Pin);
					buzzercounter = 0;
				}
			}
			else{
				if(buzzercounter > (400/tick_update_slow)){
					HAL_GPIO_TogglePin(BUZZER_GPIO_Port, BUZZER_Pin);
					buzzercounter = 0;
				}
			}
		}
		else if(osKernelGetTickCount() > 5000){
			HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
		}
		buzzercounter++;

		/* Read Sensor Board Data */
		ReadMutex(&sb1_mutex, &sb1_baro, &local_sb_data.baro, sizeof(sb1_baro));
		ReadMutex(&sb1_mutex, &sb1_imu, &local_sb_data.imu, sizeof(sb1_imu));

		telemetry_send.sb_data.pressure = local_sb_data.baro.pressure;
		telemetry_send.sb_data.temperature = local_sb_data.baro.temperature;
		telemetry_send.sb_data.acc_x = local_sb_data.imu.acc_x;
		telemetry_send.sb_data.acc_y = local_sb_data.imu.acc_y;
		telemetry_send.sb_data.acc_z = local_sb_data.imu.acc_z;
		telemetry_send.sb_data.gyro_x = local_sb_data.imu.gyro_x;
		telemetry_send.sb_data.gyro_y = local_sb_data.imu.gyro_y;
		telemetry_send.sb_data.gyro_z = local_sb_data.imu.gyro_z;


		/* Read Control Data*/
		ReadMutex(&state_est_mutex, &state_est_data_global, &state_est_data, sizeof(state_est_data));

		ReadMutex(&motor_mutex, &global_airbrake_extension, &telemetry_send.airbrake_extension, sizeof(global_airbrake_extension));

		ReadMutex(&fsm_mutex, &global_flight_phase_detection.flight_phase, &telemetry_send.flight_phase, sizeof(global_flight_phase_detection.flight_phase));

		/* read GPS */
		ReadMutex(&gps_mutex, &globalGPS, &telemetry_send.gps, sizeof(globalGPS));

		/* read Battery */
		ReadMutex(&battery_mutex, &global_battery_data, &telemetry_send.battery, sizeof(global_battery_data));


		telemetry_send.height = state_est_data.position_world[2];
		telemetry_send.velocity = state_est_data.velocity_world[2];
		telemetry_send.ts = osKernelGetTickCount();

		/* Write Buzzer State*/
		telemetry_send.flight_phase = telemetry_send.flight_phase + 128*(buzzer_on_fsm ^ buzzer_on_telemetry);

		telemetry_send.checksum = calculate_checksum(&telemetry_send);

		/* Send to Xbee module */
		HAL_UART_Transmit(&huart7, (uint8_t*) &telemetry_send, sizeof(telemetry_send), 100);

		telemetry_send.checksum = 0;

		uart_counter = 0;
		/* Sleep */
		osDelayUntil(tick_count);
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart==&huart7){

		static uint8_t buffer [4];
		buffer[uart_counter] = local_command_rx;
		uart_counter++;
		if(uart_counter == 4){
			uint8_t succesful = 1;
			for (int i = 1; i < 4; i++) if (buffer[0] != buffer[i]) succesful = 0;
			uart_counter = 0;
			if (succesful) {
				new_command = true;
				local_command = local_command_rx;
			}
		}

		HAL_UART_Receive_IT(huart, (uint8_t*)&local_command_rx, 1);
	}
}

uint8_t calculate_checksum(telemetry_t *cnf){
	uint8_t cs=0;
	uint8_t *data;
	data = (uint8_t*) cnf;
	for(int i=0; i< sizeof(telemetry_t);i++){
		cs+=*data++;
	}
	return (255 - cs);
}
