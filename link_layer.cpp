#include "link_layer.h"
#include "timeval_operators.h"
#include <pthread.h>
#include <sys/time.h>
#include <cstring>


unsigned short checksum(struct Packet);

Link_layer::Link_layer(Physical_layer_interface* physical_layer_interface,
 unsigned int num_sequence_numbers,
 unsigned int max_send_window_size,unsigned int timeout)
{
	this->physical_layer_interface = physical_layer_interface;
	this->max_send_window_size = max_send_window_size;
	this->num_sequence_numbers = num_sequence_numbers;
	this->timeout.tv_usec = timeout;

	receive_buffer_length = 0;

	next_send_seq = 0;
	next_receive_seq = 0;
	last_receive_ack = 0;
	send_queue_size=0;
	send_queue_front = 0;

	send_queue = new Timed_packet[max_send_window_size];

	new_buffer = new unsigned char[Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];
	decon_buffer = new unsigned char[Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];

	pthread_mutex_init(&receive_buffer_lock,NULL);
	pthread_mutex_init(&send_lock,NULL);
	if (pthread_create(&thread,NULL,&Link_layer::loop,this) < 0) {
		throw Link_layer_exception();
	}

}

void Link_layer::enqueue(Timed_packet p){
	send_queue[(send_queue_front+send_queue_size)%max_send_window_size] = p;
	send_queue_size++;
}

bool Link_layer::is_queue_full(){
	return (send_queue_size == max_send_window_size);
}

unsigned int Link_layer::send(unsigned char buffer[],unsigned int length)
{
	if(length == 0 || length > MAXIMUM_DATA_LENGTH) {
		throw Link_layer_exception();
	}

	if(!is_queue_full()) {
		Timed_packet P;
		gettimeofday(&P.send_time,NULL);
		//std::copy(buffer,buffer+length,P.packet.data);
		memcpy(P.packet.data, buffer, length);		
		P.packet.header.data_length = length;
		
		//shared variable 
		pthread_mutex_lock(&send_lock);
		P.packet.header.seq = next_send_seq;


		enqueue(P);

		//shared variable 

		next_send_seq = (next_send_seq + 1) % num_sequence_numbers;


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
	pthread_mutex_lock(&receive_buffer_lock);
	//cout << "receive seq number: " << next_receive_seq  << endl;
	if (p.header.seq == next_receive_seq) {
		//cout << "ELLLOOOO!" << endl;
		if (p.header.data_length > 0) {

			if (receive_buffer_length == 0){
				//copy packet data to receive_buffer
				//std::copy(p.data,p.data+sizeof(p.data),receive_buffer);
				memcpy(receive_buffer, p.data, p.header.data_length);
				
				receive_buffer_length = p.header.data_length;
				//increment next_receive_seq
				next_receive_seq = (next_receive_seq + 1) % num_sequence_numbers;
			}

		}
		else {
			//increment next_receive_seq

			next_receive_seq = (next_receive_seq + 1) % num_sequence_numbers;

		}
	}
	//cout << "receive seq number agin: " << next_receive_seq  << endl;
	last_receive_ack = p.header.ack;

	pthread_mutex_unlock(&receive_buffer_lock);
}

void Link_layer::remove_acked_packets()
{
	unsigned int size = send_queue_size;
	unsigned int front = send_queue_front;
	
	for (unsigned int i = 0; i < size; i++) {
		Timed_packet p = send_queue[(front + i) % max_send_window_size];
		unsigned int seq_num = p.packet.header.seq;
		seq_num = (seq_num + 1) % num_sequence_numbers;
		if(seq_num == last_receive_ack){
			send_queue_front = (front + i + 1) % max_send_window_size;
			send_queue_size -= (i + 1);	
		}		
	}
	
}

void Link_layer::send_timed_out_packets()
{

	pthread_mutex_lock(&send_lock);

	if(send_queue_size != 0){
		for (unsigned int i = 0; i < send_queue_size; i++) {
			Timed_packet* P = &send_queue[(send_queue_front + i) % max_send_window_size];
			timeval now;
			(gettimeofday(&now,NULL));
			if(P->send_time < now ){
				//cout << "sending a packet:" << P.packet.header.data_length << endl;
				P->packet.header.ack = next_receive_seq;
				P->packet.header.checksum = checksum(P->packet);

				//Construct char[] from header and data
				
				//unsigned char checksum_buffer = static_cast<unsigned char>(P.packet.header.checksum);
				//unsigned char seq_buffer = static_cast<unsigned char>(P.packet.header.seq);
				//unsigned char ack_buffer = static_cast<unsigned char>(P.packet.header.ack);
				//unsigned char data_length_buffer = static_cast<unsigned char>(P.packet.header.data_length);
				//unsigned char* something_buffer = checksum_buffer+seq_buffer+ack_buffer+data_length_buffer+P.packet.data;

				//cout << "sothing: " <<something_buffer[4] << endl;
				//memcpy(new_buffer, (unsigned char*)&P.packet.header.checksum, sizeof(unsigned int));
				//std::copy((unsigned char*)&P.packet.header.checksum,(unsigned char*)&P.packet.header.checksum+sizeof(int),new_buffer);
				//memcpy((new_buffer+sizeof(unsigned int)), (unsigned char*)&P.packet.header.seq, sizeof(int));
				//std::copy((unsigned char*)&P.packet.header.seq,(unsigned char*)&P.packet.header.seq+sizeof(int),new_buffer+sizeof(int));
				//memcpy((new_buffer+2*sizeof(unsigned int)), (unsigned char*)&P.packet.header.ack, sizeof(int));
				//std::copy((unsigned char*)&P.packet.header.ack,(unsigned char*)&P.packet.header.ack+sizeof(int),new_buffer+2*sizeof(int));
				//memcpy((new_buffer+3*sizeof(unsigned int)), (unsigned char*)&P.packet.header.data_length, sizeof(int));
				//std::copy((unsigned char*)&P.packet.header.data_length,(unsigned char*)&P.packet.header.data_length+sizeof(int),new_buffer+3*sizeof(int));
			
				//memcpy((new_buffer+4*sizeof(unsigned int)), P.packet.data, P.packet.header.data_length);
				//std::copy((unsigned char*)&P.packet.data,(unsigned char*)&P.packet.data+sizeof(int),new_buffer+4*sizeof(int));


				unsigned int packet_length = P->packet.header.data_length + sizeof(Packet_header);

				unsigned char something_buffer[Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];
				memcpy(something_buffer, &P->packet, packet_length);

	
				if(physical_layer_interface->send(something_buffer,packet_length)){
					gettimeofday(&now,NULL);
					P->send_time = now  + timeout;
				}
			}
		}
	}
	
	pthread_mutex_unlock(&send_lock);
}

void Link_layer::generate_ack_packet()
{

	pthread_mutex_lock(&send_lock);
	if(send_queue_size==0){
		Timed_packet P;
		gettimeofday(&P.send_time,NULL);

		P.packet.header.seq = next_send_seq;

		P.packet.header.data_length = 0;

		enqueue(P);

		next_send_seq = (next_send_seq + 1) % num_sequence_numbers;

	}

	pthread_mutex_unlock(&send_lock);
	
}

void* Link_layer::loop(void* thread_creator)
{
	const unsigned int LOOP_INTERVAL = 10;
	Link_layer* link_layer = ((Link_layer*) thread_creator);

	while (true) {
		//For received packets

		unsigned int length = link_layer->physical_layer_interface->receive(link_layer->decon_buffer);
		if (length != 0) {
			//Need to reconstruct header
			//p.header.checksum = static_cast<unsigned int>(link_layer->decon_buffer[0]);
			//memcpy(&p.header.checksum, link_layer->decon_buffer, sizeof(unsigned int));
			//std::copy(link_layer->decon_buffer, link_layer->decon_buffer+sizeof(int),&p.header.checksum);
			//cout << "checksum: " <<  p.header.checksum << endl;
			//memcpy(&p.header.seq, (link_layer->decon_buffer+sizeof(unsigned int)), sizeof(unsigned int));
			//std::copy(link_layer->decon_buffer+sizeof(int), link_layer->decon_buffer+2*sizeof(int),&p.header.seq);
			//cout << "sequence: " <<  p.header.seq << endl;
			//memcpy(&p.header.ack, (link_layer->decon_buffer+2*sizeof(unsigned int)), sizeof(unsigned int));
			//std::copy(link_layer->decon_buffer+2*sizeof(int), link_layer->decon_buffer+3*sizeof(int),&p.header.ack);
			//cout << "ack: " <<  p.header.ack << endl;
		
			//p.header.data_length = static_cast<unsigned int>(link_layer->decon_buffer[0]);
			//memcpy(&p.header.data_length, (link_layer->decon_buffer+3*sizeof(unsigned int)), sizeof(unsigned int));
			//std::copy(link_layer->decon_buffer+3*sizeof(int), link_layer->decon_buffer+4*sizeof(int),&p.header.data_length);
			//cout << "data_length: " <<  p.header.data_length << endl;

			//Currently doesn't receive any data_length for some reason
			
			//Reconstruct data
			//memcpy(&p.data, (link_layer->decon_buffer+4*sizeof(unsigned int)), p.header.data_length);

			Packet p;
			memcpy(&p, link_layer->decon_buffer, length);
			
			if(length >= sizeof(Packet_header) && length <= Physical_layer_interface::MAXIMUM_BUFFER_LENGTH && p.header.checksum == checksum(p) && p.header.data_length <= (Physical_layer_interface::MAXIMUM_BUFFER_LENGTH-sizeof(Packet_header))) {
				
				link_layer->process_received_packet(p);
			}
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
