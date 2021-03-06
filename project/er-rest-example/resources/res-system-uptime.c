/**
 * \file
 *      System uptime resource. Offers system uptime in days, hours,
 * 		minutes and seconds.
 * 		KNOWN PROBLEMS: The uptime clock is a little bit inaccurate.
 * 		We lost around two minutes per day.
 * \author
 *      Alexander Graeb <s74742@htw-dresden.de>
 */

#include "contiki.h"

#define DEBUG 0
#if DEBUG
	#include <stdio.h>
	#define PRINTF(...) printf(__VA_ARGS__)
#else
	#define PRINTF(...)
#endif

#if DE_RF_NODE

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "rest-engine.h"
#include "res-system-uptime.h"

static void res_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

unsigned long uptime_seconds = 0; // ~136.19 years till counter overflow.
static uint8_t use_rtimer = 1;

#define MAX_AGE      60
#define RTIMER_CLOCK_T_MAX USHRT_MAX // rtimer_clock_t are currently unsigned short, see core/sys/rtimer.h.

/*---------------------------------------------------------------------------*/
PROCESS(uptime_process, "Uptime process");
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(uptime_process, ev, data)
{	
  PROCESS_BEGIN();
  rtimer_clock_t rtimer_now;
  static rtimer_clock_t rtimer_delta = 0, rtimer_last = 0;
  unsigned short elapsed_seconds;

  while (1) {
	rtimer_now = RTIMER_NOW();
	
	if (rtimer_now > rtimer_last)
		rtimer_delta += rtimer_now - rtimer_last;
	else
		rtimer_delta += RTIMER_CLOCK_T_MAX - rtimer_last + rtimer_now;
		
	elapsed_seconds = rtimer_delta / RTIMER_SECOND;
	
	if (elapsed_seconds > 0) {
		uptime_seconds += elapsed_seconds;
		rtimer_delta -= elapsed_seconds * RTIMER_SECOND;
		PRINTF("Uptime second counter: %lu\n", uptime_seconds);
    }
    
    rtimer_last = rtimer_now;
    
	PROCESS_PAUSE();
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/	

/* Initiates uptime ressource. Starts process for counting seconds when using rtimer.
 * Rtimer should be used when using a rdc protocol like ContikiMac, since the normal
 * clock is'nt running when mcu in sleep mode.
 */
void res_system_uptime_init(uint8_t use_rt) {
	if (use_rt) {
		use_rtimer = 1;
		process_start(&uptime_process, NULL);
	} else {
		use_rtimer = 0;
	}
}

// Setup non-periodic ressource (no subsscription handliing).
RESOURCE(res_system_uptime,
         "title=\"System uptime\";rt=\"Uptime\";obs",
         res_get_handler,
         NULL,
         NULL,
         NULL);

static void
res_get_handler(void *request, void *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  unsigned int accept = -1;
  REST.get_header_accept(request, &accept);
  
  unsigned long us = (use_rtimer) ? uptime_seconds : clock_seconds();
  
  unsigned short days = us / (60*60*24UL);
  us -= days * (60*60*24UL);
  
  unsigned char hours = us / (60*60);
  us -= hours * (60*60);
  
  unsigned char minutes = us / 60;
  us -= minutes * 60;

  if(accept == -1 || accept == REST.type.TEXT_PLAIN) {
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "%hu d %hhu h %hhu m %lu s (%lu/%lu s)\n", days, hours, minutes, us, uptime_seconds, clock_seconds());

    REST.set_response_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
  } else if(accept == REST.type.APPLICATION_JSON) {
    REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
	snprintf((char *)buffer, REST_MAX_CHUNK_SIZE, "{'uptime':'%hu d %hhu h %hhu m %lu s'}", days, hours, minutes, us);

    REST.set_response_payload(response, buffer, strlen((char *)buffer));
  } else {
    REST.set_response_status(response, REST.status.NOT_ACCEPTABLE);
    const char *msg = "Supporting content-types text/plain and application/json";
    REST.set_response_payload(response, msg, strlen(msg));
  }

  REST.set_header_max_age(response, MAX_AGE);
}
#endif /* DE_RF_NODE */
