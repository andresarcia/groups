# ifndef LOCATOR_COMM_BINDING_H
# define LOCATOR_COMM_BINDING_H

# include "base_message_header.H"
# include <pthread.h>
# include "ipcClient.H"

template <class Service_Class> 
class Remote_Multiserver_Point;

template <class Service_Class>
class Remote_Multiserver_Binding
{
  Remote_Multiserver_Binding()
    {
      // empty
    }

  /*
    Determines wheter the service has been accomplish (false) or not (true). 
    This information is used when shutting down the Remote_Multiserver_Point. 
  */

  bool still_busy; 

  Remote_Multiserver_Point<Service_Class> * link_to_remote_multiserver_point;
 
  /**
    To be constructed Remote_Multiserver_Binding needs a Remote_Multiserver_Point
    pointer. The Local_Reception_Point works as a communication link
    to the Server attending the services.  This link is important
    because it helps with the control of the active services.  
  */

  MsgId message_id;

  void write_to_client(void * data, size_t data_size)
  {
    RawMsg reply_msg(data, data_size);
    
    MESSAGE("responding data_size %i", data_size);
    
    link_to_remote_multiserver_point->
      ptr_reception_deamon->rpc_send_reply(reply_msg, message_id);
  }

public:

  Remote_Multiserver_Binding
  (Remote_Multiserver_Point<Service_Class> * dem_rec_point, 
   MsgId _message_id ) :
    // It is true because the service hasn't been accomplished 
    still_busy(true),
    link_to_remote_multiserver_point(dem_rec_point),
    message_id(_message_id)
  {
    // Empty 
  }

  /**
     Remember from loccom_message_header.H the response class has the form:
     class Your_Inherited_Class : public Local_Msg_Exit_Header
     {
     ...
     };
     
     Since you already have a Response class which has inherited  
     Local_Msg_Exit_Header, then you simply 
     respond(your_inherited_class, sizeof(your_inherited_class)).
     
     this is because your_inherited_class is_a Local_Msg_Exit_Header.
  */

  void increase_active_services();
  
  void decrease_active_services();

  void set_still_busy(const bool _still_busy)
    {
      still_busy = _still_busy;
    }

  const bool get_still_busy() const
    {
      return still_busy;
    }

  ssize_t rpc_reply (Msg_Entry_Header * data, 
		     const size_t data_size)  
  {
    ASSERT (data_size >= sizeof(Msg_Entry_Header));
    
    write_to_client(data, data_size);
    
    // Now, the service has been accomplished.
    set_still_busy(false);
    
    return 0; 
  }
  
  ssize_t rpc_reply (Msg_Entry_Header * data, 
		     const size_t data_size,
		     void * buffer,
		     const size_t buffer_size)  
  {
    ASSERT (data_size >= sizeof(Msg_Entry_Header));
    
    ASSERT (buffer != NULL || buffer_size == 0);
    
    // data_size accounts for sizeof(Msg_Exit_Header) + sizeof(data).
    void * message = Malloc(data_size + buffer_size);
    
    // copy the first part of the message
    memcpy(message, data, data_size);
    
    // copy the second part of the message
    if ((buffer_size > 0) && (buffer != NULL))
    memcpy(static_cast<char*>(message) + data_size, buffer, buffer_size);

    // write it to the correspondant reception point
    write_to_client(message, data_size + buffer_size);
    
    Free(message);
    
    // Now, the service has been accomplished.
    set_still_busy(false);
    
    return 0;
  }
  
  ~Remote_Multiserver_Binding()
  {
    // empty
  }
};

# endif // LOCATOR_COMM_BINDING_H










