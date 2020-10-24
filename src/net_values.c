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
#include "depurador.h"

/*	Motor Control	*/
#include "mot_pap.h"

void NetValuesToSendFromRTU(int16_t iServerStatus, RTUData_t *pRTUDataTx, mpapstatus_t* pArmStatus, mpapstatus_t* pPoleStatus, liftstatus_t* pLiftStatus)
{

	*(pPoleStatus) = pole_get_status();	/*	Obtengo los valores de los dispositivos internos -Arm, Pole, LIFT-	*/
	*(pArmStatus) = arm_get_status();
	*(pLiftStatus) = lift_get_status();

	pRTUDataTx->resActArm = pArmStatus->posAct;
	pRTUDataTx->resActPole = pPoleStatus->posAct;
	pRTUDataTx->velActArm = pArmStatus->vel;
	pRTUDataTx->velActPole = pPoleStatus->vel;

	/*	-- cwLimitArm --	*/
	if (pArmStatus->cwLimit)	{	sprintf(pRTUDataTx->cwLimitArm, "%s", "ACW_LIM;");	}
	else {	sprintf(pRTUDataTx->cwLimitArm, "%s", "ACW_RUN;");	}
	
	/*	-- ccwLimitArm --	*/
	if (pArmStatus->ccwLimit)	{	sprintf(pRTUDataTx->ccwLimitArm, "%s", "ACC_LIM;");	}
	else {	sprintf(pRTUDataTx->ccwLimitArm, "%s", "ACC_RUN;");	}
	
	/*	-- cwLimitPole --	*/
	if (pPoleStatus->cwLimit)	{	sprintf(pRTUDataTx->cwLimitPole, "%s", "PCW_LIM;");	}
	else {	sprintf(pRTUDataTx->cwLimitPole, "%s", "PCW_RUN;");	}
	
	/*	-- ccwLimitPole --	*/
	if (pPoleStatus->ccwLimit)	{	sprintf(pRTUDataTx->ccwLimitPole, "%s", "PCC_LIM;");	}
	else {	sprintf(pRTUDataTx->ccwLimitPole, "%s", "PCC_RUN;");	}
	
	/*	-- limitUp --	*/
	if (pLiftStatus->upLimit)	{	sprintf(pRTUDataTx->limitUp, "%s", "LUP_LIM;");	}
	else {	sprintf(pRTUDataTx->limitUp, "%s", "LUP_RUN;");	}
	
	/*	-- limitDown --	*/
	if (pLiftStatus->downLimit)	{	sprintf(pRTUDataTx->limitDown, "%s", "LDW_LIM;");	}
	else {	sprintf(pRTUDataTx->limitDown, "%s", "LDW_RUN;");	}
	
	/*	-- stallAlm --	*/
	if (pArmStatus->stalled||pPoleStatus->stalled)	{	sprintf(pRTUDataTx->stallAlm, "%s", "STL_ALM;");	}
	else {	sprintf(pRTUDataTx->stallAlm, "%s", "STL_RUN;");	}

	/*	-- status --	*/
	if (iServerStatus) { pRTUDataTx->status = iServerStatus; }
	else { pRTUDataTx->status = 0x00; }

	
	sprintf_s(pRTUDataTx->buffer, 100, "%d %d %d %d %s %s %s %s %s %s %s %d ", 
	pRTUDataTx->resActArm, pRTUDataTx->velActArm, pRTUDataTx->resActPole, pRTUDataTx->velActPole, 
	pRTUDataTx->cwLimitArm, pRTUDataTx->ccwLimitArm, pRTUDataTx->cwLimitPole, pRTUDataTx->ccwLimitPole, 
	pRTUDataTx->limitUp, pRTUDataTx->limitDown, pRTUDataTx->stallAlm, pRTUDataTx->status);
	
	return;
}
/*-----------------------------------------------------------*/
void NetValuesReceivedFromHMI(HMIData_t *HMIData, HMICmd_t *HMICmd)
{	/*	HMI_NETVAR_SIZE + 1: Para tener en cuenta el fin de cadena en caracteres '\0' 	*/
	char tempBuffer[HMI_NETVAR_SIZE+1];
	int x;
		
	/*	Se asignan en forma directa los valores enteros correspondientes a objetivos para los resolvers	*/
	HMICmd->posCmdArm = HMIData->posCmdArm;
	HMICmd->velCmdArm = HMIData->velCmdArm;
	HMICmd->posCmdPole = HMIData->posCmdPole;
	HMICmd->velCmdPole = HMIData->velCmdPole;

	
	/*		-- HMICmd Frame Parsing --	*/
	/*	A partir de los descriptores -*HMIData- se obtienen los valores para HMICmd contenidos en la trama recibida.
	*/
	
	/*	-- mode --	*/
	snprintf(tempBuffer, HMI_NETVAR_SIZE+1,"%s", HMIData->mode);
	if(!strcmp(tempBuffer, "STOP;"))	{	HMICmd->mode = eStop;	}
	else if (!strcmp(tempBuffer, "FRUN;")) {	HMICmd->mode = eFree_run;	}
	else if (!strcmp(tempBuffer, "AUTO;")) {	HMICmd->mode = eAuto; }
	else if (!strcmp(tempBuffer, "LIFT;")) { HMICmd->mode = eLift; }
	else { depurador(eError,"error- HMICmd->mode"); }
	
	/*	--	freeRunAxis --	*/
	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->freeRunAxis);
	if (!strcmp(tempBuffer, "ARM_;")) { HMICmd->freeRunAxis = eArm; }
	else if (!strcmp(tempBuffer, "POLE;")) { HMICmd->freeRunAxis = ePole; }
	else { depurador(eError,"error- HMICmd->freeRunAxis"); }

	/*	--	freeRunDir --	*/
	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->freeRunDir);
	if (!strcmp(tempBuffer, "CW__;")) { HMICmd->freeRunDir = eCW; }
	else if (!strcmp(tempBuffer, "CCW_;")) { HMICmd->freeRunDir = eCCW; }
	else { depurador(eError,"error- HMICmd->freeRunDir"); }

	/*	-- ctrlEn --	*/
	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->ctrlEn);
	if (!strcmp(tempBuffer, "CTLE;")) { HMICmd->ctrlEn = eEnable; }
	else if (!strcmp(tempBuffer, "DCTL;")) { HMICmd->ctrlEn = eDesable; }
	else { depurador(eError,"error- HMICmd->ctrlEn"); }

	/*	-- stallEn --	*/
	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->stallEn);
	if (!strcmp(tempBuffer, "STLE;")) { HMICmd->stallEn = eEnable; }
	else if (!strcmp(tempBuffer, "DSTL;")) { HMICmd->stallEn = eDesable; }
	else { depurador(eError,"error- HMICmd->stallEn"); }
	
	/*	-- liftDir --	*/
	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->liftDir);
	if (!strcmp(tempBuffer, "LFUP;")) { HMICmd->liftDir = eUp; }
	else if (!strcmp(tempBuffer, "LFDW;")) { HMICmd->liftDir = eDown; }
	else { depurador(eError,"error- HMICmd->liftDir"); }

	/*	-- clientID --*/
	snprintf(tempBuffer, HMI_NETVAR_SIZE + 1, "%s", HMIData->clientId);
	if (!strcmp(tempBuffer, "SM13;")) { HMICmd->clientID = eSigned; }
	else { depurador(eError,"error- HMICmd->ClientID"); HMICmd->clientID = eUnsigned; }

	return;
}