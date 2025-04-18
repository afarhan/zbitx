#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include "sdr.h"
#include "sdr_ui.h"
#include "modem_ft8.h"
#include "ntputil.h"

#define MESSAGE_BLOCK_SIZE 13
/*
	1. Blocks: 
	1.1 Every message is split into one or more blocks. 
	1.2 The messages can only have a single case, numbers, stop, comma, 
	slash, plus and dash. (same as FT8).
	1.3 Maximum message size is limited to 150 characters
*/


#define NOTIFICATION_PERIOD 300 //5 minutes, 300 seconds

/** 
	struct message
	
	Each message has a maximum length of 256 bytes
	the conversation is in a linked list of messages
	they are all time-stamped since gmt midnight, 1st jan, 2000 in seconds

	flags field:
	bit 1: is out_going (if 1 and 0 if incoming)
	bit 2: completed (set if delivered and acknowledge)
	bit 3: begining time-slot of the message (of the remote tx for incoming, of ours for outoging)
	bit 4: to be deleted 
	the actual message is stored as segement in the message_buffer, pointed to by *data field

	The messages are allowed only a small subset of 40 ASCII characters:
	0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./? and SPACE.
	The data storage can be encoded even tighter to save space.
*/

#define MAX_TEXT 50000
#define MAX_MESSAGES 2000
#define MSG_INCOMING  0x00000001
#define MSG_COMPLETED 0x00000002
#define MSG_DELETE 		0x00000004
#define MSG_IMMEDIATE 0x00000008

struct message {
	uint32_t time_created;
	uint32_t time_updated;
	uint16_t flags;
	uint8_t length;
	char *data;
	struct message *next;
};
struct message message_block[MAX_MESSAGES];
char text_buffer[MAX_TEXT];
int message_free = 0;
int text_free = 0;
int next_update = 0;

#define MAX_CALLSIGN 10
#define CONTACT_FLAG_SAVED 1
#define CONTACT_FLAG_DELETE 2
struct contact {
	char callsign[MAX_CALLSIGN];
	char status[10];
	time_t last_update;
	uint32_t frequency; 
	uint32_t flags;
	struct message *m_list;
	struct contact *next; 
}; 

#define MAX_CONTACTS 200
struct contact contact_block[MAX_CONTACTS];
int contact_free = 0;
struct contact *contact_list = NULL;

char my_callsign[MAX_CALLSIGN];
char my_status[10];
uint32_t my_frequency = 7097000;

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

/* WIP
struct contact *contact_create(char *callsign, bool save, uint32_t last_seen){
	struct contact *new_contact = NULL;
	//search if we find an empty slot of in the contacts book
	for (int i = 0; i < MAX_CONTACTS; i++)
		if (contact_book
}

void contact_save(struct contact *pc, char *buff){
	fprintf("%s|%s|%u|%u|%c\n", 
		pc->callsign, 
		pc->status, 
		pc->last_update, 
		pc->frequency, 
		pc->saved_contact ? 'S' | 'N');
}
*/

struct message *add_chat(struct contact *pc, char *message, int flags){
	struct message *m = message_block + message_free;

	// time_created
	m->time_created = time_sbitx();
	m->time_updated = m->time_created;

	m->flags = flags;
	
	m->length = strlen(message);
	if (text_free + m->length >= MAX_TEXT)
		return NULL;
	m->data = text_buffer + text_free;
	m->next = NULL;

	strcpy(m->data, message);
	text_free += m->length;
	message_free++;

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
		
	message_free++;
}

struct message *message_load(char *buff){
	if (message_free >= MAX_MESSAGES)
		return NULL;

	while(*buff == ' ' || *buff == '\t')
		*buff++;
	char local[100];
	strcpy(local, buff);
	struct message *m = message_block + message_free;

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
	
	char *p = strtok(NULL, "|\n");
	m->length = strlen(p);
	if (text_free + m->length >= MAX_TEXT)
		return NULL;
	m->data = text_buffer + text_free;
	strcpy(m->data, p);
	text_free += m->length;

	message_free++;

	return m;
}

void msg_dump(){
	struct contact *pc;
	struct message *pm;

	for (pc = contact_list; pc; pc = pc->next){
		if (pc->last_update < time_sbitx() - 100)
			printf("%s %s\n", pc->callsign, pc->status);
		else
			printf("%s %s\n", pc->callsign, pc->status);
		for (pm = pc->m_list; pm; pm = pm->next)
			printf("   message: %d %.*s\n", 
				(int)(pm->length), (int)(pm->length), pm->data);	
	}
}

void update_contacts(){
	struct contact *pc;
	struct message *pm;
	char update_line[100];

	for (pc = contact_list; pc; pc = pc->next){
		if (pc->last_update < time_sbitx() - 100)
			sprintf(update_line, "#G%s #S%s\n", pc->callsign, pc->status);
		else
			sprintf(update_line, "%s %s\n", pc->callsign, pc->status);
		write_console(0, update_line);
	}
}

struct contact *contact_load(const char *string){
	if (contact_free >= MAX_CONTACTS)
		return NULL;
	struct contact *pc = contact_block + contact_free;

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

	contact_free++;
	return pc;
}

struct contact *contact_add(char *callsign, int frequency){
	if (contact_free >= MAX_CONTACTS)
			return NULL;
	struct contact *pc = contact_block + contact_free;
	strcpy(pc->callsign, callsign);
	pc->frequency = frequency;
	pc->flags = CONTACT_FLAG_SAVED;
	pc->next = contact_list; 
	pc->m_list = NULL;
	contact_list = pc;
	contact_free++;
	return pc;
}

void msg_init(){
	memset(contact_block, 0, sizeof(contact_block));
	memset(message_block, 0, sizeof(message_block));
	memset(text_buffer, 0, sizeof(text_buffer));
	
	message_free = 0;
 	text_free = 0;
	contact_free = 0;	

	msg_process(1000, "+VU2XZ RDY");
	msg_process(1200, "+VU2BVB AWY");
}

void save_messenger(char *filename){
	struct contact *pc;
	struct message *pm;
	FILE *pf = fopen(filename, "w");

	for (pc = contact_list; pc; pc = pc->next){
		fprintf(pf, "%s|%s|%d|%d|%d\n", pc->callsign, pc->status, 
			pc->last_update, pc->frequency, pc->flags);
		for (pm = pc->m_list; pm; pm = pm->next)
			fprintf(pf, " %d|%d|%d|%.*s\n", pm->time_created, pm->time_updated, pm->flags, 
					(int)(pm->length), pm->data);	
	}
	fclose(pf);
}


// Recreate the messenger from a file/presistent storage
void load_messenger(char *filename){
	char buff[100];
	struct contact *pc = NULL;
	struct message *pm = NULL;

	FILE *pf = fopen(filename, "r");
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
			printf("contact %d = ", contact_free);
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
				pm = NULL; //star a new chain of messages
			}
		}
	}
	fclose(pf);
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

struct contact *contact_by_callsign(char *callsign){
	for (struct contact *pc = contact_list; pc; pc = pc->next)
		if (!strcmp(pc->callsign, callsign))
			return pc;
	return NULL;
}

//matches a contact within +- 100 hz of the last frequency
struct contact *contact_by_frequency(int frequency){
	for (struct contact *pc = contact_list; pc; pc = pc->next)
		if (abs(pc->frequency - frequency) < 100)
			return pc;
	return NULL;
}

struct contact *contact_by_block(int freq, char *block){
	//we will just scan the block for the callsigns
	for (struct contact *pc = contact_list; pc; pc = pc->next)
		if (strstr(block, pc->callsign)) 
			return pc;
	return NULL;
}

void update_presence(int freq, char *notification){
	char call[10];
	int i;

	for (i = 0; i < MAX_CALLSIGN-1 && *notification > ' '; i++)
		call[i] = *notification++;
	call[i] = 0;
		
	notification++; //skip the space between the status and the contact
	struct contact *pc = contact_by_callsign(call); 
	if(!pc)
		pc = contact_add(call, freq);
	if (!pc) //out of memory?
		return;
	strcpy(pc->status, notification);
	pc->frequency = freq;
	pc->last_update= time_sbitx();
	contact_to_head(pc);
}

void msg_process(int freq, char *text){
	char call[10];
	struct contact *pc;

	/* if (!strncmp(text, "CQ ", 3)){
		update_presence(freq, text + 3);	
		msg_dump();
	}
	else */ 
	if (*text == '+'){
		update_presence(freq, text + 1);	
		msg_dump();
		update_contacts();
	}
	else if (pc = contact_by_block(freq, text)){
		add_chat(pc, text, 0);
	}
 
	//is it a call for us? 'W7PUA VU2ESE XD12' this is a message from als to bob (us)
	//here, the message's checksum is XD and it has 2 segments (between 1 and 8)
	
	//is it on a frequency we are listening for a message
	//is it a beacon we are interested in
	// 'W7PUA QRV CN84' 

	//is it an acknowlegment of our request to send?
	// 'VU2ESE W7PUA XA02' (the message id XA12 contains the checkum and the block count
}

void send_block(int freq, char *text){
	//TBD
	ft8_tx(text, freq);
}

void send_update(){
	if (!strcmp(my_status, "Silent"))
		return;
	char notification[20];
	char const *presence = field_str("PRESENCE");
	strcpy(notification, "+");
	strcat(notification, field_str("MYCALLSIGN"));
	strcat(notification, " ");
	if (!strcmp(presence, "READY"))
		strcat(notification, "QRV");
	else if (!strcmp(presence, "AWAY"))
		strcat(notification, "AWY");
	else if (!strcmp(presence, "SILENT"))
		strcat(notification, "QRT");
	else if (!strcmp(presence, "BUSY"))
		strcat(notification, "QRL");
	else
		strcat(notification, presence);

	send_block(field_int("TX_PITCH"), notification);
}

//this is called every 15 seconds
void msg_poll(){
	static int last_tick = 0;

	time_t now = time_sbitx();
	if (now == last_tick)
		return;
	
	last_tick = now;
	if (last_tick % 15)
		return;
	update_contacts();
	if (next_update < now){
		send_update();
		next_update = last_tick + 92;
		return;
	}
/*
	//are there any pending messages we need to retry/send?
	if (nticks % 15 == 0){
		for (struct contacts *pc = contact_list; pc; pc = pc->next)
			if (!pc->m_list && (pc->m_list & MSG_IMMEDIATE){
				send_message(pc->m_list);
				break;
			}
			//we may want to send out other messages too, it could cook the PA
	}
*/
}



/// test fixtures

/*
int main(int argc, char **argv){
	fd_set fds;
  struct timeval ts;
	char input[100];
	
  ts.tv_sec = 0; // 0 second
  ts.tv_usec = 1000; //1 millisecond
	input[0] = 0;

	msg_init("VU2ESE");
	strcpy(my_status, "QRV");
	load_messenger("messenger.txt");
	save_messenger("messenger_out.txt");

	while(1){
    FD_ZERO(&fds);
    FD_SET(0, &fds);
 
    // wait for data
    int nready = select(sock + 1, &fds, (fd_set *) 0, (fd_set *) 0, &ts);
		if(FD_ISSET(0, &fds)) {
    	char c = getc(stdin); //fgets(buf, 1, stdin);
			if (c == '\n'){
				process_user_input(input);
				input[0] = 0;
			}
			else{
				int l = strlen(input);
				if (l >= sizeof(input)
					continue;
				input[l++] = c;
				input[l] = 0;
			}
		
	}
	return 0;
}
*/

