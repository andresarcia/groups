#ifndef REMOTE_MULTISERVER_POINT
#define REMOTE_MULTISERVER_POINT

/*
  This is a class which serves as a client and server at the same time.
  So, it will take advantage of both the rpc-semantic and single-message
  semantic to solve efficiently the problem of sending messages.
 */

# include <aleph.H>
# include <pthread.h>
# include <slink.H>
# include "ipcClient.H"
# include "base_message_header.H"
# include "remote_multiserver_binding.H"
# include "useMutex.H"

template <class Service_Class>
class Remote_Multiserver_Point
{
  typedef int (Service_Class::*Sync_Service_Fct) 
    (Msg_Entry_Header *, 
     Remote_Multiserver_Binding<Service_Class> *);
  
  typedef int (Service_Class::*Async_Service_Fct) (Msg_Entry_Header *);

  Service_Class * ptr_service_class;

   /// Client_Binding needs to access some of the private members. 
  friend class Remote_Multiserver_Binding<Service_Class>;

  Ipc_MultiClient_Server * ptr_reception_deamon;

  enum Sending_Type
  {
    LOCCOM_RPC,
    LOCCOM_ASYNC,
  };

  void 
  sending_check(const Port & target_port, Msg_Entry_Header * request,
		size_t request_size,  Sending_Type type)
  throw (std::exception, NotFound)
  {  
    switch (request->get_deamon_service_code())
      {
      case DEMSERV_SEND :
	if (table[request->get_client_service_code()] == NULL)
	  {
	    MESSAGE("Not registered service!");
	    Throw (NotFound) ();
	  }
	break;
      case DEMSERV_SHUTDOWN :
	MESSAGE("Request for internal service");
	break;
      default:
	ERROR("*** ERROR: Unexpected service header");
	break;
      }
    
    ASSERT(request->get_parameters_size() >= 0);
    
    RawMsg request_message(request, request_size);
    
    switch (type)
      {
      case LOCCOM_RPC:
	ptr_reception_deamon->rpc_send_request(target_port, request_message);
	break;
      case LOCCOM_ASYNC:
	ptr_reception_deamon->send_message(target_port, request_message);
	break;
      default:
	ERROR("Error: Unexpected type of function.");
      }
  }
  
  /** 
      This variable indicates whether the server is requested 
      to be shutdown or not.
  */
  bool is_shutdown;

  /// main receiving thread for the deamon.
  pthread_t deamon_thread;

  /** 
      Number of actual active services. It has to be 0 so that the server
      can safely be shutdown.
  */
  size_t active_services;

  /**
     This mutex helps in keeping safe the critical sections of code
     which has to do with shutdown procedure.
  */
  pthread_mutex_t shutdown_lock;

  pthread_cond_t wait_for_shutdown;

  /** 
      This mutex helps whenever this class is intended for multiple 
      clients using. So, it is possible for several clients to simultaneously
      send a message. 
  */
  pthread_mutex_t lock_send;

  /**
     Condition variable in which the system waits until there are no more
     active services.
  */
  pthread_cond_t exists_act_serv;

  /** 
      This routine deciphers the message and either invokes the service
      or takes the corresponding system action.
  */  

  void dispatcher(Msg_Entry_Header * in_message,
		  Remote_Multiserver_Binding<Service_Class>  * to_whom)
  {               
    switch (in_message->get_deamon_service_code())
      {
      case DEMSERV_SEND:
	ASSERT(registered_services == number_of_services);
	
	if (table[in_message->get_client_service_code()] == NULL)
	  ERROR("Wrong service requested");
	else
	  {
	    /*
	      either run the service in the same server's thread or 
	      in a different one.
	    */
	    table[in_message->get_client_service_code()]->
	      execute_service(in_message, to_whom);
	    
	    Free(in_message);
	  }
	break;
	
      case DEMSERV_SHUTDOWN:
	MESSAGE("A SHUTDOWN message has arrived (Remote_Access_Point)");
	
	{
	  CRITICAL_SECTION(shutdown_lock);
	  
	  Free(in_message);
	  
	  // activate the shutdown flag.
	  is_shutdown = true;
	}
	
	pthread_cond_signal(&wait_for_shutdown);
	break;
	
      case DEMSERV_INVALID:
	MESSAGE("PANIC: An invalid message appeared");
	break;
	
      default:
	MESSAGE("PANIC: Check the code, a message with an unknown"
		" code arrived.");
	break;
      }
  }
  
  /**
     This is the receiving function. It is static because it is going to 
     execute in a separate thread.
  */
# define RECEPTION_BUFFER_SIZE 4096
  
  static void * locator_deamon_engine(void * _loc_com_point_addr)
    throw(std::exception, SizeFault)
  {
    typedef Msg_Entry_Header  DMEH; 
    Remote_Multiserver_Point   * ptr_loc_comm_point = 
      static_cast<Remote_Multiserver_Point *>(_loc_com_point_addr);
    
    char                     message_buffer[RECEPTION_BUFFER_SIZE]; 
    
    RawMsg                   incomming_message(&message_buffer[0], 
					       RECEPTION_BUFFER_SIZE,
					       RawMsg::DEFAULT_FLAG);
    
    Remote_Multiserver_Binding<Service_Class>     * ptr_client_binding;
    
    // this message is going to be freed later on other rutine.  
    void                     * message_to_dispatcher;
    
    // while is not required to be shutdow
    for ( ; !ptr_loc_comm_point->is_shutdown ;  ) 
      {
	MESSAGE("*** Waiting for Remote Requests ***");
	
	incomming_message.setBodySize(RECEPTION_BUFFER_SIZE);
	
	// Wait for incoming message
	ptr_loc_comm_point->dispatched_message_id = ptr_loc_comm_point->
	  ptr_reception_deamon->receive(incomming_message);
	
	if (incomming_message.getBodySize() > RECEPTION_BUFFER_SIZE)
	  Static_Throw (SizeFault) (incomming_message.getBodySize());
	
	//    take off the system message from ipc message
	message_to_dispatcher = Malloc(incomming_message.getBodySize());
	
	//    copy the system message into new location
	memcpy(message_to_dispatcher, 
	       incomming_message.getBodyAddr(), 
	       incomming_message.getBodySize());
	
	// Ask for message type: is it rpc_send or single_message.
	// Depending on that, it will be seen whether it is  necesary 
	// or not to allocate a remote_binding.
	switch (incomming_message.getFlags())
	  {
	  case RawMsg::RPC_REQUEST :
	    //    put into the binding list the new request for service.
	    //    It will be freed later at destruction time.
	    ptr_client_binding = 
	      AllocNew(ptr_loc_comm_point->alloc, 
		       Remote_Multiserver_Binding<Service_Class>) 
	      (ptr_loc_comm_point, ptr_loc_comm_point->dispatched_message_id);   
	    ptr_loc_comm_point->
	      binding_list.append(AllocNew(ptr_loc_comm_point->alloc, 
					   Binding_Node)
				  (ptr_client_binding));
	    break;
	  case RawMsg::RPC_REPLY :
	    //   MESSAGE("An RPC_REPLY");
	    //   for this type of message it is necesary an asyncronous service.
	    ptr_client_binding = NULL;
	    break;
	  case RawMsg::SINGLE_MESSAGE :
	    //   for this type of message it is necesary an asyncronous service.
	    MESSAGE("A SINGLE_MESSAGE");
	    ptr_client_binding = NULL;
	    break;
	  default:
	    ERROR("Panic: Receiving unidentified message!");
	  }
	
	//    check for binding-list entries ready to be freed.
	ptr_loc_comm_point->binding_list_freeing_fct(); 
	
	//    pass it to the dispatcher
	ptr_loc_comm_point->dispatcher(reinterpret_cast<DMEH*>
				       (message_to_dispatcher),
				       ptr_client_binding); 
      }
    return 0;
  }
# undef RECEPTION_BUFFER_SIZE

  class Service
  {    
    char             service_name[80];

    Service()
    {
      // empty
    }
    
  protected:
    Service(char * _service_name)
    {
      strcpy(service_name, _service_name);
    }
    
  public:
    char * get_service_name() const
    {
      return const_cast<char*>(service_name);
    }

    virtual long who_am_i() = 0;

    virtual int 
    execute_service(Msg_Entry_Header *, 
		    Remote_Multiserver_Binding<Service_Class> *) = 0;
  };

  friend class Async_Service : public Service
  {
    Async_Service_Fct async_fct;

    Remote_Multiserver_Point<Service_Class> * ptr_rmp;

  public:
    Async_Service(char * _serv_name,
		  Async_Service_Fct _async_fct,
		  Remote_Multiserver_Point<Service_Class> * _ptr_rmp) :
      Service(_serv_name),
      async_fct(_async_fct),
      ptr_rmp(_ptr_rmp)
    {
      // empty
    }
      
    virtual int 
    execute_service(Msg_Entry_Header * ptr_entry_header,
		    Remote_Multiserver_Binding<Service_Class> * 
		                                        return_point = NULL)
    {
      ASSERT(return_point == NULL);

      return ((ptr_rmp->ptr_service_class)->*async_fct)(ptr_entry_header);
    }    

    virtual long who_am_i()
    {
      return LOCCOM_ASYNC;
    }
  };
  
  friend class Sync_Service : public Service
  {
    Sync_Service_Fct sync_fct;

    Remote_Multiserver_Point<Service_Class> * ptr_rmp;

  public:
    Sync_Service(char * _serv_name,
		 Sync_Service_Fct _sync_fct,
		 Remote_Multiserver_Point<Service_Class> * _ptr_rmp) :
      Service(_serv_name),
      sync_fct(_sync_fct),
      ptr_rmp(_ptr_rmp)
    {
      // empty
    }
      
    virtual int 
    execute_service(Msg_Entry_Header * ptr_entry_header,
		    Remote_Multiserver_Binding<Service_Class> * return_point)
    {
      ASSERT(return_point != NULL);

      return ((ptr_rmp->ptr_service_class)->*sync_fct)
	(ptr_entry_header, return_point);
    }

    virtual long who_am_i()
    {
      return LOCCOM_RPC;
    }
  };

  Dlink binding_list;
  
  struct Binding_Node : public Dlink 
  {
    Remote_Multiserver_Binding<Service_Class> * binding_point;
    
    Binding_Node(Remote_Multiserver_Binding<Service_Class> * _binding_point) :
      binding_point(_binding_point)
      {
	// empty
      }
  };

  void binding_list_freeing_fct()
  {
    Dlink * list_cursor;
    
    for ( list_cursor = binding_list.getNext(); 
	  list_cursor != &binding_list && !binding_list.isEmpty(); )
      {
	if (static_cast<Binding_Node *>(list_cursor)->
	    binding_point->get_still_busy() == false || is_shutdown)
	  {
	    AllocDelete(alloc, static_cast<Binding_Node *>
			(list_cursor)->binding_point);
	    
	  list_cursor = list_cursor->getNext();
	  
	  AllocDelete(alloc, static_cast<Binding_Node *>
		      (list_cursor->removePrev()));
	  }
	else 
	  list_cursor = list_cursor->getNext();
    }
  }
  
  /// number of services you pretend to have in your server.
  long number_of_services;

  /** 
      Number of services you have actually registered. It is going to be
      incremented as services are being registered.
  */
  long registered_services;

  GenAlloc & alloc;

  /**
     Table of services.
  */

  Service ** table;


  /**
     The following two routines manages the variable
     active_services. This is done in order to have a proper shutting
     down of the server when required.  
  */
  void increase_active_services();
  
  void decrease_active_services();

  MsgId dispatched_message_id;

public:  
  
  Remote_Multiserver_Point(long no_of_services,
			   Service_Class * const _ptr_serv_class,
			   GenAlloc & _alloc = *objectAlloc) :
    ptr_service_class(_ptr_serv_class),
    is_shutdown(false),
    active_services(0),
    number_of_services(no_of_services),
    registered_services(0),
    alloc(_alloc)
  {
    ASSERT(number_of_services > 0);
    ASSERT(_ptr_serv_class != NULL);

    table = AllocNew (alloc, Service *) [no_of_services];
    
    for (long i = 0; i < no_of_services; i++)
      table[i] = NULL;
    
    pthread_mutex_init(&shutdown_lock, NULL);  
    
    pthread_mutex_init(&lock_send, NULL);
    
    pthread_cond_init(&wait_for_shutdown, NULL);
    
    ptr_reception_deamon = AllocNew(alloc, Ipc_MultiClient_Server);
    
    char strP[Port::stringSize];
    
    MESSAGE("Locator Port: %s", ptr_reception_deamon 
	    ->get_port().getStringPort(strP, Port::stringSize));
  }

  /** 
     Starts the deamon in a separate thread. So, remember the
     call to start_deamon() is a non-blocking one. You could also
     save the address of the thread in case you want to join it
     later.
  */  
  pthread_t * start_deamon()
    throw (std::exception, UnexpectedException)
  {
    if (pthread_create(&deamon_thread, NULL, locator_deamon_engine, this) != 0)
      Throw (UnexpectedException) ();
    
    return &deamon_thread;
  }
  
  /**
    This routine registers a service at server side.
    You have to be coherent with User_Service_Codes, and the string
    which helps to indentify a service specified in Service_Fct.

    Also remember that Service_Fct is of the type:
    typedef int (*Service_Fct) (Local_Binding, Local_Msg_Entry_Header *);

    See also the example testServer.C which shows the form of the service
    routine.    
  */
  
  void include_service(long servc_code, Service * service)
    throw (std::exception, Duplicated)
  {
    ASSERT(servc_code < number_of_services);
    
    if (table[servc_code] == NULL)
      {
	ASSERT(registered_services < number_of_services);
	
	/* service not found, therefore register it */
	table[servc_code] = service;
	
	registered_services++;
	
	// MESSAGE("Service %s registered.", service->get_service_name());
      }
    else
      {
	MESSAGE("ERROR: It seems that you intended a duplicated insertion!");
	
	AllocDelete(alloc, service);
	
	Throw (Duplicated) ();
      }
  }
  

  void  
  add_service(long servc_code, char * _serv_name, Async_Service_Fct a_service)
  {
    Service * serv = 
      AllocNew(alloc, Async_Service)(_serv_name, a_service, this);
    
    include_service(servc_code, serv);
  }
  
  void 
  add_service(long servc_code, char* _serv_name, Sync_Service_Fct s_service)
  {
    Service * serv = 
      AllocNew(alloc, Sync_Service)(_serv_name, s_service, this);
    
    include_service(servc_code, serv);
  }
  

  
  void async_send(const Port & target_port, Msg_Entry_Header * request) 
    throw (std::exception, NotFound)
  {
    ASSERT((request->get_deamon_service_code() == DEMSERV_SHUTDOWN) ||
	   (table[request->get_client_service_code()]->who_am_i() == 
	    LOCCOM_ASYNC));

    CRITICAL_SECTION(lock_send);

    try 
      {
	sending_check(target_port, 
		      request, 
		      sizeof(Msg_Entry_Header) + 
		      request->get_parameters_size(), 
		      LOCCOM_ASYNC);
      }
    catch (...)
      {
	throw;
      }
  }
  
  void rpc_send(const Port & target_port, Msg_Entry_Header * request,
		size_t request_size) 
    throw (std::exception, NotFound)
  {
    ASSERT(table[request->get_client_service_code()]->who_am_i() == 
	   LOCCOM_RPC);

    CRITICAL_SECTION(lock_send);
    
    try 
      {
	sending_check(target_port, request, request_size, LOCCOM_RPC);
      }
    catch (...)
      {
	throw;
      }
  }
  
  
  void rpc_send(const Port & target_port, Msg_Entry_Header * request,
		size_t request_size, void * invocation, size_t invocation_size)
    throw (std::exception, NotFound)
  {
    ASSERT(table[request->get_client_service_code()]->who_am_i() == 
	   LOCCOM_RPC);
    
    CRITICAL_SECTION(lock_send);
    
    // size_t size = sizeof(Msg_Entry_Header) + request->get_parameters_size(); 
    size_t message_size = request_size + invocation_size;
    
    void * message = Malloc(message_size);
    
    // copy the first part of the message
    memcpy(message, request, request_size);
    
    // copy the second part of the message
    memcpy(static_cast<char*>(message) + request_size, 
	   invocation, invocation_size);
    
    // write it to the correspondent reception point
    try 
      {
	sending_check(target_port, 
		      reinterpret_cast<Msg_Entry_Header *>(message),
		      message_size,
		      LOCCOM_RPC);
      }
    catch (...)
      {
	throw;
      }
    
    Free(message);
  }
  

// BE AWARE OF THIS:
// The correctness responsability belongs to the client, for this 
// method. The service being invoked in completly unknown by this
// class.

  MsgId 
  rpc_send_to_other_server(const Port & target_port,
			   Msg_Entry_Header * request,
			   size_t request_size,
			   void * invocation, 
			   size_t invocation_size) 
  {
    CRITICAL_SECTION(lock_send);
    
    // size_t size = sizeof(Msg_Entry_Header) + request->get_parameters_size(); 
    size_t message_size = request_size + invocation_size;
    
    void * message = Malloc(message_size);
    
    // copy the first part of the message
    memcpy(message, request, request_size);
    
    ASSERT(invocation != NULL);
    
    ASSERT(invocation_size > 0);
    
    // copy the second part of the message
    memcpy(static_cast<char*>(message) + request_size, 
	   invocation, invocation_size);
    
    ASSERT(reinterpret_cast<Msg_Entry_Header*>(message)->
	   get_parameters_size() >= 0);
    
    RawMsg request_message(message, message_size);
    
    MsgId msg_id = 
      ptr_reception_deamon->rpc_send_request(target_port, request_message);
    
    Free(message);
    
    return msg_id;
  }
  
  MsgId 
  rpc_send_to_other_server(const Port & target_port,
			   Msg_Entry_Header * request,
			   size_t request_size)
  {
    CRITICAL_SECTION(lock_send);
    
    ASSERT(request->get_parameters_size() >= 0);
    
    RawMsg request_message(request, request_size);
    
    MsgId msg_id = 
      ptr_reception_deamon->rpc_send_request(target_port, request_message); 
    
    return msg_id;
  }
  

  MsgId 
  async_send_to_other_server(const Port & target_port,
			     Msg_Entry_Header * request,
			     size_t request_size)
  {
    CRITICAL_SECTION(lock_send);
    
    ASSERT(request->get_parameters_size() >= 0);
    
    RawMsg request_message(request, request_size);
    
    MsgId msg_id = 
      ptr_reception_deamon->send_message(target_port, request_message); 
    
    return msg_id;
  }

  MsgId
  async_send_to_other_server(const Port & target_port,
			     Msg_Entry_Header * request,
			     size_t request_size,
			     void * invocation, 
			     size_t invocation_size) 
  {
    CRITICAL_SECTION(lock_send);
    
    // size_t size = sizeof(Msg_Entry_Header) + request->get_parameters_size(); 
    size_t message_size = request_size + invocation_size;
    
    void * message = Malloc(message_size);
    
    // copy the first part of the message
    memcpy(message, request, request_size);
    
    ASSERT(invocation != NULL);
    
    ASSERT(invocation_size > 0);
    
    // copy the second part of the message
    memcpy(static_cast<char*>(message) + request_size, 
	   invocation, invocation_size);
    
    ASSERT(reinterpret_cast<Msg_Entry_Header*>(message)->
	   get_parameters_size() >= 0);
    
    RawMsg request_message(message, message_size);
    
    MsgId msg_id = 
      ptr_reception_deamon->send_message(target_port, request_message);
    
    Free(message);
    
    return msg_id;
  }


  /** 
      Deletes a service at server's side, so that a new request after
      the deletion can be completed.
  */
  void delete_service(const long service_code)
    throw (std::exception, NotFound)
  {
    if (table[service_code] == NULL)
      Throw (NotFound) ();
    else
      {
	AllocDelete (alloc, table[service_code]);
	table[service_code] = NULL;
      }
  }
  
  
  /**
    It Shuts down the server. Before the complete shutting down, the server
    accomplish all the pending requests and then disapears.
    Future client requests are going to get a non-reachable server
    error, that must be handled at clients side.
  */  
  void shutdown()
  {
    ASSERT(is_shutdown == false);
    
    //  void * result;
    
    Shutdown_Message shutdown_msg;
    
    // sending the shutdown message.
    Port this_port(ptr_reception_deamon->get_port());
    
    // self sending of a message.  
    async_send(this_port, &shutdown_msg);
    
    pthread_mutex_lock(&shutdown_lock);
  
    if (!is_shutdown)
      pthread_cond_wait(&wait_for_shutdown, &shutdown_lock);
  
    pthread_mutex_unlock(&shutdown_lock);
    
    // waiting for normal termination (through shutdown flag).
    // pthread_join(deamon_thread, (void **)&result);
    
    // free the registering table  
    for (long i = 0; i < number_of_services; i++)
      if (table[i] != NULL)
	AllocDelete (alloc, table[i]);
    
    AllocDeleteArray (alloc, table);
    
    /* destoy mutex and condition variable */
    
    pthread_mutex_destroy(&shutdown_lock);
    
    pthread_cond_destroy(&wait_for_shutdown);
    
    // Destroy the binding points who last from the last freeing burst. 
    binding_list_freeing_fct();
    
    AllocDelete(alloc, ptr_reception_deamon);
    
    MESSAGE("The Remote Communication Thread has been shutdown...");
  }
  

  const Port & get_port() const
  {
    return  ptr_reception_deamon->get_port();
  }

  const MsgId get_dispatched_message_id() const
  {
    return dispatched_message_id;
  }

  /// Invokes shutdown() 
  virtual ~Remote_Multiserver_Point()
  {
    if (!is_shutdown)
      shutdown();
  }
  
};

#endif // LOCATOR_COMM_POINT


