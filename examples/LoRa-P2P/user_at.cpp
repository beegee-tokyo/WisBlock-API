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
/*********************************************************************/
int32_t new_val = 3000;

bool user_at_handler(char *user_cmd, uint8_t cmd_size)
{
	MYLOG("APP", "Received User AT commmand >>%s<< len %d", user_cmd, cmd_size);

	// Get the command itself
	char *param;

	param = strtok(user_cmd, "=");
	MYLOG("APP", "Commmand >>%s<<", param);

	// Check if the command is supported
	if (strcmp(param, (const char *)"+SETVAL") == 0)
	{
		// check if it is query or set
		param = strtok(NULL, ":");
		MYLOG("APP", "Param string >>%s<<", param);

		if (strcmp(param, (const char *)"?") == 0)
		{
			// It is a query, use AT_PRINTF to respond
			AT_PRINTF("NEW_VAL: %d", new_val);
		}
		else
		{
			new_val = strtol(param, NULL, 0);
			MYLOG("APP", "Value number >>%ld<<", new_val);
		}
		return true;
	}
	return false;
}
