# ifndef BROADCAST_DAEMON_H
# define BROADCAST_DAEMON_H

# include "useMutex.H"
# include "signal.H"
# include "basic_defs.H"
# include "timeoutQueue.H"

# define NUMBER_OF_SRVCS 8

class Broadcast_Daemon 
{
  friend class Broadcast_Answer_Time_Expired;
  
  Remote_Multiserver_Point<Broadcast_Daemon> reception_point;
  
  Dlink clients;
  
  pthread_t * ptr_reception_thread;
  
  pthread_t timeout_thread;
  // this table stores all the information related to a multicast query.
  // Once the request is completed, all the collected information will be
  // handled to the correspondant client.
  ProtectedLhashTable<long> client_response_table;
  
  // hash table that maps message ids to sequence numbers. That means, that
  // every message sent to avery group member is associated to a secuence 
  // number, so that multiple resquests to group members can be simultaniously
  // done by many clients.
  ProtectedLhashTable<MsgId> msg_id_sqc_nbr_map_table;
  
  pthread_mutex_t broadcast_mutex;
  
  long current_number_of_clients;
  
  long sequence_number;
  
  TimeoutQueue timeout_queue;

  static int daemon_instances;
  
  static unsigned long long_hash_fct(const long & key)
  {
    return static_cast<unsigned long>(key);
  }
  
  static unsigned long broadcast_hash_fct(const MsgId & key)
  {
    return static_cast<unsigned long>(key.getTimeNsec());
  }

  long get_new_sequence_number()
  {
    return sequence_number++;
  }
  
  class Broadcast_Client : public Dlink
  {
    Port client_port;
    
  public:
    
    Broadcast_Client(const Port & _port_id) :
      client_port(_port_id)
    {
      // empty
    }
    
    const Port & get_client_port() const
    {
      return client_port;
    }
  };


  int create_group(Msg_Entry_Header *,
		   Remote_Multiserver_Binding<Broadcast_Daemon> *);
  
  int delete_group(Msg_Entry_Header *,
		   Remote_Multiserver_Binding<Broadcast_Daemon> *);
  
  int join_group(Msg_Entry_Header *,
		 Remote_Multiserver_Binding<Broadcast_Daemon> *);
  
  int leave_group(Msg_Entry_Header *,
		  Remote_Multiserver_Binding<Broadcast_Daemon> *);
  
  int broadcast_and_all_respond(Msg_Entry_Header *,
				Remote_Multiserver_Binding<Broadcast_Daemon> *);
  
  
  int broadcast_and_some_respond(Msg_Entry_Header *,
				 Remote_Multiserver_Binding<Broadcast_Daemon> *);

  int receive_client_response_from_all(Msg_Entry_Header *);
  
  int receive_client_response_from_some(Msg_Entry_Header *);

  void process_client_response(const MsgId & /*sent_msg_id*/,
			       const void * /*response*/,
			       const int /*response_size*/);
  
  
public:
  Broadcast_Daemon() :
    reception_point(NUMBER_OF_SRVCS, this),
    client_response_table(long_hash_fct, clients_table_size),
    msg_id_sqc_nbr_map_table(broadcast_hash_fct, conversion_table_size),
    current_number_of_clients(0),
    sequence_number(0)    
  {
    pthread_mutex_init(&broadcast_mutex, NULL);

    daemon_instances ++; 

    ASSERT(daemon_instances == 1);

    FILE * broad_port_file = fopen("/home/Aleph/.broadcast_port", "w");

    char str_port[Port::stringSize];
    
    reception_point.get_port().getStringPort(str_port, Uid::stringSize);
    
    fprintf(broad_port_file, "%s", str_port);	    

    fclose(broad_port_file);
    
    timeout_thread = timeout_queue.getThreadId();

    ptr_reception_thread = reception_point.start_deamon();

    reception_point.add_service(GRP_CREATE_GROUP, "create group", 
				&Broadcast_Daemon::create_group);
    
    reception_point.add_service(GRP_JOIN_GROUP, "join group", 
				&Broadcast_Daemon::join_group);
    
    reception_point.add_service(GRP_LEAVE_GROUP, "leave group", 
				&Broadcast_Daemon::leave_group);

    reception_point.add_service(GRP_DELETE_GROUP, "delete group", 
				&Broadcast_Daemon::delete_group);    

    reception_point.add_service(GRP_BROADCAST_AND_ALL_RESPOND,
				"broadcast and all respond", 
				&Broadcast_Daemon::broadcast_and_all_respond);
    
    reception_point.add_service(GRP_BROADCAST_AND_SOME_RESPOND,
				"broadcast and some respond", 
				&Broadcast_Daemon::broadcast_and_some_respond);
    
    reception_point.
      add_service(GRP_RECEIVE_CLIENT_RESPONSE_FROM_ALL,
		  "receive client response from all",
		  &Broadcast_Daemon::receive_client_response_from_all);

    reception_point.
      add_service(GRP_RECEIVE_CLIENT_RESPONSE_FROM_SOME,
		  "receive client response from some",
		  &Broadcast_Daemon::receive_client_response_from_some);
  }

  void join_broadcast_daemon()
  {
    pthread_join(timeout_thread, NULL);
    
    pthread_join(*ptr_reception_thread, NULL);    
  }

  void shutdown_broadcast_daemon()
  {
    timeout_queue.shutdown();

    reception_point.shutdown();
  }

  ~Broadcast_Daemon()
  {
    MESSAGE("Group Daemon has been shutdown!");

    // Delete all members from registering list.
    while (!clients.isEmpty())
      AllocDelete(*objectAlloc, 
		  static_cast<Broadcast_Client*>(clients.removeNext()));
    
    pthread_mutex_destroy(&broadcast_mutex);
  }
};

# endif 
