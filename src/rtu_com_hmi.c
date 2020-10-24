/* SM13 Remote Terminal Unit -RTU- Prototype TCP Server
  Automatically connects to HMI running on remote client */


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
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/*	RTUcomHMI	*/
#include "rtu_com_hmi.h"
#include "bitops.h"
#include "depurador.h"

/*	Motor Control	*/
#include "mot_pap.h"


void vStartTCPServerTasks(UBaseType_t uxPriority)
{
	
	//xTaskCreate(prvServerConnectTask, "TCPServer", SERVERTASK_STACK_SIZE, NULL, uxPriority + 1, NULL);
	xTaskCreate(prvServerConnectTask, "TCPServer", SERVERTASK_STACK_SIZE, NULL, 3, NULL);
	depurador(eDebug, "prvServerConnectTask - TaskCreate"); //Pablo Priority Debug: Borrar
}
/*-----------------------------------------------------------*/
static void prvServerConnectTask(void* pvParameters)
{
	depurador(eDebug, "prvServerConnectTask - TaskCreate"); //Pablo Priority Debug: Borrar
	struct freertos_sockaddr xClient, xBindAddress;
	Socket_t xListeningSocket, xConnectedSocket;
	socklen_t xSize = sizeof(xClient);
	static const TickType_t xReceiveTimeOut = portMAX_DELAY;
	const BaseType_t xBacklog = 1;

#if( ipconfigUSE_TCP_WIN == 1 )
	WinProperties_t xWinProps;

	/* Fill in the buffer and window sizes that will be used by the socket. */
	xWinProps.lTxBufSize = ipconfigTCP_TX_BUFFER_LENGTH;
	xWinProps.lTxWinSize = SERVER_TX_WINDOW_SIZE;
	xWinProps.lRxBufSize = ipconfigTCP_RX_BUFFER_LENGTH;
	xWinProps.lRxWinSize = SERVER_RX_WINDOW_SIZE;
#endif /* ipconfigUSE_TCP_WIN */

	/* Just to prevent compiler warnings. */
	(void)pvParameters;

	/* Apertura de socket en escucha. */
	xListeningSocket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
	configASSERT(xListeningSocket != FREERTOS_INVALID_SOCKET);

	/* Timeout para la conexión. */
	FreeRTOS_setsockopt(xListeningSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof(xReceiveTimeOut));

	BaseType_t xReuseSocket = pdTRUE;
	FreeRTOS_setsockopt(xListeningSocket, 0, FREERTOS_SO_REUSE_LISTEN_SOCKET, (void*)&xReuseSocket, sizeof(xReuseSocket));


	/* Establece Tamaños de ventan y buffer de recepción y emisión. */
#if( ipconfigUSE_TCP_WIN == 1 )
	{
		FreeRTOS_setsockopt(xListeningSocket, 0, FREERTOS_SO_WIN_PROPERTIES, (void*)&xWinProps, sizeof(xWinProps));
	}
#endif /* ipconfigUSE_TCP_WIN */

	/* Vincula el socket con el puerto en que quedará en la escucha de un pedido de conexión del cliente -HMIcomRTU- */
	xBindAddress.sin_port = PORT_NUMBER;
	xBindAddress.sin_port = FreeRTOS_htons(xBindAddress.sin_port);
	FreeRTOS_bind(xListeningSocket, &xBindAddress, sizeof(xBindAddress));
	FreeRTOS_listen(xListeningSocket, xBacklog);

	for (;; )
	{
		/* Queda a la espera de la conexión. */
		xConnectedSocket = FreeRTOS_accept(xListeningSocket, &xClient, &xSize);
		configASSERT(xConnectedSocket != FREERTOS_INVALID_SOCKET);

		/* Se crea tarea RTOS que administrará la conexión. Se le envía el socket conectado como parámetro -xConnectedSocket. */
		//xTaskCreate(prvDataRxTxTask, "TCPServer", SERVERTASK_STACK_SIZE, (void*)xConnectedSocket, tskIDLE_PRIORITY, NULL);
		xTaskCreate(prvDataRxTxTask, "TCPServer", SERVERTASK_STACK_SIZE, (void*)xConnectedSocket, 2, NULL);
		depurador(eDebug, "prvDataRxTxTask - TaskCreate"); //Pablo Priority Debug: Borrar
	}
}
/*-----------------------------------------------------------*/
static void prvDataRxTxTask(void* pvParameters)
{
	depurador(eDebug, "prvDataRxTxTask - 2"); //Pablo Priority Debug: Borrar
	int32_t usSent, ulTotalSent, iRecv, ulLenHMIDataRx, icount=0;
	int16_t iServerStatus;
	Socket_t xConnectedSocket;
	static const TickType_t xReceiveTimeOut = pdMS_TO_TICKS(5000);
	static const TickType_t xSendTimeOut = pdMS_TO_TICKS(5000);
	TickType_t xTimeOnShutdown;
	RTUData_t RTUDataTx;
	HMICmd_t HMICmd;
	HMIData_t HMIDataRx;
	mpapstatus_t ArmStatus;
	mpapstatus_t PoleStatus;
	liftstatus_t LiftStatus;
	iServerStatus = 0x00;

	
		/*	Socket recibido en parametro de función.	*/
	xConnectedSocket = (Socket_t)pvParameters;
	
	/*	Temporizadores de Timeout para la recepción y envio de datos. */
	FreeRTOS_setsockopt(xConnectedSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof(xReceiveTimeOut));
	FreeRTOS_setsockopt(xConnectedSocket, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof(xReceiveTimeOut));

	arm_init();
	pole_init();
	lift_init();

	for (;; )
	{
		/*	-- Recepción de tramas ethernet enviadas por HMI --	*/
		iRecv = 0; icount++;
		depuradorN(eError,"ciclos:", icount);
		iRecv = FreeRTOS_recv(xConnectedSocket, &HMIDataRx, sizeof(HMIDataRx), 0);
		ulLenHMIDataRx = sizeof(HMIDataRx)-1; 
		/*	El término de sustracción "-1" puede se innnecesario. Depende del "alignment" del la -HMIData Struct-
			Sería necesario si la estructura -HMIData- ocupa un número impar de bytes en memoria.
		*/
		/*	Desentramado de datos. Obtención de los comandos HMI -HMICmd- a partir de los descriptores -	*/	
		NetValuesReceivedFromHMI(&HMIDataRx, &HMICmd);

		prvStatusHandlerRecv(&HMICmd, iServerStatus, xConnectedSocket, ulLenHMIDataRx, iRecv);

		/*	Error que requiere reinicio de RTU. */
		/*	ID de cliente incorrecto o Error en formato de trama	*/
		if (iServerStatus == 0x81 || iServerStatus == 0x83) { depurador(eError, "Desconexion de HMI - Socket Reiniciando RTU!!");
			break;
		}

		TaskTriggerMsg(&HMICmd, iServerStatus);

		NetValuesToSendFromRTU(iServerStatus, &RTUDataTx, &ArmStatus, &PoleStatus, &LiftStatus);
	
		usSent = 0;
		usSent = FreeRTOS_send(xConnectedSocket, RTUDataTx.buffer, sizeof(RTUDataTx.buffer), 0);

		iServerStatus = prvStatusHandlerSend(iServerStatus, usSent, xConnectedSocket);

		/*	Error - Detección de pérdida de conexión con HMI.	*/
		if (iServerStatus == 0x82 || iServerStatus == 0x83 ) { depurador(eError, " Desconexion de HMI - Socket Disconnect / Reiniciando RTU!!");
			break;
		}
	}


	/* Orden de finalización de sesión para el socket, en caso de que aún no lo haya hecho */
	FreeRTOS_shutdown(xConnectedSocket, FREERTOS_SHUT_RDWR);

	/* Esperar hasta que se termine la sesión con el socket. */
	xTimeOnShutdown = xTaskGetTickCount();
	do
	{
		if (FreeRTOS_recv(xConnectedSocket, &HMIDataRx, sizeof(HMIDataRx), 0) < 0)
		{
			break;
		}
	} while ((xTaskGetTickCount() - xTimeOnShutdown) < SHUTDOWN_DELAY);

	/* Cierre del socket */
	FreeRTOS_closesocket(xConnectedSocket);
	
	/* Re-inicio del socket, luego de perdida total de conexión */
	prvServerConnectTask(NULL);

	vTaskDelete(NULL);
}
/*-----------------------------------------------------------*/
static int16_t prvStatusHandlerRecv(HMICmd_t* pHMICmd, int16_t iServerStatus, Socket_t xConnectedSocket, uint16_t usLenHMIDataRx, uint16_t iRecv)
{
	static bool bPrimeraTrama;


	if(iRecv<=0)
	{
		depurador( eError, " - Trama vacia - "); 
		if (!FreeRTOS_issocketconnected(xConnectedSocket)) { iServerStatus = 0x83; }
	}
	else if(iRecv == 65408)
	{
		depurador(eError, "HMI - Finalizo su conexion!!");
	}
	else if(iRecv!=usLenHMIDataRx)
	{
		iServerStatus = 0x83;	/*	Error: Formato de trama incorrecto!!	*/
		depurador(eError, "Error - Formato de trama incorrecto!!");
	}
	else if (!bPrimeraTrama)
	{
		depurador(eInfo, " - Primera trama - ");	/*	Log: Primera Trama	*/	

		if(pHMICmd->clientID == eUnsigned)
		{  
			iServerStatus = 0x81;	/*	Error: Cliente Incorrecto	*/
			depurador(eError, "Error - ID Cliente incorrecto!!");
		}
		else if(pHMICmd->clientID == eSigned)
		{
			depurador(eInfo, "HMI cliente SM13 - Conectado exitosamente con RTU!");
			bPrimeraTrama = 1;
		}
		
		return iServerStatus;
	}



}
/*-----------------------------------------------------------*/
static int16_t prvStatusHandlerSend(int16_t iServerStatus, int lSent, Socket_t xConnectedSocket)
{
	/*	-- Trama RTU - No se puede enviar -- 	*/
	if (lSent < 0 && iServerStatus != 0x81)
	{
		depurador(eError, "RTUcomHMI.com", "No se pudo enviar trama RTU hacia HMI.");
		iServerStatus = 0x83;
	}

	return iServerStatus;
}
/*-----------------------------------------------------------*/
//void prvNetValuesToSendFromRTU(int16_t iServerStatus, RTUData_t* pRTUDataTx, mpapstatus_t* pArmStatus, mpapstatus_t* pPoleStatus, liftstatus_t* pLiftStatus)
//{
//
//	*(pPoleStatus) = pole_get_status();	/*	Obtengo los valores de los dispositivos internos -Arm, Pole, LIFT-	*/
//	*(pArmStatus) = arm_get_status();
//	*(pLiftStatus) = lift_get_status();
//
//	pRTUDataTx->resActArm = pArmStatus->posAct;
//	pRTUDataTx->resActPole = pPoleStatus->posAct;
//	pRTUDataTx->velActArm = pArmStatus->vel;
//	pRTUDataTx->velActPole = pPoleStatus->vel;
//
//	/*	-- cwLimitArm --	*/
//	if (pArmStatus->cwLimit) { sprintf(pRTUDataTx->cwLimitArm, "%s", "ACW_LIM;"); }
//	else { sprintf(pRTUDataTx->cwLimitArm, "%s", "ACW_RUN;"); }
//
//	/*	-- ccwLimitArm --	*/
//	if (pArmStatus->ccwLimit) { sprintf(pRTUDataTx->ccwLimitArm, "%s", "ACC_LIM;"); }
//	else { sprintf(pRTUDataTx->ccwLimitArm, "%s", "ACC_RUN;"); }
//
//	/*	-- cwLimitPole --	*/
//	if (pPoleStatus->cwLimit) { sprintf(pRTUDataTx->cwLimitPole, "%s", "PCW_LIM;"); }
//	else { sprintf(pRTUDataTx->cwLimitPole, "%s", "PCW_RUN;"); }
//
//	/*	-- ccwLimitPole --	*/
//	if (pPoleStatus->ccwLimit) { sprintf(pRTUDataTx->ccwLimitPole, "%s", "PCC_LIM;"); }
//	else { sprintf(pRTUDataTx->ccwLimitPole, "%s", "PCC_RUN;"); }
//
//	/*	-- limitUp --	*/
//	if (pLiftStatus->upLimit) { sprintf(pRTUDataTx->limitUp, "%s", "LUP_LIM;"); }
//	else { sprintf(pRTUDataTx->limitUp, "%s", "LUP_RUN;"); }
//
//	/*	-- limitDown --	*/
//	if (pLiftStatus->downLimit) { sprintf(pRTUDataTx->limitDown, "%s", "LDW_LIM;"); }
//	else { sprintf(pRTUDataTx->limitDown, "%s", "LDW_RUN;"); }
//
//	/*	-- stallAlm --	*/
//	if (pArmStatus->stalled || pPoleStatus->stalled) { sprintf(pRTUDataTx->stallAlm, "%s", "STL_ALM;"); }
//	else { sprintf(pRTUDataTx->stallAlm, "%s", "STL_RUN;"); }
//
//	/*	-- status --	*/
//	if (iServerStatus) { pRTUDataTx->status = iServerStatus; }
//	else { pRTUDataTx->status = 0x00; }
//
//
//	sprintf_s(pRTUDataTx->buffer, 100, "%d %d %d %d %s %s %s %s %s %s %s %d ",
//		pRTUDataTx->resActArm, pRTUDataTx->velActArm, pRTUDataTx->resActPole, pRTUDataTx->velActPole,
//		pRTUDataTx->cwLimitArm, pRTUDataTx->ccwLimitArm, pRTUDataTx->cwLimitPole, pRTUDataTx->ccwLimitPole,
//		pRTUDataTx->limitUp, pRTUDataTx->limitDown, pRTUDataTx->stallAlm, pRTUDataTx->status);
//
//	return;
//}
///*-----------------------------------------------------------*/
//void prvNetValuesReceivedFromHMI(HMIData_t* HMIData, HMICmd_t* HMICmd)
//{	/*	HMI_NETVAR_SIZE + 1: Para tener en cuenta el fin de cadena en caracteres '\0' 	*/
//	char tempBuffer[HMI_NETVAR_SIZE + 1];
//	int x;
//
//	/*	Se asignan en forma directa los valores enteros correspondientes a objetivos para los resolvers	*/
//	HMICmd->posCmdArm = HMIData->posCmdArm;
//	HMICmd->velCmdArm = HMIData->velCmdArm;
//	HMICmd->posCmdPole = HMIData->posCmdPole;
//	HMICmd->velCmdPole = HMIData->velCmdPole;
//
//
//	/*		-- HMICmd Frame Parsing --	*/
//	/*	A partir de los descriptores -*HMIData- se obtienen los valores para HMICmd contenidos en la trama recibida.
//	*/
//
//	/*	-- mode --	*/
//	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->mode);
//	if (!strcmp(tempBuffer, "STOP;")) { HMICmd->mode = eStop; }
//	else if (!strcmp(tempBuffer, "FRUN;")) { HMICmd->mode = eFree_run; }
//	else if (!strcmp(tempBuffer, "AUTO;")) { HMICmd->mode = eAuto; }
//	else if (!strcmp(tempBuffer, "LIFT;")) { HMICmd->mode = eLift; }
//	else { depurador(eError, "error- HMICmd->mode"); }
//
//	/*	--	freeRunAxis --	*/
//	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->freeRunAxis);
//	if (!strcmp(tempBuffer, "ARM_;")) { HMICmd->freeRunAxis = eArm; }
//	else if (!strcmp(tempBuffer, "POLE;")) { HMICmd->freeRunAxis = ePole; }
//	else { depurador(eError, "error- HMICmd->freeRunAxis"); }
//
//	/*	--	freeRunDir --	*/
//	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->freeRunDir);
//	if (!strcmp(tempBuffer, "CW__;")) { HMICmd->freeRunDir = eCW; }
//	else if (!strcmp(tempBuffer, "CCW_;")) { HMICmd->freeRunDir = eCCW; }
//	else { depurador(eError, "error- HMICmd->freeRunDir"); }
//
//	/*	-- ctrlEn --	*/
//	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->ctrlEn);
//	if (!strcmp(tempBuffer, "CTLE;")) { HMICmd->ctrlEn = eEnable; }
//	else if (!strcmp(tempBuffer, "DCTL;")) { HMICmd->ctrlEn = eDesable; }
//	else { depurador(eError, "error- HMICmd->ctrlEn"); }
//
//	/*	-- stallEn --	*/
//	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->stallEn);
//	if (!strcmp(tempBuffer, "STLE;")) { HMICmd->stallEn = eEnable; }
//	else if (!strcmp(tempBuffer, "DSTL;")) { HMICmd->stallEn = eDesable; }
//	else { depurador(eError, "error- HMICmd->stallEn"); }
//
//	/*	-- liftDir --	*/
//	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->liftDir);
//	if (!strcmp(tempBuffer, "LFUP;")) { HMICmd->liftDir = eUp; }
//	else if (!strcmp(tempBuffer, "LFDW;")) { HMICmd->liftDir = eDown; }
//	else { depurador(eError, "error- HMICmd->liftDir"); }
//
//	/*	-- clientID --*/
//	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->clientId);
//	if (!strcmp(tempBuffer, "SM13;")) { HMICmd->clientID = eSigned; }
//	else { depurador(eError, "error- HMICmd->ClientID"); HMICmd->clientID = eUnsigned; }
//
//	return;
//}
//static void prvTaskTriggerMsg(HMICmd_t* pHMICmd, int16_t iServerStatus)
//{
//	static unsigned char ucPreviousFlagByte;
//	unsigned char ucActualFlagByte, ucEventFlagByte, ucMode_ActualBits, ucMode_EventBits;
//	bool bLiftLimits, bSendToArm, bSendToPole, bSendToLift, bControlEnable_EventBit, bLiftLimits_EventBit, bLiftLimits_ActualBit,
//		bTypeStop, bTypeFreeRunStart, bTypeAutoStart, bTypeLiftUp, bTypeLiftDown;
//
//	ucActualFlagByte = 0x00;
//	bSendToArm = FALSE;
//	bSendToPole = FALSE;
//	bSendToLift = FALSE;
//	bTypeStop = FALSE;
//	bTypeFreeRunStart = FALSE;
//	bTypeAutoStart = FALSE;
//	bTypeLiftUp = FALSE;
//	bTypeLiftDown = FALSE;
//
//	mpap_t* pArmMsg;
//	mpap_t* pPoleMsg;
//	lift_t* pLiftMsg;
//
//
//	/*	-- ucActualFlagByte -- Se consituye un byte donde 4 de sus bits -b0 a b3- representan
//		b0b1: mode. 00: STOP, 01: FREE RUN, 10: AUTO, 11: LIFT
//		b2: ctrlEn. 0: Desabled, 1: Enabled.
//		b3: liftLimits. 0: Run, 1: Limit
//	*/
//	/*	-- mode --	*/
//	if (pHMICmd->mode == eStop) { NOP_FUNCTION; }
//	else if (pHMICmd->mode == eFree_run) { BitSet(ucActualFlagByte, bit0); }
//	else if (pHMICmd->mode == eAuto) { BitSet(ucActualFlagByte, bit1); }
//	else if (pHMICmd->mode == eLift) { BitSet(ucActualFlagByte, bit0); BitSet(ucActualFlagByte, bit1); }
//	else { depurador(eError, "error - prvTrigger: pHMICmd->mode"); }
//	/*	-- ctrlEn --	*/
//	if (pHMICmd->ctrlEn == eEnable) { BitSet(ucActualFlagByte, bit2); }
//
//	ucEventFlagByte = ucActualFlagByte ^ ucPreviousFlagByte;
//
//	/* Discriminación de bits para Mode, CtrlEn, y Lift, para manipulación */
//	ucMode_ActualBits = 0x03 & ucActualFlagByte;
//	ucMode_EventBits = 0x03 & ucEventFlagByte;
//	bControlEnable_EventBit = BitStatus(ucEventFlagByte, bit2);
//	bLiftLimits_EventBit = BitStatus(ucEventFlagByte, bit3);
//	bLiftLimits_ActualBit = BitStatus(ucActualFlagByte, bit3);
//
//	/*		--	CONDICIONAMIENTOS	--
//		La lógica que define los mensajes para el disparo (desbloqueo) de las tareas que administran los dispositivos -Arm, Pole, y
//		Lift- solo se ejecutan si se detectó algún cambio en los comandos ingresados desde la estación HMI -HMICmd-, los cuales se
//		registran mediante el byte de banderas -ucEventFlagByte- (en él, cada bit es una bandera). También en el caso en que se
//		produzca la deshabilitación de la variable -crtlEn-, en cuya situación se detienen todos los procesos. Mientras -ctrlEn-,
//		se encuentre deshabilitado, no se vuelve a ejecutar esta lógica de gestión de mensajes para inicio tareas
//		*/
//	if (ucEventFlagByte != 0x00 && (pHMICmd->ctrlEn || BitStatus(ucPreviousFlagByte, bit2)))
//	{
//		/*	-- Mode Trigger --	*/
//
//		switch (ucMode_EventBits)
//		{
//		case 0x00:
//			break;	/*	No se registraron eventos para Mode */
//
//		case 0x01:
//			if (ucMode_ActualBits == 0x00)	/*	-- STOP FREE RUN COMMAND --		*/
//			{
//				depurador(eError, "STOP FR MODE");
//				bTypeStop = TRUE;
//				if (pHMICmd->freeRunAxis == eArm) { bSendToArm = TRUE; }
//				else { bSendToPole = TRUE; }
//				configASSERT(pHMICmd->mode == eStop); /* Debería corresponder solo al modo STOP */
//			}
//			else if (ucMode_ActualBits == 0x01)	/*	--	START FREE RUN COMMAND --	*/
//			{
//				depurador(eError, "START FR MODE");
//				bTypeFreeRunStart = TRUE;
//				if (pHMICmd->freeRunAxis == eArm) { bSendToArm = TRUE; }
//				else { bSendToPole = TRUE; }
//				configASSERT(pHMICmd->mode == eFree_run); /* Debería corresponder solo al modo FreeRun */
//			}
//			else { depurador(eWarn, "RTUcomHMI.c", " error - prvTaskTriggerMsg:ucMode_EventBits case 0x01"); }
//
//			break;
//
//		case 0x02:
//			bSendToArm = TRUE;
//			bSendToPole = TRUE;
//			if (ucMode_ActualBits == 0x00)	/*	-- STOP AUTO COMMAND --		*/
//			{
//				depurador(eError, " STOP AUTO MODE");
//				bTypeStop = TRUE;
//				configASSERT(pHMICmd->mode == eStop); /* Debería corresponder solo al modo Automatico */
//			}
//			else if (ucMode_ActualBits == 0x02)	/*	--	START AUTO COMMAND --	*/
//			{
//				depurador(eError, " START AUTO MODE");
//				bTypeAutoStart = TRUE;
//				configASSERT(pHMICmd->mode == eAuto); /* Debería corresponder solo al modo Automatico */
//			}
//			else { depurador(eWarn, "RTUcomHMI.c", " error - prvTaskTriggerMsg:ucMode_EventBits case 0x02"); }
//
//			break;
//
//		case 0x03:
//			bSendToLift = TRUE;
//			if (ucMode_ActualBits == 0x00)	/*	-- STOP LIFT COMMAND --		*/
//			{
//				depurador(eError, " STOP LIFT");
//				bTypeStop = TRUE;
//				configASSERT(pHMICmd->mode == eStop); /* Debería corresponder solo al modo Lift */
//			}
//			else if (ucMode_ActualBits == 0x03)	/*	--	START LIFT COMMAND --	*/
//			{
//				depurador(eError, " START LIFT");
//				if (pHMICmd->liftDir == eUp) { bTypeLiftUp = TRUE; }
//				else { bTypeLiftDown = TRUE; }
//				configASSERT(pHMICmd->mode == eLift); /* Debería corresponder solo al modo Lift */
//			}
//			else { depurador(eWarn, "RTUcomHMI.c", " error - prvTaskTriggerMsg:ucMode_EventBits case 0x03"); }
//
//
//
//			break;
//		}
//
//		/*	-- CtrlEn TaskTriggerMsg --	*/
//		/*	Evalúa solo la deshabiliatación del bit -Actual- correspondiente a Control Enable, ya que una vez en -eDesabled-
//			no se procesará la lógica desde -CONDICIONAMIENTOS-	*/
//		if (bControlEnable_EventBit)
//		{
//			if (pHMICmd->ctrlEn == eDesable)
//			{
//				bTypeStop = TRUE;
//				bSendToArm = TRUE;
//				bSendToPole = TRUE;
//				bSendToLift = TRUE;
//				depurador(eError, " CONTROL DISABLE! STOP ALL!");
//				configASSERT(pHMICmd->ctrlEn == eDesable); /* Debería corresponder al HMICmd.CtrlEn = eDesable */
//			}
//			else { depurador(eError, " Se activa el control -CONTROL ENABLE-!"); }
//		}
//
//		if (bSendToArm)
//		{
//			pArmMsg = (mpap_t*)pvPortMalloc(sizeof(mpap_t));
//			if (bTypeStop) { pArmMsg->type = MOT_PAP_MSG_TYPE_STOP; }
//			else if (bTypeFreeRunStart) { pArmMsg->type = MOT_PAP_MSG_TYPE_FREE_RUNNING; }
//			else if (bTypeAutoStart) { pArmMsg->type = MOT_PAP_MSG_TYPE_CLOSED_LOOP; }
//			else { depurador(eError, " Error bSendToArm"); }
//			pArmMsg->free_run_direction = pHMICmd->freeRunDir;
//			pArmMsg->free_run_speed = pHMICmd->velCmdArm;
//			pArmMsg->closed_loop_setpoint = pHMICmd->posCmdArm;
//			if (xQueueSend(arm_queue, &pArmMsg, portMAX_DELAY) == pdPASS) { depurador(eError, " Comando enviado a arm.c exitoso!"); }
//			else { depurador(eError, "Comando NO PUDO ser enviado a arm.c"); }
//		}
//		if (bSendToPole)
//		{
//			pPoleMsg = (mpap_t*)pvPortMalloc(sizeof(mpap_t));
//			if (bTypeStop) { pPoleMsg->type = MOT_PAP_MSG_TYPE_STOP; }
//			else if (bTypeFreeRunStart) { pPoleMsg->type = MOT_PAP_MSG_TYPE_FREE_RUNNING; }
//			else if (bTypeAutoStart) { pPoleMsg->type = MOT_PAP_MSG_TYPE_CLOSED_LOOP; }
//			else { depurador(eError, "Error bSendToPole"); }
//			pPoleMsg->free_run_direction = pHMICmd->freeRunDir;
//			pPoleMsg->free_run_speed = pHMICmd->velCmdPole;
//			pPoleMsg->closed_loop_setpoint = pHMICmd->posCmdPole;
//			if (xQueueSend(pole_queue, &pPoleMsg, portMAX_DELAY) == pdPASS) { depurador(eError, "Comando enviado a pole.c exitoso!"); }
//			else { depurador(eError, "Comando NO PUDO ser enviado a pole.c"); }
//		}
//		if (bSendToLift)
//		{
//			pLiftMsg = (lift_t*)pvPortMalloc(sizeof(lift_t));
//			if (bTypeStop) { pLiftMsg->type = LIFT_TYPE_STOP; }
//			else if (bTypeLiftUp) { pLiftMsg->type = LIFT_TYPE_UP; }
//			else if (bTypeLiftDown) { pLiftMsg->type = LIFT_TYPE_DOWN; }
//			else { depurador(eError, "Error bSendToLift"); }
//			if (xQueueSend(lift_queue, &pLiftMsg, portMAX_DELAY) == pdPASS) { depurador(eError, "Comando enviado a lift.c exitoso!"); }
//			else { depurador(eError, "Comando NO PUDO ser enviado a lift.c"); }
//		}
//
//
//	}
//
//	ucPreviousFlagByte = ucActualFlagByte;
//
//	return;
//}
///*-----------------------------------------------------------*/