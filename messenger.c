#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <ctype.h>
#include "sdr.h"
#include "sdr_ui.h"
#include "modem_ft8.h"
#include "ntputil.h"
#include "md5.h"

#define BLOCK_SIZE 13
/*
	1. Blocks: 
	1.1 Every message is split into one or more blocks. 
	1.2 The messages can only have a single case, numbers, stop, comma, 
	slash, plus and dash. (same as FT8).
	1.3 Maximum message size is limited to 150 characters

	1234567890123
	fb,gotmail?AA
	hi,alwl i hp+
	 gng2del2mrAA 

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
	bit 3: begining time-slot of the message 
		(of the remote tx for incoming, of ours for outoging)
	bit 4: marked for deletion 
	the actual message is stored as segement in the message_buffer, pointed to by *data field

	The messages are allowed only a small subset of 40 ASCII characters:
	0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./? and SPACE.
	The data storage can be encoded even tighter to save space.

	the sent message is split into multiple blocks
	of total physical length of 13 characters each.

*/

#define MAX_TEXT 100000
#define MAX_MESSAGES 10000
#define MSG_INCOMING  0x00000001
#define MSG_COMPLETED 0x00000002
#define MSG_DELETE 		0x00000004
#define MSG_IMMEDIATE 0x00000008

struct message {
	uint32_t time_created;
	uint32_t time_updated;
	uint16_t flags;
	uint8_t length;
	uint8_t nsent;
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
	char status[MAX_CALLSIGN];
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
char selected_contact[MAX_CALLSIGN];

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

/*
int checksum(struct message *pm){
	int check = 0;

	//the checkum is on mycallsign + contact + timestamp + key + message

	while(*text){
		for (int i = 0; i < sizeof(charset); i++)
			if (charset[i] == *text){
				printf("check %x %d\n", check, i);
				check = (check << 1) ^ i;
			};
		text++;
	}
	return check;
}
*/

void send_block(int freq, char *text){
	printf("Sending at %d, on %d [%s]\n", freq, strlen(text), text);
	//ft8_tx(text, freq);
}

struct message *add_chat(struct contact *pc, const char *message, int flags){
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
	m->nsent = 0;

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
	return m;
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

	printf("msg dump **********\n");
	for (pc = contact_list; pc; pc = pc->next){
		if (pc->last_update < time_sbitx() - 100)
			printf("%s %s (%d)\n", pc->callsign, pc->status, pc->frequency);
		else
			printf("%s %s (%d)\n", pc->callsign, pc->status, pc->frequency);
		for (pm = pc->m_list; pm; pm = pm->next)
			printf("   message: %d flag:%x  %.*s\n", 
				(int)(pm->length), pm->flags, (int)(pm->length), pm->data);	
	}
}

void update_chat(){
	const char *contact = field_str("CONTACT");
	if (!contact)
		return;
	if (!strcmp(contact, "LIST"))
		return;
			
	struct contact *pc = contact_by_callsign(contact);
	if (!pc)
		return;

	chat_clear();					
	for (struct message *pm = pc->m_list; pm; pm = pm->next){
		char message[1000];
		sprintf(message, "%s:\n%.*s\n", 
			pm->flags & MSG_INCOMING ? pc->callsign : field_str("MYCALLSIGN"),
			(int)(pm->length), pm->data);	
		chat_append(message);
	}
}

void update_contacts(){
	struct contact *pc;
	struct message *pm;
	char update_line[100];

	//send a form feed to clear the console
	update_line[0] = 12;
	update_line[1] = 0;
	write_console(0, update_line);
	clear_contact_list();

	for (pc = contact_list; pc; pc = pc->next){
		if (pc->last_update > time_sbitx() - 600){
			sprintf(update_line, "#G*%s #S%s\n", pc->callsign, pc->status);
			write_console(0, update_line);
			sprintf(update_line, "%s - %s", pc->callsign, pc->status);
			add_item_to_contact_list(update_line);
		}
		else{
			sprintf(update_line, "*%s %s\n", pc->callsign, pc->status);
			write_console(0, update_line);
			sprintf(update_line, "%s - Inactive", pc->callsign);
			add_item_to_contact_list(update_line);
		}
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

struct contact *contact_add(const char *callsign, int frequency){
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

int block_count(int length){
	int count = 0;
	//the last block has 3 character long checksum
	while(length > BLOCK_SIZE -3){
		//each of non-last blocks has a '+' in the end
		length -= (BLOCK_SIZE - 1);
		count++;
	}
	return count;
}

void msg_init(){
	memset(contact_block, 0, sizeof(contact_block));
	memset(message_block, 0, sizeof(message_block));
	memset(text_buffer, 0, sizeof(text_buffer));
	
	message_free = 0;
 	text_free = 0;
	contact_free = 0;	
	selected_contact[0] = 0;

	chat_ui_init();

	printf("%d of %s\n", block_count(strlen("Hello")), "Hello");
	printf("%d of %s\n", block_count(strlen("Hello world")), "Hello world");
	printf("%d of %s\n", block_count(strlen("Life,Universe,Everything")), 
		"Life,Universe,Everything");
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

struct contact *contact_by_callsign(const char *callsign){
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

struct contact *contact_by_block(int freq, const char *block){
	//we will just scan the block for the callsigns
	for (struct contact *pc = contact_list; pc; pc = pc->next)
		if (strstr(block, pc->callsign)) 
			return pc;

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

	for (i = 0; i < MAX_CALLSIGN-1 && isalnum(*notification); i++)
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
}

void msg_select(char *callsign){
	strcpy(selected_contact, callsign);
	field_set("CONTACT", callsign);
	printf("chatting with [%s]\n", selected_contact);
	
	struct contact *pc = contact_by_callsign(selected_contact);
	if (!pc)
		return;
	update_chat();
	
}

void msg_process(int freq, const char *text){
	char call[10];
	struct contact *pc;

	printf("msg_process: %d [%s]\n", freq, text);
	if (!strncmp(text, "CQ ", 3)){
		update_presence(freq, text + 3);	
		msg_dump();
	}
	else if (*text == '+'){
		update_presence(freq, text + 1);	
		msg_dump();
		update_contacts();
	}
	else if (pc = contact_by_block(freq, text)){
		add_chat(pc, text, MSG_INCOMING);
		update_chat();
	}
}


void send_update(){
	if (!strcmp(field_str("PRESENCE"), "SILENT"))
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

	next_update = time_sbitx() + 92;
	send_block(field_int("TX_PITCH"), notification);
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
	pm->nsent = 0;
	return 0;
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

	printf("msg time %d\n", (int)now); 
	//from here, we do stuff on every 15th second
	msg_dump();
	/* if (!strcmp(field_str("CONTACT"), "LIST"))
		update_contacts();
	else
		update_chat();*/
	update_contacts();

	if (next_update < now)
		send_update();

	//try sending the messages only after at least one 
	//notification update has been sent

	if (!next_update)
		return;

	//check if any online contact has an outgoing message
	struct contact *pc;
	struct message *pm;
	for (pc = contact_list; pc; pc = pc->next){
		if (now - pc->last_update < 500){
			for (pm = pc->m_list; pm; pm = pm->next){
				if((!(pm->flags & MSG_INCOMING)) && pm->nsent < pm->length){
					char block[BLOCK_SIZE+1];			

					int nsize = pm->length - pm->nsent;
					if (nsize > BLOCK_SIZE)
						nsize = BLOCK_SIZE;	
					strncpy(block, pm->data + pm->nsent, nsize);
					block[nsize] = 0;
					pm->nsent += nsize; 	
					send_block(field_int("TX_PITCH"), block);
				}
			} // next message of the contact 
		}
	} // next contact
}




