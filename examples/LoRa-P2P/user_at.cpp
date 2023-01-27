/**
 * @file user_at.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Handle user defined AT commands
 * @version 0.1
 * @date 2021-11-15
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "app.h"

/*********************************************************************/
// Example AT command to change the value of the variable new_val:
// Query the value AT+SETVAL=?
// Set the value AT+SETVAL=120000
// Second AT command to show last packet content
// Query with AT+LIST=?
/*********************************************************************/
int32_t new_val = 3000;

/**
 * @brief Returns the current value of the custom variable
 * 
 * @return int always 0
 */
static int at_query_value()
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "Custom Value: %d", new_val);
	return 0;
}

/**
 * @brief Command to set the custom variable
 * 
 * @param str the new value for the variable without the AT part
 * @return int 0 if the command was succesfull, 5 if the parameter was wrong
 */
static int at_exec_value(char *str)
{
	new_val = strtol(str, NULL, 0);
	MYLOG("APP", "Value number >>%ld<<", new_val);
	return 0;
}

/**
 * @brief Example how to show the last LoRa packet content
 * 
 * @return int always 0
 */
static int at_query_packet()
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "Packet: %02X%02X%02X%02X",
			 g_lpwan_data.data_flag1,
			 g_lpwan_data.data_flag2,
			 g_lpwan_data.batt_1,
			 g_lpwan_data.batt_2);
	return 0;
}

/**
 * @brief List of all available commands with short help and pointer to functions
 * 
 */
atcmd_t g_user_at_cmd_list_example[] = {
	/*|    CMD    |     AT+CMD?      |    AT+CMD=?    |  AT+CMD=value |  AT+CMD  |  Permissions  |*/
	// GNSS commands
	{"+SETVAL", "Get/Set custom variable", at_query_value, at_exec_value, NULL, "RW"},
	{"+LIST", "Show last packet content", at_query_packet, NULL, NULL, "R"},
};

/** Number of user defined AT commands */
uint8_t g_user_at_cmd_num = sizeof(g_user_at_cmd_list_example) / sizeof(atcmd_t);

/** Pointer to the AT command list */
atcmd_t *g_user_at_cmd_list = g_user_at_cmd_list_example;