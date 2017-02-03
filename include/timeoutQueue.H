
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


# ifndef TIMEOUTQUEUE_H
# define TIMEOUTQUEUE_H

# include <sys/time.h>
# include <unistd.h>
# include <pthread.h>
# include <tpl_binHeap.H>
# include <useMutex.H>
# include "ipcPars.H"


# define MSEC 1000
# define USEC 1000000
# define NSEC 1000000000

# define EVENT_TIME(e) ((e)->getKey())

# define EVENT_SEC(e) (EVENT_TIME(e).tv_sec)
# define EVENT_NSEC(e) (EVENT_TIME(e).tv_nsec)

class TimeoutQueue 
{
public:

  friend class Event : private BinHeapVtl<struct timespec>::Node
  {
    friend class TimeoutQueue;
  private:
    
    friend bool operator < (const struct timespec&, const struct timespec&);
    friend bool operator <= (const struct timespec&, const struct timespec&);
    friend bool operator > (const struct timespec&, const struct timespec&);
    friend bool operator >= (const struct timespec&, const struct timespec&);

  public:
    enum Execution_Status 
    {
      Out_Queue, 
      In_Queue, 
      Canceled, 
      Executing, 
      Executed 
    };

  private:

    Execution_Status execution_status;

    void setAbsoluteTime(struct timespec _t)
    {
      ASSERT(_t.tv_nsec >= 0 and _t.tv_nsec < NSEC);
      EVENT_TIME(this) = _t;
    }

  protected:

    Event(const struct timespec& t) : 
      BinHeapVtl<struct timespec>::Node(t),
      execution_status(Out_Queue)
    {
      ASSERT(EVENT_NSEC(this) >= 0 and EVENT_NSEC(this) < NSEC);
    }

    Event(long sec, long nsec) :
      execution_status(Out_Queue)
    {
      ASSERT(nsec >= 0 and nsec < NSEC);
      
      EVENT_SEC(this)  = sec;
      EVENT_NSEC(this) = nsec;
    }

  public:

    virtual ~Event() 
    { 
      if (execution_status == In_Queue)
	ERROR("Attempting to delete an event in queue (0x%x)", this);
    }

    Execution_Status get_execution_status() const { return execution_status; }

    void set_execution_status(Execution_Status status)
    {
      execution_status = status;
    }    

    struct timespec getAbsoluteTime() { return EVENT_TIME(this); }

    virtual void EventFct() = 0;
  }; /* end class Event */

private:

  BinHeapVtl<struct timespec> prioQueue;

  pthread_mutex_t    mutex;

  pthread_cond_t     cond;

  pthread_t          threadId;

  bool               isShutdown;

  static int         instanceCounter;
  
  void *triggerEvent();

  static void* triggerEventThread(void*);

public:

  TimeoutQueue();

  ~TimeoutQueue();

  void insertEvent(const struct timespec &, Event*) 
    Exception_Prototypes (Overflow);

  void insertEvent(Event*) Exception_Prototypes (Overflow);

  bool cancelEvent(Event*); 

  void cancel_delete_event(Event *& event)
  {
    if (event == NULL)
      return;

    cancelEvent(event);
    Delete(event);
    event = NULL;
  }

  void shutdown();

  pthread_t getThreadId() const { return threadId; }
};


inline long msec_to_nsec(long msec)
{
  ASSERT(msec >= 0 and msec < MSEC);

  return msec*USEC;
}


inline long usec_to_nsec(long usec)
{
  ASSERT(usec >= 0 and usec < USEC);

  return usec*MSEC;
}


inline char * timespec_to_char(const struct timespec & t, char * str)
{
  sprintf(str, "(%ld sec, %ld nsec)", t.tv_sec, t.tv_nsec);

  return str;
}


    inline 
struct timespec timeval_to_timespec(const struct timeval& current_time)
{
  ASSERT(current_time.tv_usec >= 0 and current_time.tv_usec < USEC);

  struct timespec ret_val;
  ret_val.tv_sec = current_time.tv_sec;
  ret_val.tv_nsec = usec_to_nsec(current_time.tv_usec);

  ASSERT(ret_val.tv_nsec >= 0 and ret_val.tv_nsec < NSEC);

  return ret_val;
}


inline struct timespec read_current_time()
{
  struct timeval current_time;
  gettimeofday(&current_time, NULL);

  return timeval_to_timespec(current_time);
}

    inline
struct timespec timespec_plus_msec(const struct timespec& current_time, 
				   int                    msec)
{
  ASSERT(current_time.tv_nsec >= 0 and current_time.tv_nsec < NSEC);
  long sec  = msec/MSEC; /* compute seconds inside msec */
  msec %= MSEC; /* rend msec to a value lesser than 1000 */
  long total_nsec = current_time.tv_nsec + msec_to_nsec(msec);
  struct timespec _t;
  _t.tv_sec  = current_time.tv_sec + sec + total_nsec/NSEC;
  _t.tv_nsec = total_nsec%NSEC;
  ASSERT(_t.tv_nsec >= 0 and _t.tv_nsec < NSEC);
  return _t;
}

# endif /* TIMEOUTQUEUE_H */

