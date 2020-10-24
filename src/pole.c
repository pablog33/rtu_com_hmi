#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* SM13 includes */
#include "mot_pap.h"
#include "rtu_com_hmi.h"
#include "depurador.h"

#define POLE_TASK_PRIORITY ( configMAX_PRIORITIES - 2 )

static struct mot_pap_status status;
static void pole_supervisor_task();

static void pole_task(void* par)
{
	mpap_t *msg_rcv;

	while (1) {
		if (xQueueReceive(pole_queue, &msg_rcv, portMAX_DELAY) == pdPASS)
		{
			depurador(eInfo, "pole: command received");

			switch (msg_rcv->type)
			{
			case MOT_PAP_MSG_TYPE_FREE_RUNNING:
				depuradorN(eInfo, "Pole free_run_direction:", msg_rcv->free_run_direction);
				depuradorN(eInfo, "Pole free_run_speed:",msg_rcv->free_run_speed);
				break;

			case MOT_PAP_MSG_TYPE_CLOSED_LOOP:	//PID
				depuradorN(eInfo,"Pole closed_loop_setpoint:",msg_rcv->closed_loop_setpoint);
				break;

			default:
				depurador(eError, "lift.c", "STOP");
				break;
			}
			

		}
	}

}

void pole_supervisor_task()
{
	status.dir = MOT_PAP_STATUS_STOP;
	status.posCmd = 0;
	status.posAct = 0xEEFA;
	status.vel = 5;
	status.cwLimit = 1;
	status.ccwLimit = 0;
}


void pole_init()
{
	pole_queue = xQueueCreate(5, sizeof(struct mot_pap_msg*));
	//xTaskCreate(pole_task, "Pole", configMINIMAL_STACK_SIZE, NULL, POLE_TASK_PRIORITY, NULL);
	xTaskCreate(pole_task, "Pole", configMINIMAL_STACK_SIZE, NULL, 4, NULL);
	depurador(eDebug, "pole.c", "pole_task - TaskCreate"); //Pablo Priority Debug: Borrar
}

struct mot_pap_status pole_get_status(void)
{
	pole_supervisor_task();
	return status;
}