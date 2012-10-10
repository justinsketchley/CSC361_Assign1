#include "link_layer.h"
#include "timeval_operators.h"
#include <pthread.h>
#include <sys/time.h>
#include <cstring>


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
void Send_Queue :: removeLeft(unsigned int packet_num) {
	if(isEmpty()){
		return;
	}
	//Find packet in queue
	int location = -1;
	for(int i=head;i<size();i++){
		if(data[i].packet.header.seq == packet_num){
			location = i;
			break;		
		}
	}
	if(location != -1){
		//remove packets to left of found packet
		for(int i=head;i<=location;i++){
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
	next_receive_seq = 0;
	last_receive_ack = 0;

	new_buffer = new unsigned char[Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];
	decon_buffer = new unsigned char[Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];

	pthread_mutex_init(&receive_buffer_lock,NULL);
	pthread_mutex_init(&send_lock,NULL);
	if (pthread_create(&thread,NULL,&Link_layer::loop,this) < 0) {
		throw Link_layer_exception();
	}

}

unsigned int Link_layer::send(unsigned char buffer[],unsigned int length)
{
	if(length == 0 || length > MAXIMUM_DATA_LENGTH) {
		throw Link_layer_exception();
	}
	if(!send_queue.isFull()) {
		Timed_packet P;
		gettimeofday(&P.send_time,NULL);
		std::copy(buffer,buffer+length,P.packet.data);
		//if(length> 1) {
		//	cout << "add to send queue: " << length << endl;
		//}
		P.packet.header.data_length = length;
		
		//shared variable 
		pthread_mutex_lock(&send_lock);
		P.packet.header.seq = next_send_seq;
		pthread_mutex_unlock(&send_lock);
		send_queue.enqueue(P);

		//shared variable 
		pthread_mutex_lock(&send_lock);
		next_send_seq++;
		pthread_mutex_unlock(&send_lock);

		//now its length not true
		return length;		
	}
	//actually it is 0 instead of false
	return 0;
}

unsigned int Link_layer::receive(unsigned char buffer[])
{
	unsigned int length;

	pthread_mutex_lock(&receive_buffer_lock);
	length = receive_buffer_length;
	if (length > 0) {
		for (unsigned int i = 0; i < length; i++) {
			buffer[i] = receive_buffer[i];
		}
		receive_buffer_length = 0;
	}
	pthread_mutex_unlock(&receive_buffer_lock);

	return length;
}

void Link_layer::process_received_packet(struct Packet p)
{
	//cout << "receive seq number: " << next_receive_seq  << endl;
	if (p.header.seq == next_receive_seq) {
		cout << "ELLLOOOO!" << endl;
		if (sizeof(p.data) > 0) {
			pthread_mutex_lock(&receive_buffer_lock);
			if (receive_buffer_length == 0){
				//copy packet data to receive_buffer
				std::copy(p.data,p.data+sizeof(p.data),receive_buffer);
				receive_buffer_length = p.header.data_length;
				//increment next_receive_seq
				next_receive_seq++;
			}
			pthread_mutex_unlock(&receive_buffer_lock);
		}
		else {
			//increment next_receive_seq
			pthread_mutex_lock(&receive_buffer_lock);
			next_receive_seq++;
			pthread_mutex_unlock(&receive_buffer_lock);
		}
	}
	//cout << "receive seq number agin: " << next_receive_seq  << endl;
	last_receive_ack = p.header.ack;

}

void Link_layer::remove_acked_packets()
{
	send_queue.removeLeft(last_receive_ack);
}

void Link_layer::send_timed_out_packets()
{
	if(!send_queue.isEmpty()){
		for(int i=send_queue.head;i<send_queue.tail;i++){
			Timed_packet P = send_queue.data[i];
			timeval now;
			(gettimeofday(&now,NULL));
			if(P.send_time < now ){
				//cout << "sending a packet:" << P.packet.header.data_length << endl;
				P.packet.header.ack = next_receive_seq;
				P.packet.header.checksum = checksum(P.packet);

				//Construct char[] from header and data
				//memcpy(new_buffer, (unsigned char*)&P.packet.header.checksum, sizeof(int));
				std::copy((unsigned char*)&P.packet.header.checksum,(unsigned char*)&P.packet.header.checksum+sizeof(int),new_buffer);
				//memcpy((new_buffer+sizeof(int)), (unsigned char*)&P.packet.header.seq, sizeof(int));
				std::copy((unsigned char*)&P.packet.header.seq,(unsigned char*)&P.packet.header.seq+sizeof(int),new_buffer+sizeof(int));
				//memcpy((new_buffer+2*sizeof(int)), (unsigned char*)&P.packet.header.ack, sizeof(int));
				std::copy((unsigned char*)&P.packet.header.ack,(unsigned char*)&P.packet.header.ack+sizeof(int),new_buffer+2*sizeof(int));
				//memcpy((new_buffer+3*sizeof(int)), (unsigned char*)&P.packet.header.data_length, sizeof(int));
				std::copy((unsigned char*)&P.packet.header.data_length,(unsigned char*)&P.packet.header.data_length+sizeof(int),new_buffer+3*sizeof(int));
			
				//memcpy((new_buffer+4*sizeof(int)), P.packet.data, P.packet.header.data_length);
				std::copy((unsigned char*)&P.packet.data,(unsigned char*)&P.packet.data+sizeof(int),new_buffer+4*sizeof(int));	
	
				//std::copy(P.packet.header.checksum,P.packet.header.checksum+sizeof(int),new_buffer);
				//std::copy(P.packet.header.seq,P.packet.header.seq+sizeof(int),new_buffer+sizeof(int));
				//std::copy(P.packet.header.ack,P.packet.header.ack+sizeof(int),new_buffer+(sizeof(int)*2));
				//std::copy(P.packet.header.checksum,P.packet.header.data_length_buffer+sizeof(int),new_buffer+(sizeof(int)*3));
				if(physical_layer_interface->send(new_buffer,sizeof(new_buffer))){
					gettimeofday(&now,NULL);
					P.send_time = now  + timeout;
				}
			}
		}
	}
}

void Link_layer::generate_ack_packet()
{

	if(send_queue.isEmpty()){
		Timed_packet P;
		gettimeofday(&P.send_time,NULL);

		pthread_mutex_lock(&send_lock);
		P.packet.header.seq = next_send_seq;
		pthread_mutex_unlock(&send_lock);

		P.packet.header.data_length = 0;

		pthread_mutex_lock(&send_lock);
		next_send_seq++;
		pthread_mutex_unlock(&send_lock);
	}
	
}

void* Link_layer::loop(void* thread_creator)
{
	const unsigned int LOOP_INTERVAL = 10;
	Link_layer* link_layer = ((Link_layer*) thread_creator);

	while (true) {
		//For received packets
		Packet p;
		unsigned int length = link_layer->physical_layer_interface->receive(link_layer->decon_buffer);
		if (length != 0) {
			//Need to reconstruct header
			//memcpy(&p.header.checksum, link_layer->decon_buffer, sizeof(int));
			std::copy(link_layer->decon_buffer, link_layer->decon_buffer+sizeof(int),&p.header.checksum);
			//cout << "checksum: " <<  p.header.checksum << endl;
			//memcpy(&p.header.seq, (link_layer->decon_buffer+sizeof(int)), sizeof(int));
			std::copy(link_layer->decon_buffer+sizeof(int), link_layer->decon_buffer+2*sizeof(int),&p.header.seq);
			//cout << "sequence: " <<  p.header.seq << endl;
			//memcpy(&p.header.ack, (link_layer->decon_buffer+2*sizeof(int)), sizeof(int));
			std::copy(link_layer->decon_buffer+2*sizeof(int), link_layer->decon_buffer+3*sizeof(int),&p.header.ack);
			//cout << "ack: " <<  p.header.ack << endl;
			//memcpy(&p.header.data_length, (link_layer->decon_buffer+3*sizeof(int)), sizeof(int));
			std::copy(link_layer->decon_buffer+3*sizeof(int), link_layer->decon_buffer+4*sizeof(int),&p.header.data_length);
			//cout << "data_length: " <<  p.header.data_length << endl;

			//Reconstruct data
			memcpy(&p.data, (link_layer->decon_buffer+4*sizeof(int)), p.header.data_length);

			//p.header.checksum = checksum(p);
			//cout<<"max data length: "<<MAXIMUM_DATA_LENGTH << endl;
			if (length > 0 && length <= Physical_layer_interface::MAXIMUM_BUFFER_LENGTH) {
				//cout << "do i get here?" <<endl;
				link_layer->process_received_packet(p);
			}
			//cout << "second something" << endl;
		}

		//For sent packets
		link_layer->remove_acked_packets();
		link_layer->send_timed_out_packets();
		
		//Pause
		usleep(LOOP_INTERVAL);

		//Acknowledgement packets
		link_layer->generate_ack_packet();		
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
