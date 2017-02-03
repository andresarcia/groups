# ifndef REMOTE_MESSAGE_HEADER_H
# define REMOTE_MESSAGE_HEADER_H

# include "binder.H"
# include "base_message_header.H"

enum Remote_Locator_Call_Code
{
  /**
     Remote service codes. These are the codes that are going to
     be sent betwen locators. So, they are going to be part of the
     messages handled by the IPC deamon.
  */

  INVOCATION_REQUEST,
  INVOCATION_REPLY,
  FIND_OWNER,
  GATHER_OBJECT_INFORMATION,
  RESPONSE_FROM_FIND_OWNER,
  RESPOSNE_FROM_GATHER,
  LOCATOR_ANOUNCE,
  DEAD_ANOUNCE,
  PING_N_OBJECTS_REQUEST,
  PING_REPLY_FOR_N_OBJECTS,
  RECEIVE_CACHE_IN,
  
  REMOTE_LOCATOR_CALL_CODE_LAST // the last code.
};

enum Invocation_Reply_Type
{
  LOCREPLY_SUCCESS,
  LOCREPLY_NOT_FOUND,
  LOCREPLY_RECENT_BINDING,
  LOCREPLY_DEAD,

  LOCREPLY_CODE_LAST
};



struct Cache_In_Data 
{
  Site_Id           source_site_id;
  Logical_Timestamp timestamp;

  Cache_In_Data(const Site_Id & _src_site, 
		const Logical_Timestamp & _timestamp) :
    source_site_id(_src_site),
    timestamp(_timestamp)
  {
    // empty
  }

  Cache_In_Data()
  {
    // empty
  }

  bool operator == (const Cache_In_Data & right_hand_op) const 
  {
    return source_site_id == right_hand_op.source_site_id;
  }
};


// TODO: Missing to include the implicit messages.

class Invocation_Request_Msg : public Msg_Entry_Header
{
  size_t     request_size;
  Binding    binding;
  Message_Id message_id;
  char       request[0];

public:

  Invocation_Request_Msg(size_t _request_size, 
			 const Binding & _binding,
			 const Message_Id & _message_id) :
    Msg_Entry_Header(INVOCATION_REQUEST, sizeof(*this) + _request_size),
    request_size(_request_size),
    binding(_binding),
    message_id(_message_id)
    {
      // empty
    }

  Binding & get_binding() 
  {
    return binding;
  }
  
  const void * get_request() const
  {
    return &request[0];
  }

  size_t get_request_size() const
  {
    return request_size;
  }

  const Message_Id & get_message_id() const
  {
    return message_id;
  }

};


class Invocation_Reply_Msg : public Msg_Entry_Header
{
  size_t  reply_size;
  Binding binding;
  Message_Id message_id;
  Invocation_Reply_Type reply_type;
  char    reply[0];
  
public:

  Invocation_Reply_Msg(size_t _reply_size, 
		       const Binding & _binding, 
		       const Message_Id & _msg_id,
		       const Invocation_Reply_Type & _reply_type) :
    Msg_Entry_Header(INVOCATION_REPLY, sizeof(*this) + _reply_size),
    reply_size(_reply_size),
    binding(_binding),
    message_id(_msg_id),
    reply_type(_reply_type)
    {
      // empty
    }

  size_t get_reply_size() const
    {
      return reply_size;
    }
  
  const Binding & get_binding() const
    {
      return binding;
    }

  const Message_Id & get_message_id() const
    {
      return message_id;
    }

  const void * get_reply() const 
  {
    return &reply[0];
  }

  void set_reply_type(Invocation_Reply_Type new_reply_type)
  {
    reply_type = new_reply_type;
  }

  const Invocation_Reply_Type get_reply_type() const
  {
    return reply_type;
  }
};

/*

class Find_Request : public Msg_Entry_Header 
{
  Binding binding;
};

class Object_Dead_Anounce : public Msg_Entry_Header
{
  Object_Id object_id;
};

*/

class Ping_N_Objects_Request : public Msg_Entry_Header
{
  unsigned int number_of_objects;
  unsigned int actual_number_of_obj;
  
  char locators[0];
  
public:
  Ping_N_Objects_Request(unsigned int _number_of_obj) :
    Msg_Entry_Header(PING_N_OBJECTS_REQUEST, sizeof(*this) + 
		     _number_of_obj * sizeof(Locator)),
    number_of_objects(_number_of_obj),
    actual_number_of_obj(0)
  {
    // empty
  }
  
  const char * get_locators() 
  {
    return locators;
  }

  void add_locator(const Locator & locator) 
  {
    ASSERT(actual_number_of_obj < number_of_objects);
    
    memcpy(reinterpret_cast<char*>(this)+
	   sizeof(*this)+
	   actual_number_of_obj * sizeof(Locator),
	   &locator, sizeof(Locator));
    
    actual_number_of_obj ++;	 
  }

  void reset_locators_counter() 
  {
    actual_number_of_obj = 0;
  }

  Locator & operator[] (unsigned int position)
  {
    ASSERT(position < number_of_objects);
    
    char * locator_pos = reinterpret_cast<char*>(this)+
      sizeof(*this) + position * sizeof(Locator);
    
    return *reinterpret_cast<Locator *>(locator_pos); 
  }

  unsigned int get_number_of_objects() const
  {
    return number_of_objects;
  }
};



class Ping_Reply_For_N_Objects : public Msg_Entry_Header
{
  unsigned number_of_responses;
  unsigned actual_number_of_response;
  
public:
  Ping_Reply_For_N_Objects(unsigned _n_of_resp) :
    Msg_Entry_Header(PING_REPLY_FOR_N_OBJECTS, sizeof(*this)),
    number_of_responses(_n_of_resp),
    actual_number_of_response(0)
  {
    // empty
  }

  unsigned get_number_of_responses() const
  {
    return number_of_responses;
  }

  void add_response(const Locator & locator,
		    const Invocation_Reply_Type resp_type) 
  {
    ASSERT(actual_number_of_response < number_of_responses);
    
    // copy the locator
    memcpy(reinterpret_cast<char*>(this) +
	   sizeof(*this) + 
	   actual_number_of_response * 
	   (sizeof(Locator) + sizeof(Invocation_Reply_Type)),
	   &locator, sizeof(Locator));
    
    // copy the response type
    memcpy(reinterpret_cast<char*>(this) +
	   sizeof(*this) + sizeof(Locator) +
	   actual_number_of_response * 
	   (sizeof(Locator) + sizeof(Invocation_Reply_Type)),
	   &resp_type, sizeof(Invocation_Reply_Type));
    
    actual_number_of_response ++;
  }

  const Locator & get_locator(unsigned position)
  {
    ASSERT(position < number_of_responses);
    
    const Locator * ptr_locator = 
      reinterpret_cast<Locator *>
      (reinterpret_cast<char*>(this) +
       sizeof(*this) + 
       position * (sizeof(Locator) + sizeof(Invocation_Reply_Type)));
    
    return *ptr_locator;
  }

  const Invocation_Reply_Type get_reply_type(unsigned position)
  {
     ASSERT(position < number_of_responses);

     const Invocation_Reply_Type reply_type = 
       *reinterpret_cast<Invocation_Reply_Type *>
       (reinterpret_cast<char*>(this) +
	sizeof(*this) + sizeof(Locator) +
	position * (sizeof(Locator) + sizeof(Invocation_Reply_Type)));
     
    return reply_type;
  }
  
};


/*
class Ping_Object_Request : public Msg_Entry_Header
{
  Message_Id message_id;
  Locator locator;

public:
  Ping_Object_Request(const Message_Id & _msg_id,
		      const Locator & _locator) :
    Msg_Entry_Header(PING_REQUEST, sizeof(*this)),
    message_id(_msg_id),
    locator(_locator)
  {
    // empty
  }

  Locator & get_locator() 
  {
    return locator;
  }

  const Message_Id & get_message_id() const
  {
    return message_id;
  }
};

*/

class Find_RetMsg : public Msg_Entry_Header
{
  long client_id;
  Locator locator;
  Invocation_Reply_Type ping_reply_type;

public:
  Ping_Object_Reply(const long _client_id,
		    const Locator & _locator,
		    const Invocation_Reply_Type _reply_type):
    Msg_Entry_Header(PING_REPLY, sizeof(*this)),
    client_id(_client_id),
    locator(_locator),
    ping_reply_type(_reply_type)
  {
    // empty
  }
  
  const Locator & get_locator() const
  {
    return locator;
  }
  
  const Invocation_Reply_Type get_ping_reply_type() const
  {
    return ping_reply_type;
  }

  const long get_client_id() const
  {
    return client_id;
  }
};


class Locator_Anounce : public Msg_Entry_Header
{
  Locator locator;

public:
  
  Locator_Anounce(const Locator & _locator) :
    Msg_Entry_Header(LOCATOR_ANOUNCE, sizeof(*this)),
    locator(_locator)
  {
    // empty
  }

  const Locator & get_locator() const 
  {
    return locator;
  }
};



class Cache_In_Entries : public Msg_Entry_Header
{
  Object_Id object_id;
  unsigned number_of_entries;
  char entries[0];

public:

  Cache_In_Entries(const Object_Id & _obj_id,
		   unsigned _numb_of_ent) :
    Msg_Entry_Header(RECEIVE_CACHE_IN, 
		     sizeof(*this) + 
		     _numb_of_ent * sizeof(struct Cache_In_Data)),
    object_id(_obj_id),
    number_of_entries(_numb_of_ent)
  {
    ASSERT(number_of_entries >= 0);
  }
  
  const Object_Id & get_object_id() const
  {
    return object_id;
  }

  unsigned get_number_of_entries() const
  {
    return number_of_entries;
  }

  const char * get_entries()
  {
    if (number_of_entries > 0)
      return entries;
    else
      return NULL;
  }
};


# endif // REMOTE_MESSAGE_HEADER_H







