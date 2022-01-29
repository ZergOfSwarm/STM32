/*
 * ESP8266_HAL.c
 *
 *  Created on: Apr 14, 2020
 *      Author: Controllerstech
 */


#include "stm32f4xx_hal.h"
#include "uartRingBufDMA.h"
#include "ESP8266_HAL.h"
#include "stdio.h"
#include "string.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

#define wifi_uart &huart1
#define pc_uart &huart2


char buffer[20];


char *Basic_inclusion = "<!DOCTYPE html> <html>\n<head><meta name=\"viewport\"\
		content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n\
		<title>LED CONTROL</title>\n<style>html { font-family: Helvetica; \
		display: inline-block; margin: 0px auto; text-align: center;}\n\
		body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\
		h3 {color: #444444;margin-bottom: 50px;}\n.button {display: block;\
		width: 80px;background-color: #1abc9c;border: none;color: white;\
		padding: 13px 30px;text-decoration: none;font-size: 25px;\
		margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n\
		.button-on {background-color: #1abc9c;}\n.button-on:active \
		{background-color: #16a085;}\n.button-off {background-color: #34495e;}\n\
		.button-off:active {background-color: #2c3e50;}\np {font-size: 14px;color: #888;margin-bottom: 10px;}\n\
		</style>\n</head>\n<body>\n<h1>ESP8266 LED CONTROL</h1>\n";

char *LED_ON = "<p>LED Status: ON</p><a class=\"button button-off\" href=\"/ledoff\">OFF</a>";
char *LED_OFF = "<p>LED1 Status: OFF</p><a class=\"button button-on\" href=\"/ledon\">ON</a>";
char *Terminate = "</body></html>";


static void uartSend (char *str)
{
	HAL_UART_Transmit(wifi_uart, (uint8_t *) str, strlen (str), 1000);
}

static void debugLog (char *str)
{
	HAL_UART_Transmit(pc_uart, (uint8_t *) str, strlen (str), 1000);
}

/*****************************************************************************************************************************************/

int ESP_Init (char *SSID, char *PASSWD)
{
	char data[80];

	Ringbuf_Init();

//	uartSend("AT+RST\r\n");
	debugLog("STARTING.");
	for (int i=0; i<5; i++)
	{
		debugLog(".");
		HAL_Delay(1000);
	}

	/********* AT **********/
	uartSend("AT\r\n");
	if(isConfirmed(1000) != 1)
	{
		debugLog("failed at AT\n");
		return 0;
	}
	debugLog("\nAT---->OK\n\n");
	HAL_Delay (1000);

	/********* AT+CWMODE=1 **********/
	uartSend("AT+CWMODE=1\r\n");
	if(isConfirmed(2000) != 1)
	{
		debugLog("failed at CWMODE\n");
		return 0;
	}
	debugLog("CW MODE---->1\n\n");
	HAL_Delay (1000);

	/********* AT+CWJAP="SSID","PASSWD" **********/
	debugLog("connecting... to the provided AP\n");
	sprintf (data, "AT+CWJAP=\"%s\",\"%s\"\r\n", SSID, PASSWD);
	uartSend(data);
	if(waitFor("GOT IP", 10000) != 1)
	{
		debugLog("failed to connect to the provided SSID\n");
		return 0;
	}
	sprintf (data, "Connected to,\"%s\"\n\n", SSID);
	debugLog(data);
	debugLog("Waiting for 5 seconds\n");
	/* send dummy */
//	uartSend("AT\r\n");
	HAL_Delay (4000);

	/* sending dummy data */
	Ringbuf_Reset();
	HAL_Delay (1000);



	/********* AT+CIFSR **********/
	uartSend("AT+CIFSR\r\n");
	if (waitFor("CIFSR:STAIP,\"", 5000) != 1)
	{
		debugLog("failed to get the STATIC IP Step 1\n");
		return 0;
	}
	if (copyUpto("\"",buffer, 3000) != 1)
	{
		debugLog("failed to get the STATIC IP Step 2\n");
		return 0;
	}
	if(isConfirmed(2000) != 1)
	{
		debugLog("failed to get the STATIC IP Step 3\n");
		return 0;
	}
	int len = strlen (buffer);
	buffer[len-1] = '\0';
	sprintf (data, "IP ADDR: %s\n\n", buffer);
	debugLog(data);
	HAL_Delay (1000);

	uartSend("AT+CIPMUX=1\r\n");
	if (isConfirmed(2000) != 1)
	{
		debugLog("Failed at CIPMUX\n");
		return 0;
	}
	debugLog("CIPMUX---->OK\n\n");

	uartSend("AT+CIPSERVER=1,80\r\n");
	if (isConfirmed(2000) != 1)
	{
		debugLog("Failed at CIPSERVER\n");
		return 0;
	}
	debugLog("CIPSERVER---->OK\n\n");

	debugLog("Now Connect to the IP ADRESS\n\n");

	return 1;

}




int Server_Send (char *str, int Link_ID)
{
	int len = strlen (str);
	char data[80];
	sprintf (data, "AT+CIPSEND=%d,%d\r\n", Link_ID, len);
	uartSend(data);
	if (waitFor(">",5000) != 1)
	{
		debugLog("Failed to get \">\" after CIPSEND\n");
		return 0;
	}
	uartSend (str);
	if (isConfirmed(5000) != 1)
	{
		debugLog("Failed to get \"OK\" after sending data\n");
		return 0;
	}
	sprintf (data, "AT+CIPCLOSE=5\r\n");
	uartSend(data);
	if (isConfirmed(5000) != 1)
	{
		debugLog("Failed to get \"OK\" after Closing the connection\n");
		return 0;
	}
	return 1;
}

void Server_Handle (char *str, int Link_ID)
{
	char datatosend[1024] = {0};
	if (!(strcmp (str, "/ledon")))
	{
		sprintf (datatosend, Basic_inclusion);
		strcat(datatosend, LED_ON);
		strcat(datatosend, Terminate);
		Server_Send(datatosend, Link_ID);
	}

	else if (!(strcmp (str, "/ledoff")))
	{
		sprintf (datatosend, Basic_inclusion);
		strcat(datatosend, LED_OFF);
		strcat(datatosend, Terminate);
		Server_Send(datatosend, Link_ID);
	}

	else
	{
		sprintf (datatosend, Basic_inclusion);
		strcat(datatosend, LED_OFF);
		strcat(datatosend, Terminate);
		Server_Send(datatosend, Link_ID);
	}

}

int Server_Start (void)
{
	char buftocopyinto[64] = {0};
	char Link_ID;

	if (getAfter("+IPD,", 1, &Link_ID, 10000) != 1)
	{
		debugLog("Failed to get \"IPD link\" in server_start\n");
		return 0;
	}

	Link_ID -= 48;

	if (copyUpto(" HTTP/1.1", buftocopyinto, 5000) != 1)
	{
		debugLog("Failed to get copy upto \"HTTP\" \n");
		return 0;
	}

	if (checkString("/ledon", buftocopyinto) == 1)
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, 1);
		Server_Handle("/ledon",Link_ID);
		return 1;
	}

	else if (checkString("/ledoff", buftocopyinto) == 1)
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, 0);
		Server_Handle("/ledoff",Link_ID);
		return 1;
	}

	else if (checkString("/favicon.ico", buftocopyinto) == 1) return 1;

	else
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, 0);
		Server_Handle("/ ", Link_ID);
		return 1;
	}

	return 1;
}
