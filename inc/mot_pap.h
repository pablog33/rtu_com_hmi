#ifndef MOT_PAP_H_
#define MOT_PAP_H_

#include "stdint.h"
#include "stdbool.h"
#include "queue.h"
//#include "pid.h"
#include "rtu_com_hmi.h"

#ifdef __cplusplus
extern "C" {
#endif

	enum mot_pap_direction {
		MOT_PAP_DIRECTION_CW = 0, MOT_PAP_DIRECTION_CCW = 1,
	};


	QueueHandle_t pole_queue;

	QueueHandle_t arm_queue;

#ifdef __cplusplus
}
#endif

#endif /* MOT_PAP_H_ */
#pragma once
