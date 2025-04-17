#define FT8_MAX_BUFF (12000 * 18) 
void ft8_rx(int32_t *samples, int count);
void ft8_init();
void ft8_abort();
void ft8_tx(char *message, int freq);
void ft8_poll(int seconds, int tx_is_on);
float ft8_next_sample();
void ft8_process(char *message, int operation);
void ft8_set_protocol(int protocol); //added for messenger operation MODE_FT8 or MODE_MSG
