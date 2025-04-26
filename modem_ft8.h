#define FT8_MAX_BUFF (12000 * 18) 
void ft8_rx(int32_t *samples, int count);
void ft8_init();
void ft8_abort();
void ft8_tx(char *message, int freq);
void ft8_poll(int seconds, int tx_is_on);
float ft8_next_sample();
void ft8_process(char *message, int operation);
void ft8_set_protocol(int protocol); //added for messenger operation MODE_FT8 or MODE_MSG

void msg_init();
void msg_process(int freq, const char *text);
void msg_save(char *filename);
void msg_load(char *filename);
void msg_poll();
void msg_select(char *msg);
int msg_post(const char *callsign, const char *message);
void msg_add_contact(const char *callsign);
void msg_remove_contact(const char *callsign);

void chat_ui_init();
void clear_contact_list();
void add_item_to_contact_list(const char *c);
void chat_clear();
void chat_title(const char *title);
void chat_append(const char *text);
