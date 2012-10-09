#include "link_layer.h"
#include "timeval_operators.h"
#include <pthread.h>
#include <sys/time.h>

unsigned short checksum(struct Packet);

void Send_Queue :: enqueue(Timed_packet p)
{
	if(isFull()) {
		//queue full
	}	
	else {
		if(tail==MAX_QUEUE_SIZE-1) {
			tail=0;
		}
		else {
			tail++;
		}
		data[tail]=p;
	}
	if(head == -1) {
		head = 0;
	}
}
Timed_packet Send_Queue :: dequeue()
{
	Timed_packet k;

	if(isEmpty()) {
		//Empty
	}
	else {
		k=data[head];
		if(head==tail) {
			head=tail=-1;
		}
		else if (head==MAX_QUEUE_SIZE-1) {
			head=0;
		}
		else {
			head++;
		}
	}
	return k;
}
bool Send_Queue :: isEmpty() {
	if(head == -1) {
		return true;
	}
	return false;
}
//Wrong!
int Send_Queue :: size(){
	return (tail - head);
}
bool Send_Queue :: isFull() {
	if((head==0 && tail==MAX_QUEUE_SIZE-1) || (tail+1==head)) {
		return true;
	}
	return false;
}
void Send_Queue :: removeLeft(unsigned int packetNum) {
	//Find packet in queue
	int location = -1;
	for(int i=head;i<size();i++){
		if(data[i].packet.header.seq == packetNum){
			location = i;
			break;		
		}
	}
	if(location != -1){
		//remove packets to left of found packet
		for(int i=head;i<location;i++){
			dequeue();
		}
	}
}


Link_layer::Link_layer(Physical_layer_interface* physical_layer_interface,
 unsigned int num_sequence_numbers,
 unsigned int max_send_window_size,unsigned int timeout):num_sequence_numbers(num_sequence_numbers),max_send_window_size(max_send_window_size)
{
	this->physical_layer_interface = physical_layer_interface;

	receive_buffer_length = 0;

	next_send_seq = 0;
	next_send_ack = 0;
	next_receive_seq = 0;
	last_receive_ack = 0;

	pthread_mutex_init(&lock,NULL);
	if (pthread_create(&thread,NULL,&Link_layer::loop,this) < 0) {
		throw Link_layer_exception();
	}

}

unsigned int Link_layer::send(unsigned char buffer[],unsigned int length)
{
	if(length == 0 || length > max_send_window_size) {
		throw Link_layer_exception();
	}
	if(!send_queue.isFull()) {
		Timed_packet P;
		gettimeofday(&P.send_time,NULL);
		std::copy(buffer,buffer+sizeof(buffer),P.packet.data);
		P.packet.header.data_length = length;
		P.packet.header.seq = next_send_seq;
		send_queue.enqueue(P);

		next_send_seq++;

		//now return true		
	}
	//Remove this	
	unsigned int n = physical_layer_interface->send(buffer,length);
	//Change to return false
	return n;
}

unsigned int Link_layer::receive(unsigned char buffer[])
{
	unsigned int length;

	pthread_mutex_lock(&lock);
	length = receive_buffer_length;
	if (length > 0) {
		for (unsigned int i = 0; i < length; i++) {
			buffer[i] = receive_buffer[i];
		}
		receive_buffer_length = 0;
	}
	pthread_mutex_unlock(&lock);

	return length;
}

void Link_layer::process_received_packet(struct Packet p)
{
	if (p.header.seq == next_receive_seq) {
		if (sizeof(p.data) > 0) {
			pthread_mutex_lock(&lock);
			if (receive_buffer_length == 0){
				//copy packet data to receive_buffer
				std::copy(p.data,p.data+sizeof(p.data),receive_buffer);
				//increment next_receive_seq
				next_receive_seq++;
			}
			pthread_mutex_unlock(&lock);
		}
		else {
			//increment next_receive_seq
			next_receive_seq++;
		}
	}
	last_receive_ack = p.header.ack;
}

void Link_layer::remove_acked_packets()
{
	send_queue.removeLeft(last_receive_ack);
}

void Link_layer::send_timed_out_packets()
{
}

void Link_layer::generate_ack_packet()
{
}

void* Link_layer::loop(void* thread_creator)
{
	const unsigned int LOOP_INTERVAL = 10;
	Link_layer* link_layer = ((Link_layer*) thread_creator);

	while (true) {
		pthread_mutex_lock(&link_layer->lock);
		if (link_layer->receive_buffer_length == 0) {
			unsigned int length =
			 link_layer->physical_layer_interface->receive
			 (link_layer->receive_buffer);

			if (length > 0) {
				link_layer->receive_buffer_length = length;
			}
		}
		pthread_mutex_unlock(&link_layer->lock);

		usleep(LOOP_INTERVAL);
	}

	return NULL;
}



// this is the standard Internet checksum algorithm
unsigned short checksum(struct Packet p)
{
	unsigned long sum = 0;
	struct Packet copy;
	unsigned short* shortbuf;
	unsigned int length;

	if (p.header.data_length > Link_layer::MAXIMUM_DATA_LENGTH) {
		throw Link_layer_exception();
	}

	copy = p;
	copy.header.checksum = 0;
	length = sizeof(Packet_header)+copy.header.data_length;
	shortbuf = (unsigned short*) &copy;

	while (length > 1) {
		sum += *shortbuf++;
		length -= 2;
	}
	// handle the trailing byte, if present
	if (length == 1) {
		sum += *(unsigned char*) shortbuf;
	}

	sum = (sum >> 16)+(sum & 0xffff);
	sum = (~(sum+(sum >> 16)) & 0xffff);
	return (unsigned short) sum;
}
