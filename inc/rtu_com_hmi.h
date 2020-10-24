#ifndef RTU_COM_HMI_H
#define RTU_COM_HMI_H

/* SM13 Remote Terminal Unit -RTU- Prototype TCP Server
  Automatically connects to HMI running on remote client */

  /* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/* SM13 */
#include "mot_pap.h"


/* Máximo tiempo de espera para cierre de seción del socket. */
#define SHUTDOWN_DELAY	( pdMS_TO_TICKS( 5000 ) )

/* Puerto de conexión del RTU. */
#define PORT_NUMBER		5020

/* If ipconfigUSE_TCP_WIN is 1 then the Tx sockets will use a buffer size set by
ipconfigTCP_TX_BUFFER_LENGTH, and the Tx window size will be
configECHO_SERVER_TX_WINDOW_SIZE times the buffer size.  Note
ipconfigTCP_TX_BUFFER_LENGTH is set in FreeRTOSIPConfig.h as it is a standard TCP/IP
stack constant, whereas configECHO_SERVER_TX_WINDOW_SIZE is set in
FreeRTOSConfig.h as it is a demo application constant. */
#ifndef SERVER_TX_WINDOW_SIZE
#define SERVER_TX_WINDOW_SIZE	2
#endif

/* If ipconfigUSE_TCP_WIN is 1 then the Rx sockets will use a buffer size set by
ipconfigTCP_RX_BUFFER_LENGTH, and the Rx window size will be
configECHO_SERVER_RX_WINDOW_SIZE times the buffer size.  Note
ipconfigTCP_RX_BUFFER_LENGTH is set in FreeRTOSIPConfig.h as it is a standard TCP/IP
stack constant, whereas configECHO_SERVER_RX_WINDOW_SIZE is set in
FreeRTOSConfig.h as it is a demo application constant. */
#ifndef SERVER_RX_WINDOW_SIZE
#define SERVER_RX_WINDOW_SIZE	2
#endif

/* Tamaño de variables de red a recibir y transmitir al HMI via ethernet */
enum { HMI_NETVAR_SIZE = 5 };
enum { RTU_NETVAR_SIZE = 9 };
enum { RTUDATA_BUFFER_SIZE = 100 };
enum { SERVERTASK_STACK_SIZE = 100 };


/*	-- RTUData --
	Estructura de los datos a colocar en la trama que se envía 
	al software HMI, con el estado actual del RTU y sus dispositivos.
 */
 typedef struct
{
	uint16_t resActArm;
	uint16_t resActPole;
	uint16_t velActArm;
	uint16_t velActPole;
	char cwLimitArm[RTU_NETVAR_SIZE];
	char ccwLimitArm[RTU_NETVAR_SIZE];
	char cwLimitPole[RTU_NETVAR_SIZE];
	char ccwLimitPole[RTU_NETVAR_SIZE];
	char limitUp[RTU_NETVAR_SIZE];
	char limitDown[RTU_NETVAR_SIZE];
	char stallAlm[RTU_NETVAR_SIZE];
	uint8_t status;
	char buffer[RTUDATA_BUFFER_SIZE];

} RTUData_t;
/**/
typedef struct HMIDATA
{
	uint16_t posCmdArm;
	uint16_t posCmdPole;
	uint8_t velCmdArm;
	uint8_t velCmdPole;
	unsigned char mode[HMI_NETVAR_SIZE];		/*	-- mode --			STOP; FRUN; AUTO; LIFT; */
	unsigned char freeRunAxis[HMI_NETVAR_SIZE];	/*	-- freeRunAxis	--	POLE; ARM_;				*/
	unsigned char freeRunDir[HMI_NETVAR_SIZE];	/*	-- freeRunDir  --	CW__; CCW_;				*/
	unsigned char ctrlEn[HMI_NETVAR_SIZE];		/*	-- ctrlEn --		CTLE; DCTL;				*/
	unsigned char stallEn[HMI_NETVAR_SIZE];		/*	-- stallEn --		STLE; DSTL;				*/
	unsigned char liftDir[HMI_NETVAR_SIZE];		/*	-- LiftDir --		LFUP; LFDW;				*/
	unsigned char clientId[HMI_NETVAR_SIZE];	/*	-- clientId --		SM13; 					*/

} HMIData_t;
/*		--	HMICMD	--
	 Estructura que almacena la recepción de datos en la trama que envía 
	el software HMI de acuerdo a los comandos efectuados por el operador. */
typedef enum	/*	Definición enumerada de los modos de funcionamiento comandados por el HMI.	*/
{
	eStop = 0,
	eFree_run = 1,
	eAuto = 2,
	eLift = 3,

}mode_t;	
typedef enum { eArm, ePole } freeRunAxis_t;
typedef enum { eCW, eCCW } freeRunDir_t;
typedef enum { eDesable, eEnable } enable_t;
typedef enum { eDown, eUp } liftDir_t;
typedef enum { eUnsigned, eSigned } sign_t;
/**/
typedef struct
{
	uint16_t posCmdArm;
	uint16_t posCmdPole;
	uint8_t velCmdArm;
	uint8_t velCmdPole;
	mode_t mode;		
	freeRunAxis_t freeRunAxis;
	freeRunDir_t freeRunDir;
	enable_t ctrlEn;
	enable_t stallEn;
	liftDir_t liftDir;
	sign_t clientID;

} HMICmd_t;

/*	--	mpap	--	*/
typedef struct mot_pap_msg {
	enum {
		MOT_PAP_MSG_TYPE_FREE_RUNNING,
		MOT_PAP_MSG_TYPE_CLOSED_LOOP,
		MOT_PAP_MSG_TYPE_STOP
	} type;
	enum mot_pap_direction free_run_direction;
	uint32_t free_run_speed;
	uint32_t closed_loop_setpoint;
}mpap_t;
typedef struct mot_pap_status {
	enum {
		MOT_PAP_STATUS_CW = 0, MOT_PAP_STATUS_CCW = 1, MOT_PAP_STATUS_STOP
	} dir;
	int32_t posCmd;
	int32_t posAct;
	uint32_t vel;
	volatile bool cwLimit;
	volatile bool ccwLimit;
	volatile bool stalled;
}mpapstatus_t;

/*	--	lift	--	*/
typedef enum {LIFT_TYPE_UP = 0, LIFT_TYPE_DOWN = 1, LIFT_TYPE_STOP} eLift_t;

typedef struct
{
	eLift_t type;
}lift_t;

typedef struct lift_status {
	eLift_t type;
	volatile bool upLimit;
	volatile bool downLimit;
}liftstatus_t;

/*	-- Declaracion de funciones compartidas --	*/
mpapstatus_t pole_get_status(void);	/* Funciones que utilizamos para obtener los valores de las estructuras mot_pap_status */
mpapstatus_t arm_get_status(void);
liftstatus_t lift_get_status(void);

/*	-- Declaración de objetos --	*/
QueueHandle_t lift_queue;

/*	-- Declaración de funciones RTUcomHMI --	*/
/*
	Asigna valores a RTUDATA según el contenido de RTUVAL.
	Esta conversión de valores se realiza con el fin de
	transmitir valores "Human readable" por ethernet.
	De esta forma se simplifica el debbug, mediante captura
	de tramas ethernet.
*/
void NetValuesToSendFromRTU(int16_t iServerStatus,RTUData_t* pRTUDataTx, mpapstatus_t* pArmStatus, mpapstatus_t* pPoleStatus, liftstatus_t* pLiftStatus); // Confirmar cambio
/*
	-- DataRxTx --
	Intercambio de tramas con el software HMI.
 */
static void prvDataRxTxTask(void* pvParameters);
/*
	-- Desentramado HMIDataRx --
	Se trabaja la trama recibida desde HMI para conformar los datos y comandos HMI.
*/
void NetValuesReceivedFromHMI(HMIData_t* HMIData, HMICmd_t* HMICmd);
/*	-- Conexión del servidor --
		Queda a la espera por solicitud de conexión del Software HMI en el puerto 5020.
 */
static void prvServerConnectTask(void* pvParameters);
/**/
static int16_t prvStatusHandlerRecv(HMICmd_t* pHMICmd, int16_t iServerStatus, Socket_t xConnectedSocket, uint16_t usLenHMIDataRx, uint16_t iRecv);
/**/
static int16_t prvStatusHandlerSend(int16_t iServerStatus, int lSent, Socket_t xConnectedSocket);
/**/
void TaskTriggerMsg(HMICmd_t* pHMICmd, int16_t iServerStatus);
/*	LECCION: TODOS LOS ARGUMENTOS QUE SE INCLUYEN EN LAS FUNCIONES DEBEN NECESARIAMENTE COMPARTIR EL MISMO ARCHIVO .H EN DONDE 
	SE DECLARA LA FUNCIÓN. POR EJEMPLO, LA FUNCIÓN -prvTrip- SE DECLARA EN ESTE ARCHIVO -RTUcomHMI.h- POR LO QUE LA ESTRUCTURA
	DE TIPO mpap_t, NO PUEDE DECLARARSE EN EL ARCHIVO -MOT_PAP.h-. POR ESO MOVÍ LA DECLARACIÓN A ESTE ARCHIVO.
*/	
/*-----------------------------------------------------------*/
#endif