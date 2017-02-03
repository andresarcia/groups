# ifndef BROADCAST_PARS
# define BROADCAST_PARS

extern const int Default_Response_Buffer_Size;

extern const int Default_Clients_Table_Size;

extern const int Default_Conversion_Table_Size;

extern const int Default_Timeout_For_Clients;

extern int response_buffer_size;

extern int clients_table_size;

extern int conversion_table_size;

extern int msec_timeout_for_clients;

extern void parameters_decode(int /*argc*/, char * argv[]);
# endif // LOCATOR_PARS 

