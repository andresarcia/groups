# include "broadcast_pars.H"
# include <argp.h>
# include <stdlib.h>
# include "aleph.H"
# include "ahDaemonize.H"

const int Default_Response_Buffer_Size = 4096;

const int Default_Clients_Table_Size = 101;

const int Default_Conversion_Table_Size = 101;

const int Default_Msec_Timeout_For_Clients = 50000;

int response_buffer_size;

int clients_table_size;

int conversion_table_size;

int msec_timeout_for_clients;


static const char hello_preamble [] =
"\n"
"  ALEPH BROADCAST daemon\n"
"  Copyright (c) 2000, 2001\n"
"      University of Los Andes (ULA) Merida - VENEZUELA\n"
" - Center of Studies in Microelectronics & Distributed Systems (CEMISID)\n"
" - National Center of Scientific Computing of ULA (CECALCULA)\n"
"  All rights reserved."
"\n"
"\n"
;


static const char license_text [] =
"\n"
"Aleph Broadcast Daemon License & Copyright Note\n"
"\n"
"  Copyright (c) 2000, 2001\n"
"  UNIVERSIDAD DE LOS ANDES (ULA) Merida - VENEZUELA\n"
"  All rights reserved.\n\n"

"  - Center of Studies in Microelectronics & Distributed Systems (CEMISID)\n"
"  - National Center of Scientific Computing (CECALCULA)\n\n"

"  PERMISSION TO USE, COPY, MODIFY AND DISTRIBUTE THIS SOFTWARE AND ITS \n"
"  DOCUMENTATION IS HEREBY GRANTED, PROVIDED THAT BOTH THE COPYRIGHT \n"
"  NOTICE AND THIS PERMISSION NOTICE APPEAR IN ALL COPIES OF THE \n"
"  SOFTWARE, DERIVATIVE WORKS OR MODIFIED VERSIONS, AND ANY PORTIONS \n"
"  THEREOF, AND THAT BOTH NOTICES APPEAR IN SUPPORTING DOCUMENTATION. \n"
"\n"
"  Aleph IPC system is distributed in the hope that it will be useful,\n"
"  but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. \n"
"\n"
"  UNIVERSIDAD DE LOS ANDES requests users of this software to return to \n"
"\n"
"      CEMISID - Software \n"
"      Ed La Hechicera \n"
"      3er piso, ala Este \n"
"      Facultad de Ingenieria \n"
"      Universidad de Los Andes \n"
"      Merida 5101 - VENEZUELA \n"
"\n"
"  or to 	software@cemisid.ing.ula.ve \n"
"\n"
"  any improvements or extensions that they make and grant Universidad \n"
"  de Los Andes (ULA) the full rights to redistribute these changes. \n"
"\n"
"Aleph IPC system is (or was) granted by: \n"
"- Consejo de Desarrollo Cientifico, Humanistico, Tecnico de la ULA  (CDCHT)\n"
"- Consejo Nacional de Investigaciones Cientificas y Tecnologicas (CONICIT)\n"
"\n"
"\n";

static char doc[] = "    Options are as follow:";

static char arg_doc[] = "";

struct Parameters
{
  int response_buffer_size;
  
  int clients_table_size;
  
  int conversion_table_size;
  
  int msec_timeout_for_clients;
  
  bool null_output;

  bool daemonize;

  Parameters() :
    response_buffer_size(Default_Response_Buffer_Size),
    clients_table_size(Default_Clients_Table_Size),
    conversion_table_size(Default_Conversion_Table_Size),
    msec_timeout_for_clients(Default_Msec_Timeout_For_Clients),
    null_output(false),
    daemonize(false)
  {
    // empty
  }
    
};



static struct argp_option options [] = 
{
  {"buffer-size", 'b', 0, OPTION_ARG_OPTIONAL, "Set response buffer size", 0},
  {"clients-table", 'c', 0, OPTION_ARG_OPTIONAL, "Set clients table size", 0},
  {"conv-table", 'o', 0, OPTION_ARG_OPTIONAL, "Set conversion table size", 0},
  {"timeout", 't', 0, OPTION_ARG_OPTIONAL, "Set timeout for client's response in miliseconds", 0},
  {"daemonize", 'D', 0, OPTION_ARG_OPTIONAL, "Load locator as a daemon", 0},
  {"null-desc", 'u', 0, OPTION_ARG_OPTIONAL, "Load locator with stdin, stdout and stderr as null file descriptors", 0},
  
  { 0 }
};

void process_size(int & size_var, 
		  const char * size_name, 
		  const char opt_char, 
		  struct argp_state *state)
{
  if (state->argv[state->next] == NULL)
    EXIT("missing %s size in -%c option\n", size_name, opt_char);
  
  char *end;
  
  size_var = strtol(state->argv[state->next], &end, 10);
  
  if (*end != '\0' && *end != '\n')
    EXIT("Bad format in -%c %s option\n", opt_char, state->argv[state->next]);
  if (size_var < 0)
    EXIT("%s size can't be negative value.\n", size_name);
  state->next++;
}


static error_t parser_opt(int key, char *arg, struct argp_state *state)
{
  Parameters *pars_ptr = static_cast<Parameters*>(state->input);

  switch (key)
    {
    case ARGP_KEY_END:
      break;
    case 'b':
      {
	process_size(pars_ptr->response_buffer_size, 
		     "buffer-size", 'b', state);
	break;
      }
    case 'c':
      {
	process_size(pars_ptr->clients_table_size, 
		     "clients-table-size", 'c', state);
	break;
      }
    case 'o':
      {
	process_size(pars_ptr->conversion_table_size, 
		     "conversion-table-size", 'o', state);
	break;
      }
    case 't':
      {
	process_size(pars_ptr->msec_timeout_for_clients, 
		     "timeout", 't', state);
	break;
      }
    case 'u':
      {
	pars_ptr->null_output = true;
	break;
      }
    case 'D':
      {
	pars_ptr->daemonize = true;
	pars_ptr->null_output = true;
	break;
      }
    case ARGP_KEY_ARG:
    default: return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp arg_defs = { options, parser_opt, arg_doc, doc };

void parameters_decode(int argc, char * argv[])
{
  Parameters pars;
  
  error_t status = argp_parse(&arg_defs, argc, argv, 0, 0, &pars);
  
  if (status != 0)
    EXIT("Internal error in parsing of commands line: %s", strerror(errno));


  response_buffer_size = pars.response_buffer_size;
  
  clients_table_size = pars.clients_table_size;
  
  conversion_table_size = pars.conversion_table_size;
  
  msec_timeout_for_clients = pars.msec_timeout_for_clients;
  
  if (pars.daemonize)
    daemonize(argv[0], LOG_USER);

  if (pars.null_output)
    {
      stdout = stderr = stdin = fopen("/dev/null", "r+");
      if (stdout == NULL)
	EXIT("%s\n", strerror(errno));
    }
}

