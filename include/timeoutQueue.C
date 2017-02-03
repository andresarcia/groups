
/*
  Aleph System License & Copyright Note

  Copyright (c) 1999, 2000, 2001, 2002 
  UNIVERSITY LOS ANDES (ULA) Merida - VENEZUELA  
  All rights reserved.

  - Center of Studies in Microelectronics & Distributed Systems (CEMISID) 
  - National Center of Scientific Computing of ULA (CECALCULA)  
  - Computer Science Department of ULA

  PERMISSION TO USE, COPY, MODIFY AND DISTRIBUTE THIS SOFTWARE AND ITS
  DOCUMENTATION IS HEREBY GRANTED, PROVIDED THAT BOTH THE COPYRIGHT
  NOTICE AND THIS PERMISSION NOTICE APPEAR IN ALL COPIES OF THE
  SOFTWARE, DERIVATIVE WORKS OR MODIFIED VERSIONS, AND ANY PORTIONS
  THEREOF, AND THAT BOTH NOTICES APPEAR IN SUPPORTING DOCUMENTATION.

  Aleph is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.

  UNIVERSIDAD DE LOS ANDES requests users of this software to return to 

  CEMISID - Software     
  Ed La Hechicera 
  3er piso, ala Este
  Facultad de Ingenieria 
  Universidad de Los Andes 
  Merida - VENEZUELA                         or

  aleph@cemisid.ing.ula.ve

  any improvements or extensions that they make and grant Universidad 
  de Los Andes (ULA) the rights to redistribute these changes. 

  Aleph is (or was) granted by: 
  - Consejo de Desarrollo Cientifico, Humanistico, Tecnico de la ULA 
    (CDCHT)
  - Consejo Nacional de Investigaciones Cientificas y Tecnologicas 
    (CONICIT)

  This file is part of the Aleph system 
*/


# include <errno.h>
# include <string.h>
# include <typeinfo>
# include <ahNew.H>
# include "timeoutQueue.H"
# include "boot.H"

int TimeoutQueue::instanceCounter = 0;


bool operator < (const struct timespec& l, const struct timespec& r)
{
  ASSERT(l.tv_nsec >= 0 and l.tv_nsec < NSEC);
  ASSERT(r.tv_nsec >= 0 and r.tv_nsec < NSEC);

  if (l.tv_sec not_eq r.tv_sec)
    return l.tv_sec < r.tv_sec;

  return l.tv_nsec < r.tv_nsec;
}

    
bool operator <= (const struct timespec& l, const struct timespec& r)
{
  ASSERT(l.tv_nsec >= 0 and l.tv_nsec < NSEC);
  ASSERT(r.tv_nsec >= 0 and r.tv_nsec < NSEC);

  if (l.tv_sec not_eq r.tv_sec)
    return l.tv_sec < r.tv_sec; 

  return l.tv_nsec <= r.tv_nsec;
}


bool operator > (const struct timespec& l, const struct timespec& r)
{
  return not (l <= r);
}


bool operator >= (const struct timespec& l, const struct timespec& r)
{
  return not (l < r);
}


TimeoutQueue::TimeoutQueue() : isShutdown(false)
{
  if (instanceCounter >= 1)
    EXIT("Double instantiation (%d) of TimeoutQueue", instanceCounter);

  instanceCounter++;

  init_mutex(mutex);

  pthread_cond_init(&cond, NULL);

  int status = pthread_create(&threadId, NULL , triggerEventThread, this); 

  if (status not_eq 0)
    EXIT("Cannot create triggerEventThread (error code = %d)", status);

  MESSAGE("timeoutQueue thread created (0x%x)", &threadId);
}


TimeoutQueue::~TimeoutQueue()
{
  if (not isShutdown)
    EXIT("TimeoutQueue is not shut down");

  destroy_mutex(mutex);

  pthread_cond_destroy(&cond);
}


void TimeoutQueue::insertEvent(const struct timespec & trigger_time,
			       TimeoutQueue::Event *   event) 
  Exception_Prototypes (Overflow)
{
  ASSERT(event not_eq NULL);
  ASSERT(trigger_time.tv_nsec >= 0 and trigger_time.tv_nsec < NSEC);

  {
    CRITICAL_SECTION(mutex);

    if (event->get_execution_status() == Event::In_Queue)
      Throw (Duplicated) ("Event has already inserted in timeoutQueue");

    if (isShutdown)
      return;

    event->set_execution_status(Event::In_Queue);
    event->setAbsoluteTime(trigger_time);

    prioQueue.insert(event);
  }
  pthread_cond_signal(&cond);
}


void TimeoutQueue::insertEvent(TimeoutQueue::Event * event) 
  Exception_Prototypes (Overflow)
{
  ASSERT(event not_eq NULL);
  ASSERT(EVENT_NSEC(event) >= 0 and EVENT_NSEC(event) < NSEC);

  {
    CRITICAL_SECTION(mutex);

    if (event->get_execution_status() == Event::In_Queue)
      Throw (Duplicated) ("Event has already inserted in timemeQueue");

    if (isShutdown)
      return;

    event->set_execution_status(Event::In_Queue);

    prioQueue.insert(event);
  }
  pthread_cond_signal(&cond);
}


bool TimeoutQueue::cancelEvent(TimeoutQueue::Event* event) 
{
  CRITICAL_SECTION(mutex);

  if (event->get_execution_status() not_eq Event::In_Queue)
    return false;

  event->set_execution_status(Event::Canceled);

  prioQueue.remove(event);

  pthread_cond_signal(&cond);

  return true;
}


void *TimeoutQueue::triggerEvent()
{
  Event *event_to_schedule;
  Event *event_to_execute;
  struct timespec t;
  int status = 0;

  {
    CRITICAL_SECTION(mutex);

    while (1)
      {
	/* sleep if there is no events */ 
	while ((prioQueue.size() == 0) and (not isShutdown))
	  pthread_cond_wait(&cond, &mutex);

	if (isShutdown)
	  goto end; /* if shutdown is activated, get out */

	/* read the soonest event */ 
	event_to_schedule = static_cast<Event*>(prioQueue.top());
	    
	/* compute time when the event must triggered */ 
	t = EVENT_TIME(event_to_schedule);
	
	do
	  { /* sleep during t units of time, but be immune to signals
	       interruptions (status will be EINTR in the case where the
	       thread was signalized) */
	    status = pthread_cond_timedwait(&cond, &mutex, &t);

	    if (isShutdown) /* perhaps thread was signaled because
			       shutdown was requested */
	      goto end;

	  } while (status == EINTR);

	if (status == ETIMEDOUT) /* soonest event could be executed */
	  { 
	    /* event to execute could be changed if it was canceled */
	    event_to_execute = static_cast<Event*>(prioQueue.top());
	    if (event_to_execute not_eq event_to_schedule)
	      { /* events differ ==> perhaps event_to_schedule should not be
		   executed */
		if (EVENT_TIME(event_to_execute) > 
		    EVENT_TIME(event_to_schedule))
		  { /* This would possibly be a bug: there is ar later event
		       in the top of priority queue. We issue a warning and
		       continue */
		    WARNING("Detected an later event on the top of priority"
			    " queue");
		    continue; 
		  }
		/* in this case, a new and soonest event has been inserted
		   just when the timeout has expired */
	      }

	    /* event must be extracted from priority queue before its
	       execution because event function could self insert again
	       in the queue */
	    prioQueue.getMin();  
	    event_to_execute->set_execution_status(Event::Executing); 

	    critical_section.leave();	 
	    try
	      {
		event_to_execute->EventFct(); 
	      }
	    catch (exception & e)
	      {
		MESSAGE("Event execution has caused exception: %s", e.what());
	      }
	    critical_section.enter(); 
	    if (event_to_execute->get_execution_status() == Event::Executing)
	      event_to_execute->set_execution_status(Event::Executed); 
	  } /* posix threads only returns two error values. For that reason,
	       there is not else case */
      } /* end while (1) */

  end: /* shutdown has been requested */ 

  /* extract all events from priority queue */ 
    if (prioQueue.size() > 0)
      {
	/* extract and cancel all events */
	while (prioQueue.size() not_eq 0)
	  static_cast<Event*>(prioQueue.getMin())->
	    set_execution_status(Event::Canceled);
      }
  } /* end of critical section */

  MESSAGE("Terminating timeoutQueue thread %ld", pthread_self());
}


void TimeoutQueue::shutdown()
{
  CRITICAL_SECTION(mutex);

  isShutdown = true;

  pthread_cond_signal(&cond);
}


void* TimeoutQueue::triggerEventThread(void *obj)
{
  TimeoutQueue *timeoutQueue = static_cast<TimeoutQueue*>(obj);

  return timeoutQueue->triggerEvent();
}

