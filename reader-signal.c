// reader.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h> 

#include "reader.h"
#include "tfpgaad.h"

// parse a struct of frame head
void frame_head_from_bytes(frame_head * fh, char * buf){
	int offset = 0;
	memcpy(&fh->magic_num, buf + offset, sizeof(fh->magic_num));
	offset += sizeof(fh->magic_num);
	memcpy(&fh->version, buf + offset, sizeof(fh->version));
	offset += sizeof(fh->version);
	memcpy(&fh->reserved1, buf + offset, sizeof(fh->reserved1));
	offset += sizeof(fh->reserved1);
	memcpy(&fh->UTC, buf + offset, sizeof(fh->UTC));
	offset += sizeof(fh->UTC);
	memcpy(&fh->wave_code, buf + offset, sizeof(fh->wave_code));
	offset += sizeof(fh->wave_code);
	memcpy(&fh->hopping_code, buf + offset, sizeof(fh->hopping_code));
	offset += sizeof(fh->hopping_code);
	memcpy(&fh->start_freq, buf + offset, sizeof(fh->start_freq));
	offset += sizeof(fh->start_freq);
	memcpy(&fh->sample_rate, buf + offset, sizeof(fh->sample_rate));
	offset += sizeof(fh->sample_rate);
	memcpy(&fh->frame_index, buf + offset, sizeof(fh->frame_index));
	offset += sizeof(fh->frame_index);
	memcpy(&fh->reserved2, buf + offset, sizeof(fh->reserved2));
}

void pulse_head_from_bytes(pulse_head * ph, char * buf){
	int offset = 0;
	memcpy(&ph->frame_index, buf + offset, sizeof(ph->frame_index));
	//fprintf(stdout, "%d\n", ph->frame_index);
	offset += sizeof(ph->frame_index);
	memcpy(&ph->pulse_index, buf + offset, sizeof(ph->pulse_index));
	offset += sizeof(ph->pulse_index);
	memcpy(&ph->gate_delay, buf + offset, sizeof(ph->gate_delay));
	offset += sizeof(ph->gate_delay);
	memcpy(&ph->gate_width, buf + offset, sizeof(ph->gate_width));
	offset += sizeof(ph->gate_width);
	memcpy(&ph->hopping_code, buf + offset, sizeof(ph->hopping_code));
	offset += sizeof(ph->hopping_code);
	memcpy(&ph->PRT, buf + offset, sizeof(ph->PRT));
	offset += sizeof(ph->PRT);
	memcpy(&ph->pulse_width, buf + offset, sizeof(ph->pulse_width));
	offset += sizeof(ph->pulse_width);
	memcpy(&ph->band_width, buf + offset, sizeof(ph->band_width));
}

void session_dump(__uint64_t arg){
	reader_thread_arg_t * sess = (reader_thread_arg_t *)(arg);
	fprintf(stdout, "offset: %d\n", sess->offset);
	fprintf(stdout, "points: %d\n", sess->points);
	fprintf(stdout, "packages: %d\n", sess->packages);
	fprintf(stdout, "channels: %d\n", sess->channels);
	fprintf(stdout, "ratio: %.3f\n", sess->ratio);
	fprintf(stdout, "reg#0: 0x%x\n", sess->reg0);
	fprintf(stdout, "reg#1: 0x%x\n", sess->reg1);
	fprintf(stdout, "reg#2: %d\n", sess->reg2);
	fprintf(stdout, "reg#3: %d\n", sess->reg3);
	fprintf(stdout, "reg#4: %d\n", sess->reg4);
}

int search_mark(char * buf, int size){
	int i;
	__uint32_t v;
	for(i=0; i<size-sizeof(__uint32_t)+1; ++i){
		v = *(__uint32_t *)(buf+i);
		if(v == FRAME_HEAD_MARK){
			return i;
		}
		
	}
	return -1;
}

int pulses_from_bytes(
		float * data,
		char * bytes,
		int gate_width,
		int packages_to_read, // at least these should be continuous
		int packages_available,
		int offset, // number of points to escape
		int packages,
		int points, 
		int channels
){
	int i, j, k, m;
	int frame_index = -1;
	int last_pulse_index = -1;
	pulse_head ph;
	char * buf = bytes + FRAME_HEAD_STREAM_SIZE;
	if(gate_width < offset + points){
		return 1;
	}
	for(i=0; i<packages_available; ++i){
		if(i==packages){
			return 0;
		}
		// extract data from buffer
		k = i * (gate_width*8 + PULSE_HEAD_STREAM_SIZE);
		pulse_head_from_bytes(&ph, buf + k);
		if(frame_index==-1){
			frame_index = ph.frame_index;
		} else {
			if(frame_index!=ph.frame_index){
				return 2; // frame indexes do not match
			}
		}
		if(ph.pulse_index - last_pulse_index == 1){
			// no missing pulses
			k += PULSE_HEAD_STREAM_SIZE + 8*offset;
			for(j=0; j<points; ++j){
				for(m=0; m<channels; ++m){
					data[i*points*channels + j*channels + m] = (float)(*((__uint16_t*)(buf+(k+j*8+m*2))));
				}
			}
		} else {
			// missing packages!!!
			if(i>=packages_to_read){ // within tolerrance
				// filling with zeros
				for(j=0; j<points; ++j){
					for(m=0; m<channels; ++m){
						data[i*points*channels + j*channels + m] = 0;
					}
				}
			} else {
				// abandon the whole frame at all
				return 3;
			}
		}
	}
	// filling with zeros
	for(i=packages_available; i<packages; ++i){
		for(j=0; j<points; ++j){
			for(m=0; m<channels;++m){
				data[i*points*channels + j*channels + m] = 0;
			}
		}
	}
	return 0;
}

void * thread_fn(void * arg){
	int fd;
	char buf[2*BUFFER_SIZE]; // size is illegal?
	int num_read_bytes;
	float * data;
	int i, mark_offset, pulse_size;
	frame_head fh;
	pulse_head ph;
	int is_to_read;
	int min_frame_size;
	int packages_to_read;
	int packages_available;
	int buffer_size_used;
	int err;
	
	reader_thread_arg_t * sess = (reader_thread_arg_t *)(arg);
	data = (float *)malloc(sizeof(float)*sess->packages*sess->points*sess->channels);
	// open the device to receive signal data
	fd = open(sess->file_name, FILE_READ_FLAGS);
	if(fd==-1){
		fprintf(stdout, "Error: failed to open fpag device!\n");
		return NULL;
	}
	SetInternalSpiRegisters(fd);
	WDC_WriteAddr32(fd, 2, 0x804, sess->reg0);
	WDC_WriteAddr32(fd, 2, 0x808, sess->reg1);
	WDC_WriteAddr32(fd, 2, 0x848, sess->reg2);
	WDC_WriteAddr32(fd, 2, 0x83C, sess->reg3);
	WDC_WriteAddr32(fd, 2, 0x84C, sess->reg4);
	startDma(fd);
	buffer_size_used = 0;
	is_to_read = 1;
	
	fprintf(stdout, "Info: Thread started!\n");
	while(sess->status == STATUS_RUN){
		if(is_to_read){
			num_read_bytes = read(fd, buf + buffer_size_used, BUFFER_SIZE*sizeof(char));
			if(num_read_bytes<BUFFER_SIZE){
				fprintf(stdout, "Info: End of stream!\n");
				break;
			}
			buffer_size_used += BUFFER_SIZE;
		}
		// find the frame head mark
		mark_offset = search_mark(buf, buffer_size_used);
		if(mark_offset < 0){
			// magic number not found
			memmove(
				buf, 
				buf + (buffer_size_used - sizeof(fh.magic_num) + 1),
				(sizeof(fh.magic_num) - 1)*sizeof(char)
			);
			buffer_size_used = sizeof(fh.magic_num) - 1;
			is_to_read = 1;
		} else {
			// mark found
			// remove the extra bytes ahead
			buffer_size_used -= mark_offset;
			memmove(
				buf, 
				buf + mark_offset, 
				buffer_size_used*sizeof(char)
			);
			// find the next frame head
			mark_offset = search_mark(
				buf + sizeof(fh.magic_num), 
				buffer_size_used - sizeof(fh.magic_num)
			);
			if(mark_offset < 0){
				// if reading again will exceed the buffer size
				if(buffer_size_used > BUFFER_SIZE){
					memmove(
						buf, 
						buf + (buffer_size_used - sizeof(fh.magic_num) + 1),
						(sizeof(fh.magic_num) - 1)*sizeof(char)
					);
					buffer_size_used = sizeof(fh.magic_num) - 1;
					is_to_read = 1;
				} else {
					is_to_read = 1;
				}
			} else { // next frame head is found
				if(mark_offset < FRAME_HEAD_STREAM_SIZE + PULSE_HEAD_STREAM_SIZE){
					// this frame head is not complete in stream, delete it from stream
					memmove(
						buf,
						buf + mark_offset,
						(buffer_size_used - mark_offset) * sizeof(char)
					);
					buffer_size_used -= mark_offset;
					is_to_read = 0; // a second reading may not be needed
				} else {
					// load the pulse head
					pulse_head_from_bytes(&ph, buf + FRAME_HEAD_STREAM_SIZE);
					// debug
					/*if(1){
						fprintf(stdout, "%d\n", ph.frame_index);
					}*/
					// calculate the pulse size: n point * 4 channels * 2 bytes (uint16)
					pulse_size = ph.gate_width * 8;
					fprintf(stdout, "expected=%d, real=%d, frame_id=%d\n", sess->reg4*(pulse_size+PULSE_HEAD_STREAM_SIZE)+FRAME_HEAD_STREAM_SIZE, mark_offset, ph.frame_index);
					// check if missing pulses are beyond our tolerrance
					packages_to_read = (int)(floor(sess->packages * sess->ratio));
					packages_available = (mark_offset - FRAME_HEAD_STREAM_SIZE)/(PULSE_HEAD_STREAM_SIZE + pulse_size);
					if(packages_available < packages_to_read){
						// two many pulses missing, delete it from stream
						memmove(
							buf,
							buf + mark_offset,
							(buffer_size_used - mark_offset) * sizeof(char)
						);
						buffer_size_used -= mark_offset;
						is_to_read = 0; // a second reading may not be needed
					} else {
						// obtain pulses from a frame
						err = pulses_from_bytes(
							data, 
							buf,
							ph.gate_width,
							packages_to_read,
							packages_available,
							sess->offset, 
							sess->packages,
							sess->points, 
							sess->channels
						);
						if(!err){
							// lock it before any modification to global resource
							while(sess->queue_lock[sess->queue_cur]);
							sess->queue_lock[sess->queue_cur] =  1;
							memmove(
								(char*)sess->queue[sess->queue_cur], 
								(char*)data, 
								sizeof(float)*sess->packages*sess->points*sess->channels
							);
							// release lock
							sess->queue_lock[sess->queue_cur] = 0;
							sess->queue_cur = (sess->queue_cur + 1) % QUEUE_LENGTH;
						}
						memmove(
							buf,
							buf + mark_offset,
							(buffer_size_used - mark_offset) * sizeof(char)
						);
						buffer_size_used -= mark_offset;
						is_to_read = 0;
					} 
				}	
			}
		}
	}
	fprintf(stdout, "Stopping DMA!\n");
	stopDma(fd);
	fprintf(stdout, "Thread finished!\n");
	close(fd);

	return NULL;
}

__uint64_t session(
	const char * file_name,
	int offset, 
	int points, 
	int packages,
	int channels, 
	float ratio, 
	int reg0,
	int reg1,
	int reg2,
	int reg3,
	int reg4
){
	int i;
	reader_thread_arg_t * arg = (reader_thread_arg_t *)malloc(sizeof(reader_thread_arg_t));
	memset(arg->file_name, 0, sizeof(arg->file_name));
	strcpy(arg->file_name, file_name);
	arg->status = STATUS_RUN;
	arg->queue_cur = 0;
	arg->offset = offset;
	arg->points = points;
	arg->packages = packages;
	arg->channels = channels;
	arg->ratio = ratio;
	arg->reg0 = reg0;
	arg->reg1 = reg1;
	arg->reg2 = reg2;
	arg->reg3 = reg3;
	arg->reg4 = reg4; 
	
	// allocate memory for queue buffering and set the lock as free
	for(i=0; i<QUEUE_LENGTH; ++i){
		arg->queue[i] = (float *)malloc(sizeof(float)*arg->points*arg->packages*arg->channels);
		arg->queue_lock[i] = 0;
	}
	
	__uint64_t sess = (__uint64_t)arg;
	//fprintf(stdout, "c: %lu\n", sess);
	return sess;
}

__uint64_t start(__uint64_t arg){
	__uint64_t tid = 0;
	int ret;
	//fprintf(stdout, "c: %lu\n", arg);
	reader_thread_arg_t * sess = (reader_thread_arg_t *) arg;
	//fprintf(stdout, "%s\n", sess->file_name);
	//fprintf(stdout, "%d\n", sess->status);
	ret = pthread_create(&tid, NULL, thread_fn, (void*)sess);
	
	if(ret){
		fprintf(stdout, "error: failed to create reader thread in [%s:%d]!\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	return tid;
}

void stop(__uint64_t arg, __uint64_t tid){
	void * status;
	reader_thread_arg_t * sess = (reader_thread_arg_t *) arg;
	sess->status = STATUS_STOP;
	pthread_join(tid, &status);
}

int batch(__uint64_t arg, float * ndarr){
	// obtain a package from global queue
	// in NHWC format
	reader_thread_arg_t * sess = (reader_thread_arg_t *) arg;
	int n = sess->points * sess->packages * sess->channels;
	int id = (sess->queue_cur + QUEUE_LENGTH+1)%QUEUE_LENGTH;
	while(sess->queue_lock[id]);
	sess->queue_lock[id] = 1;
	memmove((char*)ndarr, (char*)sess->queue[id], sizeof(float)*n);
	sess->queue_lock[id] = 0;
	return 0;
}

__uint64_t r_alloc(int size){
	float * ptr = (float *)malloc(size * sizeof(float));	
	__uint64_t addr = (__uint64_t) ptr;
	//fprintf(stdout, "%lu\n", addr);
	return addr;
}

void r_free(__uint64_t addr){
	//fprintf(stdout, "%lu\n", addr);
	free((float *)addr);
}

__uint64_t alloc_float_arr(int size){
	return (__uint64_t)((float *)malloc(sizeof(float)*size));
}

void free_float_arr(__uint64_t ptr){
	free((float *)ptr);
}

float item_at(__uint64_t ptr, int idx){
	__uint16_t * arr = (__uint16_t *) ptr;
	return (float)(arr[idx]);
}

void set_ndarr(float * ndarr, __uint64_t ptr, int size){
	int i;
	float * carr = (float *)ptr;
	for(i=0; i<size; ++i){
		ndarr[i] = carr[i];
	}
}


