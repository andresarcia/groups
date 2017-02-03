# include "timeoutQueue.H"
# include <time.h>
# include <aleph.H>
# include "broadcast_daemon.H"

enum Event_Code
{
  BROADCAST_ANSWER_TIME_EXPIRED,

  BROADCAST_LAST
};

template<class Node_Type, class Key_Type>
Node_Type * search_elem(Dlink & centinel_node,
			const Key_Type & key)
{
  Dlink * cursor = &centinel_node;
  
  for (cursor = cursor->getNext(); 
       cursor != &centinel_node; 
       cursor = cursor->getNext())
    {
      char * key_inside_node = 
	reinterpret_cast<char *>(cursor) + sizeof(Dlink); 

      if (key == *reinterpret_cast<Key_Type*>(key_inside_node))
	return static_cast<Node_Type*>(cursor);
    }

  return NULL;
}


class Msg_Id_Sqc_Nbr_Map_Bucket : 
  public ProtectedLhashTable<MsgId>::Bucket
{
  long sequence_number;

  Broadcast_Answer_Time_Expired * ptr_time_expired_event;

public:
  
  Msg_Id_Sqc_Nbr_Map_Bucket(const MsgId & _msg_id,
			    const long _sqc_number,
			    Broadcast_Answer_Time_Expired * const _event) :
    ProtectedLhashTable<MsgId>::Bucket(_msg_id),
				sequence_number(_sqc_number),
				ptr_time_expired_event(_event)
  {
    // empty
  }

  const long get_sequence_number() const
  {
    return sequence_number;
  }

  Broadcast_Answer_Time_Expired * get_ptr_time_expired_event() const
  {
    return ptr_time_expired_event;
  }

  ~Msg_Id_Sqc_Nbr_Map_Bucket()
  {
    AllocDelete(*objectAlloc, ptr_time_expired_event);
  }
};




class Wait_For_Responses_Bucket : 
  public ProtectedLhashTable<long>::Bucket
{
  long client_return_service_code;

  long total_members_snapshot;

  long number_of_expected_responses;

  long current_number_of_responses;

  long current_number_of_lost_responses;

  char * buffer_for_response;

  int current_buffer_size;

  Remote_Multiserver_Binding<Broadcast_Daemon> * waiting_client_port;
  
public:
  Wait_For_Responses_Bucket(const long _client_return_service_code,
			    const long _total_members_snapshot,
			    const long _sequence_number,
			    const long _exp_resp,
			    Remote_Multiserver_Binding<Broadcast_Daemon> * _client) :
    ProtectedLhashTable<long>::Bucket(_sequence_number),
		 client_return_service_code(_client_return_service_code),
		 total_members_snapshot(_total_members_snapshot),
		 number_of_expected_responses(_exp_resp),
		 current_number_of_responses(0),
		 current_number_of_lost_responses(0),
		 current_buffer_size(0),
		 waiting_client_port(_client)
  {
    ASSERT(number_of_expected_responses > 0);
    ASSERT(waiting_client_port != NULL);
    buffer_for_response = new char [response_buffer_size];
  }
  
  const long get_total_members_snapshot() const 
  {
    return total_members_snapshot;
  }

  const long get_number_of_expected_responses() const 
  {
    return number_of_expected_responses;
  }

  const long get_current_number_of_responses() const
  {
    return current_number_of_responses;
  }

  const long get_current_number_lost_of_responses() const
  {
    return current_number_of_lost_responses;
  }

  const size_t get_current_buffer_size() const
  {
    return current_buffer_size;
  }
  
  char * get_buffer_for_response() 
  {
    return buffer_for_response;
  }

  const long get_client_return_service_code() const
  {
    return client_return_service_code;
  }

  Remote_Multiserver_Binding<Broadcast_Daemon> * 
  get_waiting_client_port() const
  {
    return waiting_client_port;
  }

  void add_lost_response() 
  {
    current_number_of_lost_responses++;
  }

  void add_new_response(const void * response, int response_size)
  {
    ASSERT(response != NULL);
    ASSERT(response_size > 0);
    ASSERT(current_buffer_size + response_size <= response_buffer_size);

    memcpy(buffer_for_response + current_buffer_size,
	   response,
	   response_size);

    current_buffer_size += response_size;

    current_number_of_responses++;
  }

  bool ready_to_respond() const
  {
    ASSERT(current_number_of_responses +
	   current_number_of_lost_responses <= number_of_expected_responses);

    return ((current_number_of_responses + current_number_of_lost_responses) 
	    == number_of_expected_responses);
  }

  ~Wait_For_Responses_Bucket()
  {
    delete [] buffer_for_response ;
  }
};


class Broadcast_Timeout_Event : public TimeoutQueue::Event
{
  Event_Code event_code;

  MsgId msg_id;

public:
  Broadcast_Timeout_Event(const Event_Code _event_code,
		      const struct timespec & _exec_time,
		      const MsgId & _msg_id) :
    TimeoutQueue::Event(_exec_time),
		  event_code(_event_code),
		  msg_id(_msg_id)
  {
    // empty
  }

  const Event_Code get_event_code() const
  {
    return event_code;
  }

  const MsgId get_msg_id() const
  {
    return msg_id;
  }
};

class Broadcast_Answer_Time_Expired : public Broadcast_Timeout_Event
{
# define ACCESS(member) ptr_broadcast_daemon->member

  Broadcast_Daemon * ptr_broadcast_daemon;

public:
  Broadcast_Answer_Time_Expired(Broadcast_Daemon * _ptr_broadcast_daemon,
				const struct timespec & _exec_time,
				const MsgId & _msg_id) :
    Broadcast_Timeout_Event(BROADCAST_ANSWER_TIME_EXPIRED, 
			    _exec_time, _msg_id),
    ptr_broadcast_daemon(_ptr_broadcast_daemon)
  {
    // empty
  }

  virtual void EventFct()
  {
    CRITICAL_SECTION(ACCESS(broadcast_mutex));

    // search msg_id entry in message table
    Msg_Id_Sqc_Nbr_Map_Bucket * 
      searched_msg = static_cast<Msg_Id_Sqc_Nbr_Map_Bucket*>
      (ACCESS(msg_id_sqc_nbr_map_table).search(get_msg_id()));
    
    if (searched_msg == NULL) 
      // response is late and it was taken by "receive_client_response"
      return;
    
    searched_msg->unLock();
    
    // search waiting_response entry and update lost response counter
    Wait_For_Responses_Bucket * 
      searched_bucket = static_cast<Wait_For_Responses_Bucket *>
      (ACCESS(client_response_table).
       search(searched_msg->get_sequence_number()));
    
    // remove msg_id entry 
    ACCESS(msg_id_sqc_nbr_map_table).remove(searched_msg);

    // the expired event is deleted in searched_msg destructor 
    AllocDelete(*objectAlloc, searched_msg);

    if (searched_bucket == NULL) 
      // whether response is complete and has been delivered to client or
      // have been expired enough timeouts, and the answer has also been 
      // delivered.
      return;
    
    searched_bucket->unLock();
  
    searched_bucket->add_lost_response();

    // check if deamon is ready to respond to client and respond if necesary
    if (searched_bucket->ready_to_respond())
      {
	Responses_RetMsg 
	  return_msg(searched_bucket->get_client_return_service_code(),
		     GRP_OK,
		     searched_bucket->get_total_members_snapshot(),
		     searched_bucket->get_number_of_expected_responses(),
		     searched_bucket->get_current_number_of_responses(),
		     searched_bucket->get_current_buffer_size());

	Msg_Entry_Header * msg_to_return = 
	  static_cast<Msg_Entry_Header *>(&return_msg);

	searched_bucket->get_waiting_client_port()->
	  rpc_reply(msg_to_return,
		    sizeof(Responses_RetMsg),
		    searched_bucket->get_buffer_for_response(),
		    searched_bucket->get_current_buffer_size());

	ACCESS(client_response_table).remove(searched_bucket);

	AllocDelete(*objectAlloc, searched_bucket);
      }
  }
# undef ACCESS
};
