# ifndef BROADCAST_CLIENT_H
# define BROADCAST_CLIENT_H

# include "protected_lhash.H"
# include "broadcast_message_header.H"
# include "remote_multiserver_point.H"
# include "basic_defs.H"

# define DEFAULT_TABLE_SIZE 101

template <class Service_Class> 
class Broadcast_Client 
{
  Remote_Multiserver_Point<Service_Class> * client_port;

  Port broadcast_daemon_port;

  ProtectedLhashTable<Message_Id> waiting_for_response_table;

  static unsigned long broadcast_hash_fct(const Message_Id & key)
  {
    return static_cast<unsigned long>(key.get_random_number());
  }

public:
  
  Broadcast_Client(Remote_Multiserver_Point<Service_Class> * _client_port,
		   const Port & _broadcast_daemon_port) :
    client_port(_client_port),
    broadcast_daemon_port(_broadcast_daemon_port),
    waiting_for_response_table(broadcast_hash_fct,
			       DEFAULT_TABLE_SIZE)
  {
    // empty
  }
  
  // TODO:
  void create_group(Uid & broadcast_uid);

  void Broadcast_Client::join_group(const Port & listening_port) 
    throw (std::exception, Duplicated)
  {
    Ipc_MultiClient_Server join_client;
    
    Join_Broadcast_CallMsg join_msg(listening_port);
    
    RawMsg rpc_msg(&join_msg, sizeof(join_msg)); 
    
    MsgId send_msg_id = join_client.rpc_send_request(broadcast_daemon_port,rpc_msg);
    
# define BUFFER_SIZE 512
    
    char message_buffer[BUFFER_SIZE];
    
    RawMsg incomming_message(&message_buffer[0], BUFFER_SIZE,
			     RawMsg::DEFAULT_FLAG);
    
    MsgId reply_msg_id = join_client.receive(incomming_message);
    
    ASSERT(send_msg_id == reply_msg_id);
    
    Join_Broadcast_RetMsg * return_msg = reinterpret_cast<Join_Broadcast_RetMsg *>
      (incomming_message.getBodyAddr()); 
    
    ASSERT(return_msg->get_response_code() == GRP_RESPONSE_TO_JOIN);
    
    if (return_msg->get_error_code() == GRP_INTENDED_DOUBLE_SUSCRIPTION)
      Throw (Duplicated)();
    
    ASSERT(return_msg->get_error_code() == GRP_OK);
  }
  
# undef BUFFER_SIZE

# define TIMEOUT_NOT_NECESARY -1

  MsgId Broadcast_Client::strong_send_to_group(const long return_service_code,
					       void * message,
					       const size_t message_size)
  {
    Send_And_Receive_From_All_CallMsg request(client_port->get_port(),
					      return_service_code,
					      GRP_RELIABLE_SEND,
					      TIMEOUT_NOT_NECESARY,
					      message_size);
    
    return client_port->
      rpc_send_to_other_server(broadcast_daemon_port,
			       &request,
			       sizeof(Send_And_Receive_From_All_CallMsg),
			       message,
			       message_size);
  }
  
  
  MsgId weak_send_to_group(const long /*return_service_code*/,
			   const long /*timeout*/,
			   const void * /*message*/,
			   const size_t /*message_size*/); 
  
  MsgId strong_send_to_some(const long return_service_code,
			    void * message,
			    const size_t message_size,
			    const long number_of_exp_resp)
  {
    Send_And_Receive_From_Some_CallMsg request(client_port->get_port(),
					       return_service_code,
					       GRP_RELIABLE_SEND,
					       number_of_exp_resp,
					       TIMEOUT_NOT_NECESARY,
					       message_size);
    
    return client_port->
      rpc_send_to_other_server(broadcast_daemon_port,
			       &request,
			       sizeof(Send_And_Receive_From_Some_CallMsg),
			       message,
			       message_size);
  }

# undef TIMEOUT_NOT_NECESARY
  
  MsgId weak_send_to_some(const long /*return_service_code*/,
			 const long /*timeout*/,
			 const void * /*message*/,
			 const size_t /*message_size*/); 

  
  void Broadcast_Client::leave_group(const Port & listening_port) 
    throw (std::exception, NotFound)
  {
    Ipc_MultiClient_Server leave_client;
    
    Leave_Broadcast_CallMsg leave_msg(listening_port);
    
    RawMsg rpc_msg(&leave_msg, sizeof(leave_msg)); 
    
    MsgId send_msg_id = leave_client.rpc_send_request(broadcast_daemon_port,rpc_msg);
    
# define BUFFER_SIZE 512
    
    char message_buffer[BUFFER_SIZE];
    
    RawMsg incomming_message(&message_buffer[0], BUFFER_SIZE,
			     RawMsg::DEFAULT_FLAG);
    
    MsgId reply_msg_id = leave_client.receive(incomming_message);
    
    ASSERT(send_msg_id == reply_msg_id);
    
    Leave_Broadcast_RetMsg * 
      return_msg = reinterpret_cast<Leave_Broadcast_RetMsg *>
      (incomming_message.getBodyAddr()); 
    
    ASSERT(return_msg->get_response_code() == GRP_RESPONSE_TO_LEAVE);
    
    if (return_msg->get_error_code() == GRP_MEMBER_NOT_SUSCRIBED)
      Throw (NotFound)();
    
    ASSERT(return_msg->get_error_code() == GRP_OK);
  }

#undef BUFFER_SIZE

  // TODO:
  void delete_group(const Uid & broadcast_uid) 
    throw (std::exception, NotFound);

  virtual ~Broadcast_Client()
  {
    // empty
  }

};

# undef DEFAULT_TABLE_SIZE




# endif //BROADCAST_CLIENT_H

  





