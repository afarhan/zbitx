#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <ctype.h>
#include <time.h>
#include "sdr.h"
#include "sdr_ui.h"
#include "modem_ft8.h"
#include "ntputil.h"
#include "md5.h"

#define PACKET_SIZE 13
#define MAX_MESSAGE 150
#define MSG_RETRY_SECONDS  480 
#define NOTIFICATION_REPEAT 300 
/*
	1. Blocks: 
	1.1 Every message is split into one or more blocks. 
	1.2 The messages can only have a single case, numbers, stop, comma, 
	slash, plus and dash. (same as FT8).
	1.3 Maximum message size is limited to 50 characters

	msg_process() is called with every raw packet decoded.
	if it detects a new message header, it initializes the
	corresponding contact to start accumulating all the packets.
	At the end of the receiving all the packets and checking the packet integrity


*/

#define NOTIFICATION_PERIOD 300 //5 minutes, 300 seconds

#define MSG_INCOMING  		0x00000001
#define MSG_ACKNOWLEDGE  0x00000002

#define MAX_MSG_LENGTH (52)
struct message {
	uint32_t time_created;
	uint32_t time_updated;
	uint16_t flags;
	int16_t nsent;
	uint8_t length;
	uint8_t state;
	struct message *next;
	char data[1];
};


#define MAX_CALLSIGN 20 
#define CONTACT_FLAG_SAVED 1
#define CONTACT_FLAG_DELETE 2
struct contact {
	char callsign[MAX_CALLSIGN];
	char status[MAX_CALLSIGN];
	time_t last_update;
	uint32_t frequency; 
	uint32_t flags;
	char msg_buff[MAX_MSG_LENGTH+1];
	time_t msg_timeout;
	struct message *m_list;
	struct contact *next; 
}; 

static struct contact *contact_list = NULL;
static unsigned int next_update = 0;
static unsigned int refresh_chat = 0;
static unsigned int refresh_contacts = 0;
static unsigned int next_save  = 0;
static unsigned int pause_until = 0;
static int save_messages = 0;

static char my_callsign[MAX_CALLSIGN];
static char my_presence[10];
uint32_t my_frequency = 7097000;
char selected_contact[MAX_CALLSIGN];
//void update_chat();
void update_contacts();

struct contact *contact_by_callsign(const char *callsign);

/**
	Contacts/Users:
	* Each user is identiied by their callsign of upto 10 characters
	* You may filter out messages to only those who are on you contact list
	* The contacts are updated with the last heard time.
			Ex: a contact is transmitting on 7.074 MHz with a pitch of 1750 Hz,
			the frequency of transmission is taken as 70741750 Hz.
	* Contacts will periodically transmit their status every 5 minutes.
	* Contacts marked as saved are stored on persistent media.
*/

#define MAX_CONTACTS 200

static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./? ";

void reduce_text(const char *message, char *output){
	int i = 0;
	for (i = 0; i < strlen(message); i++){
		if (isalnum(message[i]) || message[i] == '+' || 
		message[i] == '-' || message[i] == '.' ||
		message[i] == '/' || message[i] == '?' ||
		message[i] == ' ')
			output[i] = toupper(message[i]);
		else
			output[i] = '?';
	}
	output[i] = 0;  
	//pad the text to fill the last packet
	while (i % PACKET_SIZE){
		strcat(output, " ");	
		i++;
	}
}

void make_header(const char *dest, const char *src, 
	const char *message, char *output){
	char msg_local[MAX_MESSAGE];
	int i = 0;
  MD5Context ctx;

  md5Init(&ctx);

	reduce_text(message, msg_local);

  md5Update(&ctx, (uint8_t *)src, strlen(src));
  md5Update(&ctx, (uint8_t *)dest, strlen(dest));
  md5Update(&ctx, (uint8_t *)message, strlen(message));
  md5Finalize(&ctx);

	char check[3];
	check[0] = 'A' + (ctx.digest[0] & 0xf);
	check[1] = 'A' + (ctx.digest[1] & 0xf);
	check[2] = 0; 

	int packet_count = (strlen(message) + PACKET_SIZE- 1)/PACKET_SIZE;
	sprintf(output, "%s %s %s%d0", dest, src, check, packet_count); 	
}

void send_packet(int freq, char *text){
	//printf("Sending %s\n", text);
	printf("Sending(%u) on %dhz, [%s]\n", time_sbitx(), freq, text);
	fflush(stdout);
	ft8_tx(text, freq);
}

void send_update(){
	
	if (!strcmp(my_presence, "SILENT"))
		return;
	char notification[20];
	strcpy(notification, "+");
	strcat(notification, field_str("MYCALLSIGN"));
	strcat(notification, " ");
	strcat(notification, my_presence);
	send_packet(field_int("TX_PITCH"), notification);
	next_update = time_sbitx() + NOTIFICATION_REPEAT + ((rand() % 2) * 15);
	printf("Next in %d seconds\n", next_update - time_sbitx());
}


struct message *message_load(char *buff){
	while(*buff == ' ' || *buff == '\t')
		*buff++;
	char local[100];
	strcpy(local, buff);
	struct message *m = (struct message *)malloc(sizeof(struct message) + strlen(local));


	// time_created
	uint32_t x = strtoul(strtok(buff, "|"), NULL, 10);
	if (x == 0)
		return NULL;
	m->time_created = x;

	x = strtoul(strtok(NULL, "|"), NULL, 10);
	if (x == 0)
		return NULL;
	m->time_updated = x;

	x = strtoul(strtok(NULL, "|"), NULL, 10);
	m->flags = x;

	x = strtoul(strtok(NULL, "|"), NULL, 10);
	m->nsent = x;
	
	char *p = strtok(NULL, "|\n");
	m->length = strlen(p);
	strcpy(m->data, p);
	m->next = NULL;

	return m;
}

void msg_dump(){
	struct contact *pc;
	struct message *pm;
	
	return;

	printf("msg dump **********\n");
	for (pc = contact_list; pc; pc = pc->next){
		if (pc->last_update < time_sbitx() - 100)
			printf("%s %s (%d)\n", pc->callsign, pc->status, pc->frequency);
		else
			printf("%s %s (%d)\n", pc->callsign, pc->status, pc->frequency);
		for (pm = pc->m_list; pm; pm = pm->next)
			printf("   message: %d/%d flag:%x  %.*s\n", 
				pm->nsent, (int)(pm->length), pm->flags, (int)(pm->length), pm->data);	
	}
}

void update_chat(){
	char message[1000];

	chat_clear();					
	chat_title("(Select Contact)");

	const char *contact = field_str("CONTACT");
	if (!contact){
		return;
	}
	if (!strcmp(contact, "LIST"))
		return;
			
	struct contact *pc = contact_by_callsign(contact);
	if (!pc)
		return;

	chat_title(contact);
	for (struct message *pm = pc->m_list; pm; pm = pm->next){
		time_t msg_time = pm->time_created;
		struct tm *t = gmtime(&msg_time);
		if(!(pm->flags & MSG_INCOMING)){
			if (pm->nsent == -1){
				sprintf(message, "%s (%d/%d/%d %02d:%02d) New \n%.*s\n", 
					field_str("MYCALLSIGN"), t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, 
					t->tm_hour, t->tm_min, (int)(pm->length), pm->data);	
			}
			else{
				if (pm->flags & MSG_ACKNOWLEDGE)
					sprintf(message, "%s (%d/%d/%d %02d:%02d) Ack\n%.*s\n", 
						field_str("MYCALLSIGN"), t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, 
						t->tm_hour, t->tm_min, (int)(pm->length), pm->data);	
				else 
					sprintf(message, "%s (%d/%d/%d %02d:%02d) %d/%d\n%.*s\n", 
						field_str("MYCALLSIGN"), t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, 
						t->tm_hour, t->tm_min, pm->nsent, pm->length, 
						(int)(pm->length), pm->data);	
			}

		}
		else {
			sprintf(message, "%s (%d/%d/%d %02d:%02d)\n%.*s\n", 
				pc->callsign, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, 
				t->tm_hour, t->tm_min, (int)(pm->length), pm->data);	
		}
		chat_append(message);
	}
	refresh_chat = 0;
}

void update_contacts(){
	struct contact *pc;
	struct message *pm;
	char update_line[100];

	//send a form feed to clear the console
	update_line[0] = 12;
	update_line[1] = 0;
	//write_console(0, update_line);
	clear_contact_list();

	for (pc = contact_list; pc; pc = pc->next){
		if (pc->last_update > time_sbitx() - 600){
			sprintf(update_line, "#G*%s #S%s\n", pc->callsign, pc->status);
			//write_console(0, update_line);
			sprintf(update_line, "%s - %s", pc->callsign, pc->status);
			add_item_to_contact_list(update_line);
		}
		else{
			sprintf(update_line, "*%s %s\n", pc->callsign, pc->status);
			//write_console(0, update_line);
			sprintf(update_line, "%s - Inactive", pc->callsign);
			add_item_to_contact_list(update_line);
		}
	}
	refresh_contacts = 0;
}

struct contact *contact_load(const char *string){
	struct contact *pc = (struct contact *)malloc(sizeof(struct contact));
	if (!pc)
		return NULL;

	memset(pc, 0, sizeof(struct contact));

	char buff[100];
	strcpy(buff, string);

	//callsign
	char *p = strtok(buff, "|");
	if (strlen(p) < 3 || MAX_CALLSIGN - 1  < strlen(p))
		return NULL;
	strcpy(pc->callsign, p);
	
	//status
	p = strtok(NULL, "|");
	if (strlen(p) > 9)
		return NULL;
	strcpy(pc->status, p);

	//last heard
	uint32_t x = strtoul(strtok(NULL, "|"), NULL, 10);
	if (x == 0)
		return NULL;
	pc->last_update= x;

	//frequency
	x = strtoul(strtok(NULL, "|"), NULL, 10);
	if (x == 0)
		return NULL;
	pc->frequency = x;

	//flags, this can be zero
	x = strtoul(strtok(NULL, "|"), NULL, 10);
	//skip deleted flags
	if (x & CONTACT_FLAG_DELETE)
		return NULL;
	pc->flags= x;
	pc->next = NULL;
	pc->m_list = NULL;

	return pc;
}

//moves the contact to the head of the list
void contact_to_head(struct contact *pc){
	struct contact *prev = NULL;
	for (struct contact *p = contact_list; p; p = p->next)
		if (p->next == pc){
			p->next = pc->next; //remove from the list
			pc->next = contact_list; //insert at the head
			contact_list = pc;
		}
}

struct contact *contact_add(const char *callsign, int frequency){
	struct contact *pc = (struct contact *)malloc(sizeof(struct contact));
	if (!pc)
		return NULL;
	memset(pc, 0, sizeof(struct contact));

	strcpy(pc->callsign, callsign);
	pc->frequency = frequency;
	pc->flags = CONTACT_FLAG_SAVED;
	pc->status[0] = 0;
	pc->last_update = 0;
	pc->next = contact_list; 
	pc->m_list = NULL;
	contact_list = pc;

	save_messages++;
	return pc;
}

struct message *add_chat(struct contact *pc, const char *message, int flags){

	struct message *m = (struct message *)malloc(sizeof(struct message) + strlen(message));

	if (!m)
		return NULL;

	// time_created
	m->time_created = time_sbitx();
	m->time_updated = m->time_created;

	m->flags = flags;
	
	m->length = strlen(message);
	m->next = NULL;
	m->nsent = -1;

	reduce_text(message, m->data);

	//append to the end of the list
	struct message *prev = NULL;
	struct message *now = pc->m_list;
	while (now){
		prev = now;
		now = now->next;
	}	
	if (prev)
		prev->next = m;
	else
		pc->m_list = m;	
	
	contact_to_head(pc);
	
	refresh_chat = 1;
	return m;
	save_messages++;
}

int packet_count(int length){
	int count = 0;
	//the last block has 3 character long checksum
	while(length > PACKET_SIZE -3){
		//each of non-last blocks has a '+' in the end
		length -= (PACKET_SIZE - 1);
		count++;
	}
	return count;
}

void msg_init(){

	strcpy(my_presence, "READY");
	selected_contact[0] = 0;
	next_save  = time_sbitx() + 300;
	next_update= time_sbitx() + 15; //next available slot	
	chat_ui_init();

	msg_load("/home/pi/sbitx/data/messenger.txt");
	update_contacts();
}

void msg_save(char *filename){
	struct contact *pc;
	struct message *pm;
	FILE *pf = fopen(filename, "w");

	printf("Saving the messages\n");
	if(!pf)
		return;

	for (pc = contact_list; pc; pc = pc->next){
		fprintf(pf, "%s|%s|%d|%d|%d\n", pc->callsign, pc->status, 
			pc->last_update, pc->frequency, pc->flags);
			for (pm = pc->m_list; pm; pm = pm->next)
				fprintf(pf, " %d|%d|%d|%d|%.*s\n", pm->time_created, pm->time_updated, pm->flags, 
					pm->nsent, (int)(pm->length), pm->data);	
	}
	fclose(pf);
	save_messages = 0;
	next_save = time_sbitx() + 300;
}


// Recreate the messenger from a file/presistent storage
void msg_load(char *filename){
	char buff[100];
	struct contact *pc = NULL;
	struct message *pm = NULL;

	FILE *pf = fopen(filename, "r");
	if (!pf)
		return;

	while(fgets(buff, sizeof(buff)-1, pf)){
		if (strlen(buff) < 10)
			continue;
		//if it a message in  a valid contact's conversation?
		if ((buff[0] == ' ' || buff[0] == '\t') && pc){
			struct message *m_new = message_load(buff);
			if (pc && m_new){
				if (pm)
					pm->next = m_new;
				else
					pc->m_list = m_new;
				pm = m_new;
			}		
		}
		else {
			//add the next contact to the tail
			struct contact *prev = pc;
			if (!prev)
				prev = contact_list;
			pc = contact_load(buff);
			if (pc){
				printf("call:%s, status:%s,last:%u,freq:%u,flag:%u\n",
					pc->callsign, pc->status, pc->last_update, pc->frequency, pc->flags);
				if (prev)
					prev->next = pc;
				else
					contact_list = pc;
				prev = pc;
				pm = NULL; //start a new chain of messages
			}
		}
	}
	fclose(pf);
}


struct contact *contact_by_callsign(const char *callsign){
	for (struct contact *pc = contact_list; pc; pc = pc->next)
		if (!strcmp(pc->callsign, callsign))
			return pc;
	return NULL;
}

//matches a contact within +- 100 hz of the last frequency
struct contact *contact_by_frequency(int frequency){
	for (struct contact *pc = contact_list; pc; pc = pc->next)
		if (abs(pc->frequency - frequency) < 20)
			return pc;
	return NULL;
}

struct contact *contact_by_freq(int freq, const char *block){
	//we will just scan the block for the callsigns
	/*
	for (struct contact *pc = contact_list; pc; pc = pc->next)
		if (strstr(block, pc->callsign)) 
			return pc;
	*/

	//after this we try matching the frequency
	for (struct contact *pc = contact_list; pc; pc = pc->next)
		if (abs(freq - pc->frequency) < 20){ 
			printf("   matched %s with %d\n", pc->callsign, pc->frequency);
			return pc;
		}

	return NULL;
}

void update_presence(int freq, const char *notification){
	char call[10];
	int i;

	for (i = 0; i < MAX_CALLSIGN-1 
		&& (isalnum(*notification) || *notification == '/'); i++)
		call[i] = *notification++;
	call[i] = 0;
		
	notification++; //skip the space between the status and the contact

	struct contact *pc = contact_by_callsign(call); 
	if(!pc)
		pc = contact_add(call, freq);
	if (!pc) //out of memory?
		return;

	strncpy(pc->status, notification, MAX_CALLSIGN-1);
	pc->status[MAX_CALLSIGN-1] = 0;
	pc->frequency = freq;
	pc->last_update= time_sbitx(); //this is being done one slot late
	contact_to_head(pc);
	refresh_contacts++;
}

void msg_select(char *callsign){
	strcpy(selected_contact, callsign);
	field_set("CONTACT", callsign);
	
	struct contact *pc = contact_by_callsign(selected_contact);
	if (!pc)
		return;
	update_chat();
	
}

int msg_post(const char *contact, const char *message){
	if (!contact && selected_contact[0] == 0){
		printf("No contact selected\n");	
		return -1;
	}

	if (!contact)
		contact= selected_contact;

	struct contact *pc = contact_by_callsign(contact);	
	if(!pc)
		pc = contact_add(contact, 0); //we havent heard from them yet
	if (!pc) //out of memory?
		return -1;

	struct message *pm = add_chat(pc, message, 0);
	pm->nsent = -1;
	update_chat();
	refresh_chat++;
	save_messages++;
	return 0;
}

void msg_process(int freq, const char *text){
	char msg_local[20];
	struct contact *pc;
	const char *mycall = field_str("MYCALLSIGN");
	time_t now = time_sbitx();

	strcpy(msg_local, text);


	//this may happen at the start, before
	//the user_settings.ini is loaded
	if (!mycall)
		return;

	printf("%u msg_process: %d [%s]\n", now % 600, freq, text);
	if (!strncmp(text, "CQ ", 3)){
		update_presence(freq, text + 3);	
	}
	else if (*text == '+'){
		update_presence(freq, text + 1);	
	}
	else{

		//test if this is the header of a new message
		strncpy(msg_local, text, sizeof(msg_local) - 1);
		msg_local[sizeof(msg_local) - 1] = 0;	
	
		char *me = strtok(msg_local, " ");
		char *contact = strtok(NULL, " ");
		char *checksum = strtok(NULL, " ");

		//does this look like an acknowledgment?
		//the firt callsign is of the contact and second if of the sender(us)
		if (me && contact && checksum && !strcmp(contact, mycall) 
			&& strlen(checksum) == 4 && strlen(contact) <= 8
			&& checksum[3] == '0'){
			printf("Received an acknowledgment[%s]\n", text);
			pc = contact_by_callsign(me);
			if (pc){
				for (struct message *m = pc->m_list; m; m = m->next){
					char header[100];
					make_header(pc->callsign, contact, m->data, header);
					//the header matches an outgoing message
					//check only for recent messages
					if (!strcmp(header, text) && !(m->flags & MSG_INCOMING)
						&& m->time_updated + MSG_RETRY_SECONDS < now){
						printf("header matched with outgoing message [%s]\n", m->data);
						m->flags = m->flags + MSG_ACKNOWLEDGE;
						save_messages++;
						return;
					}
					else
						printf("header %s didn't match %s\n", header, text);
				}//end of if(pc)
			}
		}

		//does this look like a header?
		if (me && contact && checksum && !strcmp(me, mycall) 
			&& strlen(checksum) == 4 && strlen(contact) <= 8
			&& checksum[3] == '0'){
		
			printf("starting a new message accumulation from %s\n", contact);	
			pc = contact_by_callsign(contact); 
			if(!pc)
				pc = contact_add(contact, freq);

			//we hope we are not in the middle of
			//receiving another message
			if (pc->msg_timeout < now){
				int nslots = checksum[2] - '0';
				printf("got %d slots\n", nslots);
				pc->msg_timeout = now + (15 * nslots);
				printf("msg_timeout %u vs now %u\n", pc->msg_timeout, now);
				strcpy(pc->msg_buff, text);
				pause_until = now + (15 *(nslots + 4));
			}
		}
		else if (pc = contact_by_freq(freq, text)){
		
			printf("msg_timeout %u vs now %u\n", pc->msg_timeout, now); 	
			if (pc->msg_timeout >= now){
				strcat(pc->msg_buff, text);
				printf("appended to partial message [%s]\n", pc->msg_buff);
			}
			if (pc->msg_timeout <= now && pc->msg_buff[0]){
				
				printf("message finished as [%s], acknowledging\n", pc->msg_buff);
				//checksum the message
				//send the acknowledgment, the same header
				char *p = pc->msg_buff;
				while(*p > ' ')
					p++;
				p++;
				while(*p > ' ')
					p++;
				//skip the space followed by the header of 4
				p+= 5;

				char ack[100];
				make_header(mycall, pc->callsign, p, ack);	
				if (!strncmp(ack, pc->msg_buff, strlen(ack))){
					printf("sending acknowledgement of [%s]\n", p);
					struct message *m = add_chat(pc, p, MSG_INCOMING);
					m->flags = m->flags + MSG_ACKNOWLEDGE;
					pc->msg_buff[0] = 0;
					pc->msg_timeout = 0;
					//release the buffer
				}
				save_messages++;
				//even if the message didnt add up, we reset the msg buff
				//even if the message didnt add up, we reset the msg buff
			}
		}
	}
	refresh_chat++;
	msg_dump();
}

static struct message *m_in_tx = NULL;

void on_slot(){
	time_t now = time_sbitx();

	//check if we are in teh middle of receiving any message
	int active = 0;
	struct contact *pc;
	//give two slots time out
	for (pc = contact_list; pc; pc = pc->next){
		if (pc->msg_timeout + 30 >= now)
			active++;
	}

	if (active)
		return;

	printf("on_slot paused until %d\n", pause_until);
	struct message *pm;
	for (pc = contact_list; pc; pc = pc->next){
		//if (now - pc->last_update < 600)
		if (1){
			for (pm = pc->m_list; pm; pm = pm->next){
		//if it is an incoming message and ACK is due
				if(pm->flags & MSG_INCOMING){
					if (pm->flags & MSG_ACKNOWLEDGE){
						char packet[100];
						make_header(field_str("MYCALLSIGN"), pc->callsign, pm->data, packet);
						send_packet(field_int("TX_PITCH"), packet);
						printf(">>>>>>>>>>>>>>>>>>>sending acknowledgment of %s\n", packet);
						pm->flags = MSG_INCOMING; //blank out the acknowledgement due
					}
				}
				else if(now - pc->last_update < NOTIFICATION_REPEAT *2){
					char packet[20];			
		//for all outgoing packets
				
					if(pm->nsent < pm->length){
						//send out the header, starting the tx process only if we had recently seen the contact
						//don't start a new message when tx has been paused
						if (pm->nsent == -1 && now - pc->last_update < (NOTIFICATION_REPEAT * 2)
							&& pause_until < now){
							make_header(pc->callsign, field_str("MYCALLSIGN"), pm->data, packet);
							pm->nsent = 0;	
							//pause starting other messages or sending an update
							//until this is fully transmitted
							pause_until = now + ((strlen(pm->data)/PACKET_SIZE)+3) * 15;
						}
						else {
							int nsize = pm->length - pm->nsent;
							if (nsize > PACKET_SIZE)
								nsize = PACKET_SIZE;	
							strncpy(packet, pm->data + pm->nsent, nsize);
							packet[nsize] = 0;
							pm->nsent += nsize; 	
						}
						pm->time_updated = now;
						send_packet(field_int("TX_PITCH"), packet);
						return; // don't try sending any more
					} //end of transmit message attempt
					else if (pm->nsent >= pm->length && (pm->flags & MSG_ACKNOWLEDGE) == 0
						&& pm->time_updated + MSG_RETRY_SECONDS < now){			
							pm->nsent = -1;	
					}
				}

			} // next message of the contact 
		}
	} // next contact

	//if nothing else was sent, send the notification
	if (next_update < now && pause_until < now){
		printf("sending update %u vs %u\n", next_update, now);
		send_update();
	}
}

//this is called every second
void msg_poll(){
	static int last_tick = 0;

	time_t now = time_sbitx();
	if (now == last_tick)
		return;

	if (refresh_chat)
		update_chat();

	if (refresh_contacts)
		update_contacts();

	last_tick = now;

	if (next_save < now  || save_messages > 0)
		msg_save("/home/pi/sbitx/data/messenger.txt");

	//from here, we do stuff on every 15th second
	//msg_dump();

	if (now % 15)
		return;

	on_slot();
}

void msg_add_contact(const char *callsign){
	char newcall[12];

	if (strlen(callsign) > 10){
		printf("messenger.c:callsign %s is too long\n", callsign);
		return;
	}
	char *q = newcall;
	for (const char *p = callsign; *p; p++)
		*q++ = toupper(*p);
	*q = 0;

	struct contact *pc = contact_add(newcall, 0);
	update_contacts();
	refresh_contacts++;
}

void msg_remove_contact(const char *callsign){
	struct contact *pc, *prev = NULL;	
	for (pc = contact_list; pc; pc = pc->next){
		if (!strcmp(callsign, pc->callsign)){
			if (prev)
				prev->next = pc->next;
			else
				contact_list = pc->next;
			//free the message list
			struct message *pm = pc->m_list; 
			while(pm){
				struct message *next = pm->next;
				free(pm);
				pm = next;	
			}
		
			if (!strcmp(selected_contact, callsign))
				selected_contact[0] = 0;	
			free(pc);
			refresh_contacts++;
			refresh_chat++;
			return;
		}
		prev = pc;
	}
}

void msg_presence(const char *new_presence){
	strcpy(my_presence, new_presence);
	next_update = time_sbitx();
}
