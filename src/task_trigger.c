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

void TaskTriggerMsg(HMICmd_t* pHMICmd, int16_t iServerStatus)
{
	static unsigned char ucPreviousFlagByte;
	unsigned char ucActualFlagByte, ucEventFlagByte, ucMode_ActualBits, ucMode_EventBits;
	bool bLiftLimits, bSendToArm, bSendToPole, bSendToLift, bControlEnable_EventBit, bLiftLimits_EventBit, bLiftLimits_ActualBit,
		bTypeStop, bTypeFreeRunStart, bTypeAutoStart, bTypeLiftUp, bTypeLiftDown;

	ucActualFlagByte = 0x00;
	bSendToArm = FALSE;
	bSendToPole = FALSE;
	bSendToLift = FALSE;
	bTypeStop = FALSE;
	bTypeFreeRunStart = FALSE; 
	bTypeAutoStart = FALSE;
	bTypeLiftUp = FALSE;
	bTypeLiftDown = FALSE;

	mpap_t* pArmMsg;
	mpap_t* pPoleMsg;
	lift_t *pLiftMsg;

	
	/*	-- ucActualFlagByte -- Se consituye un byte donde 4 de sus bits -b0 a b3- representan 
		b0b1: mode. 00: STOP, 01: FREE RUN, 10: AUTO, 11: LIFT
		b2: ctrlEn. 0: Desabled, 1: Enabled.
		b3: liftLimits. 0: Run, 1: Limit
	*/
	/*	-- mode --	*/
	if (pHMICmd->mode == eStop) { NOP_FUNCTION; }
	else if (pHMICmd->mode == eFree_run) { BitSet(ucActualFlagByte, bit0); }
	else if (pHMICmd->mode == eAuto)	 { BitSet(ucActualFlagByte, bit1); }
	else if (pHMICmd->mode == eLift)	 { BitSet(ucActualFlagByte, bit0); BitSet(ucActualFlagByte, bit1); }
	else { depurador(eError, "error - prvTrigger: pHMICmd->mode"); }
	/*	-- ctrlEn --	*/
	if (pHMICmd->ctrlEn == eEnable) { BitSet(ucActualFlagByte, bit2);  }

	ucEventFlagByte = ucActualFlagByte ^ ucPreviousFlagByte;

	/* Discriminación de bits para Mode, CtrlEn, y Lift, para manipulación */
	ucMode_ActualBits = 0x03 & ucActualFlagByte;
	ucMode_EventBits = 0x03 & ucEventFlagByte;
	bControlEnable_EventBit = BitStatus(ucEventFlagByte, bit2);
	bLiftLimits_EventBit = BitStatus(ucEventFlagByte, bit3);
	bLiftLimits_ActualBit = BitStatus(ucActualFlagByte, bit3);

	/*		--	CONDICIONAMIENTOS	--
		La lógica que define los mensajes para el disparo (desbloqueo) de las tareas que administran los dispositivos -Arm, Pole, y 
		Lift- solo se ejecutan si se detectó algún cambio en los comandos ingresados desde la estación HMI -HMICmd-, los cuales se 
		registran mediante el byte de banderas -ucEventFlagByte- (en él, cada bit es una bandera). También en el caso en que se 
		produzca la deshabilitación de la variable -crtlEn-, en cuya situación se detienen todos los procesos. Mientras -ctrlEn-, 
		se encuentre deshabilitado, no se vuelve a ejecutar esta lógica de gestión de mensajes para inicio tareas 
		*/
	if(ucEventFlagByte!=0x00 && (pHMICmd->ctrlEn || BitStatus(ucPreviousFlagByte, bit2)))
	{
		/*	-- Mode Trigger --	*/
		
		switch (ucMode_EventBits)
		{
		case 0x00:
			break;	/*	No se registraron eventos para Mode */
		
		case 0x01:
			if (ucMode_ActualBits == 0x00)	/*	-- STOP FREE RUN COMMAND --		*/
			{
				depurador(eError, "STOP FR MODE");
				bTypeStop = TRUE;
				if (pHMICmd->freeRunAxis == eArm) { bSendToArm = TRUE; }
				else { bSendToPole = TRUE; }
				configASSERT(pHMICmd->mode == eStop); /* Debería corresponder solo al modo STOP */
			}
			else if (ucMode_ActualBits == 0x01)	/*	--	START FREE RUN COMMAND --	*/
			{
				depurador(eError,  "START FR MODE");
				bTypeFreeRunStart = TRUE;
				if (pHMICmd->freeRunAxis == eArm) { bSendToArm = TRUE; }
				else { bSendToPole = TRUE; }
				configASSERT(pHMICmd->mode == eFree_run); /* Debería corresponder solo al modo FreeRun */
			}
			else { depurador(eWarn, "RTUcomHMI.c", " error - prvTaskTriggerMsg:ucMode_EventBits case 0x01"); }

			break;

		case 0x02:
			bSendToArm = TRUE; 
			bSendToPole = TRUE;
			if (ucMode_ActualBits == 0x00)	/*	-- STOP AUTO COMMAND --		*/
			{
				depurador(eError, " STOP AUTO MODE");
				bTypeStop = TRUE;
				configASSERT(pHMICmd->mode == eStop); /* Debería corresponder solo al modo Automatico */
			}
			else if (ucMode_ActualBits == 0x02)	/*	--	START AUTO COMMAND --	*/
			{
				depurador(eError, " START AUTO MODE");
				bTypeAutoStart = TRUE;
				configASSERT(pHMICmd->mode == eAuto); /* Debería corresponder solo al modo Automatico */
			}
			else { depurador(eWarn, "RTUcomHMI.c", " error - prvTaskTriggerMsg:ucMode_EventBits case 0x02"); }

			break;

		case 0x03:
			bSendToLift = TRUE;
			if (ucMode_ActualBits == 0x00)	/*	-- STOP LIFT COMMAND --		*/
			{
				depurador(eError, " STOP LIFT");
				bTypeStop = TRUE;
				configASSERT(pHMICmd->mode == eStop); /* Debería corresponder solo al modo Lift */
			}
			else if (ucMode_ActualBits == 0x03)	/*	--	START LIFT COMMAND --	*/
			{
				depurador(eError, " START LIFT");
				if (pHMICmd->liftDir == eUp) { bTypeLiftUp = TRUE; }
				else { bTypeLiftDown = TRUE; }
				configASSERT(pHMICmd->mode == eLift); /* Debería corresponder solo al modo Lift */
			}
			else { depurador(eWarn, "RTUcomHMI.c", " error - prvTaskTriggerMsg:ucMode_EventBits case 0x03"); }

			

			break;
		}

		/*	-- CtrlEn TaskTriggerMsg --	*/
		/*	Evalúa solo la deshabiliatación del bit -Actual- correspondiente a Control Enable, ya que una vez en -eDesabled- 
			no se procesará la lógica desde -CONDICIONAMIENTOS-	*/
		if (bControlEnable_EventBit)
		{
			if (pHMICmd->ctrlEn == eDesable)
			{
				bTypeStop = TRUE;
				bSendToArm = TRUE;
				bSendToPole = TRUE;
				bSendToLift = TRUE;
				depurador(eError, " CONTROL DISABLE! STOP ALL!");
				configASSERT(pHMICmd->ctrlEn == eDesable); /* Debería corresponder al HMICmd.CtrlEn = eDesable */
			}
			else { depurador(eError, " Se activa el control -CONTROL ENABLE-!"); }
		}
		
		if(bSendToArm)
		{	pArmMsg = (mpap_t*)pvPortMalloc(sizeof(mpap_t)); 
			if (bTypeStop) { pArmMsg->type = MOT_PAP_MSG_TYPE_STOP; }
			else if (bTypeFreeRunStart) { pArmMsg->type = MOT_PAP_MSG_TYPE_FREE_RUNNING; }
			else if (bTypeAutoStart) { pArmMsg->type = MOT_PAP_MSG_TYPE_CLOSED_LOOP; }
			else { depurador(eError, " Error bSendToArm"); }
			pArmMsg->free_run_direction = pHMICmd->freeRunDir;
			pArmMsg->free_run_speed = pHMICmd->velCmdArm;
			pArmMsg->closed_loop_setpoint = pHMICmd->posCmdArm;
			if (xQueueSend(arm_queue, &pArmMsg, portMAX_DELAY) == pdPASS) { depurador(eError, " Comando enviado a arm.c exitoso!"); }
			else { depurador(eError, "Comando NO PUDO ser enviado a arm.c"); }
		}
		if (bSendToPole)
		{
			pPoleMsg = (mpap_t*)pvPortMalloc(sizeof(mpap_t));
			if (bTypeStop) { pPoleMsg->type = MOT_PAP_MSG_TYPE_STOP; }
			else if (bTypeFreeRunStart) { pPoleMsg->type = MOT_PAP_MSG_TYPE_FREE_RUNNING; }
			else if (bTypeAutoStart) { pPoleMsg->type = MOT_PAP_MSG_TYPE_CLOSED_LOOP; }
			else { depurador(eError, "Error bSendToPole"); }
			pPoleMsg->free_run_direction = pHMICmd->freeRunDir;
			pPoleMsg->free_run_speed = pHMICmd->velCmdPole;
			pPoleMsg->closed_loop_setpoint = pHMICmd->posCmdPole;
			if (xQueueSend(pole_queue, &pPoleMsg, portMAX_DELAY) == pdPASS) { depurador(eError, "Comando enviado a pole.c exitoso!"); }
			else { depurador(eError, "Comando NO PUDO ser enviado a pole.c"); }
		}
		if(bSendToLift)
		{
			pLiftMsg = (lift_t*)pvPortMalloc(sizeof(lift_t));
			if (bTypeStop) { pLiftMsg->type = LIFT_TYPE_STOP; }
			else if (bTypeLiftUp) { pLiftMsg->type = LIFT_TYPE_UP; }
			else if (bTypeLiftDown) { pLiftMsg->type = LIFT_TYPE_DOWN; }
			else { depurador(eError, "Error bSendToLift"); }
			if (xQueueSend(lift_queue, &pLiftMsg, portMAX_DELAY) == pdPASS) { depurador(eError, "Comando enviado a lift.c exitoso!"); }
			else { depurador(eError, "Comando NO PUDO ser enviado a lift.c"); }
		}
		

	}
	
	ucPreviousFlagByte = ucActualFlagByte;

	return;
}
/*-----------------------------------------------------------*/
