# include "broadcast_client.H"
# include "remote_multiserver_point.H"
# include <string>

# define NUMBER_OF_SRVCS 3


enum Test_Services_Codes
{
  RECEIVE_FROM_BROADCAST_DAEMON,
  COOPERATIVE_SERVICE,
  COOPERATIVE_SERVICE_II,
  COOPERATIVE_SERVICE_RESPONSE,  
  SERVICE_FOUR,
    
  LAST
};

Port * ptr_broadcast_daemon_port;

class Cooperative_Question : public Msg_Entry_Header
{
  int client_id;

  int question_number;

public:
  Cooperative_Question(const int _client_id,
		       const int _question_number) :
    Msg_Entry_Header(COOPERATIVE_SERVICE, sizeof(*this)),
    client_id(_client_id),
    question_number(_question_number)
  {
    // empty
  }
  
  int get_client_id()
  {
    return client_id;
  }

  int get_question_number()
  {
    return question_number;
  }
};


class Cooperative_Question_2 : public Msg_Entry_Header
{
  int client_id;

  int question_number;

public:
  Cooperative_Question_2(const int _client_id,
			 const int _question_number) :
    Msg_Entry_Header(COOPERATIVE_SERVICE_II, sizeof(*this)),
    client_id(_client_id),
    question_number(_question_number)
  {
    // empty
  }
  
  int get_client_id()
  {
    return client_id;
  }

  int get_question_number()
  {
    return question_number;
  }
};


class Cooperative_Response : public Msg_Entry_Header
{
  int client_id;
  
  int question_number;
  
  int total_client_served;

public:
  Cooperative_Response(const int _client_id,
		       const int _question_number,
		       const int _total_client_served) :
    Msg_Entry_Header(COOPERATIVE_SERVICE_RESPONSE, sizeof(*this)),
    client_id(_client_id),
    question_number(_question_number),
    total_client_served(_total_client_served)
  {
    // empty
  }

  int get_client_id() const 
  {
    return client_id;
  }

  int get_question_number() const 
  {
    return question_number;
  }

  int get_total_client_served() const 
  {
    return total_client_served;
  }
};


class Test_Client : public Broadcast_Client<Test_Client>
{
  Remote_Multiserver_Point<Test_Client> services_point;

  int total_served_clients;

  pthread_t * services_thread;

  
  
  int receive_from_broadcast_daemon(Msg_Entry_Header * entry_msg)
  {
    MESSAGE(">>>> RECEIVING MULTICAST REQUEST <<<<");
    
    Responses_RetMsg * reply_msg = static_cast<Responses_RetMsg *>(entry_msg);
    
    MESSAGE("total members in group %i", 
	    reply_msg->get_total_members_in_group());
    
    MESSAGE("expected responses %i", 
	    reply_msg->get_expected_responses());
    
    MESSAGE("current_number_of_responses %i", 
	    reply_msg->get_current_number_of_responses());
    
    MESSAGE("responses_size %i", reply_msg->get_responses_size());

    char asc[40];

    if (reply_msg->get_current_number_of_responses() == 0)
      return 0;
    else 
      if (reply_msg->get_current_number_of_responses() < 0)
	ERROR("*** bad parameter value");

    int r_size = reply_msg->get_responses_size() / 
      reply_msg->get_current_number_of_responses();

    string responses;

    for (int n_resp = 0; n_resp < reply_msg->get_current_number_of_responses();
	 n_resp ++)
      {
	Cooperative_Response * coop_resp = 
	  reinterpret_cast<Cooperative_Response *>
	  (const_cast<char*>(reply_msg->get_responses()) + n_resp*r_size);

	sprintf(asc, "id %i-numb %i-%i | ", coop_resp->get_client_id(),
		coop_resp->get_question_number(),
		coop_resp->get_total_client_served());
	responses += asc;
      }

    MESSAGE("responses %s", responses.c_str());
    
    return 0;
  }
  
  int 
  cooperative_service (Msg_Entry_Header * entry_msg,
		       Remote_Multiserver_Binding<Test_Client> * return_point)
  {
    MESSAGE(">>> COOPERATIVE SERVICE <<<");

    Cooperative_Question * casted_input_msg = 
      static_cast<Cooperative_Question *>(entry_msg);
    
    total_served_clients++;
    
    Cooperative_Response 
      response(casted_input_msg->get_client_id(),
	       casted_input_msg->get_question_number(),
	       total_served_clients);
    
    Single_Response_From_All_RetMsg 
      ret_msg_to_daemon_group(sizeof(Cooperative_Response));
    
    //sleep(8);
    
    return_point->rpc_reply(&ret_msg_to_daemon_group, 
			    sizeof(Single_Response_From_All_RetMsg),
			    &response,
			    sizeof(Cooperative_Response));
    
    return 0;
  }

  int 
  cooperative_service_2 (Msg_Entry_Header * entry_msg)
  {
    MESSAGE(">>> COOPERATIVE SERVICE II <<<");
    
    Cooperative_Question * casted_input_msg = 
      static_cast<Cooperative_Question *>(entry_msg);
    
    total_served_clients++;
    
    Cooperative_Response 
      response(casted_input_msg->get_client_id(),
	       casted_input_msg->get_question_number(),
	       total_served_clients);
    
    Single_Response_From_Some_RetMsg 
      ret_msg_to_daemon_group(services_point.get_dispatched_message_id(),
			      sizeof(Cooperative_Response));
    
    //sleep(2);
    
    services_point.
      async_send_to_other_server(*ptr_broadcast_daemon_port,
				 &ret_msg_to_daemon_group, 
				 sizeof(Single_Response_From_Some_RetMsg),
				 &response,
				 sizeof(Cooperative_Response));
    
    return 0;
  }

public:

  Test_Client(const Port & _broadcast_port) :
    Broadcast_Client<Test_Client>(&services_point, _broadcast_port),
    services_point(NUMBER_OF_SRVCS, this),
    total_served_clients(0)
  {
    try
      {
	join_group(services_point.get_port());
      }
    catch (Duplicated)
      {
	MESSAGE("Intended duplication...");
      }
    
    services_thread = services_point.start_deamon();

    services_point.add_service(RECEIVE_FROM_BROADCAST_DAEMON,
			       "receive_from_broadcast_daemon", 
			       &Test_Client::receive_from_broadcast_daemon);
    
    services_point.add_service(COOPERATIVE_SERVICE,
			       "cooperative_service", 
			       &Test_Client::cooperative_service);

    services_point.add_service(COOPERATIVE_SERVICE_II,
			       "cooperative_service_ii", 
			       &Test_Client::cooperative_service_2);
  }

  void join_execution()
  {
    pthread_join(*services_thread, NULL);
  }

  ~Test_Client()
  {
    try
      {
	leave_group(services_point.get_port());
      }
    catch (NotFound)
      {
	MESSAGE("Not registered member");
      }
  }
};


int main(int argc, char * argv[])
{
  if (argc == 1)
    {
      MESSAGE("usage: test_broadcast_client <PORT_STRING> <# CLIENT_ID> <SEND | RECEIVE> <# SENDS>");
      exit(0);
    }

  static Port broadcast_daemon_port(argv[1]);
  
  ptr_broadcast_daemon_port = &broadcast_daemon_port;

  Test_Client test_client(broadcast_daemon_port);

  sleep(5);

  if (strcmp(argv[3], "SEND") == 0)
    for (int n=0; n < atoi(argv[4]); n++)
      {
	Cooperative_Question_2 request(atoi(argv[2]), n);
	
	//usleep(100000);

	test_client.strong_send_to_some(RECEIVE_FROM_BROADCAST_DAEMON,
					&request,
					sizeof(Cooperative_Question_2),
					1);
      }
  test_client.join_execution();
     
  return 0;
}


