#define STATUS_STOP 1
#define STATUS_RUN 2
#define STATUS_ERROR 3

#define FILE_READ_FLAGS O_RDWR

#define QUEUE_LENGTH 2 // size more than 2 is redundant
#define BUFFER_SIZE 512*1024
#define FILE_NAME_MAX_LENGTH 1024

// signal data setting
//#define FRAME_HEAD_SIZE 264
//#define PULSE_HEAD_SIZE 24

#define FRAME_HEAD_STREAM_SIZE 288
#define PULSE_HEAD_STREAM_SIZE 96
#define FRAME_HEAD_MARK 0x731f44a8

typedef struct T_FRAME_HEAD{
	__uint32_t magic_num;	// frame mark
	__uint32_t version;	// version number
	char reserved1[16];	// reserved bytes
	__uint32_t UTC;		// time
	__uint32_t wave_code;	// wave code
	__uint32_t hopping_code;// hopping code
	__uint32_t start_freq;	// starting frequency
	__uint32_t sample_rate;	// sampling rate
	__uint32_t frame_index;	// index of frame
	char reserved2[216];	// reserved bytes
} frame_head;

typedef struct T_PULSE_HEAD{
	__uint16_t frame_index;
	__uint16_t pulse_index;
	__uint16_t gate_delay;
	__uint16_t gate_width;
	__uint32_t hopping_code;
	__uint32_t PRT;
	__uint32_t pulse_width;
	__uint32_t band_width;
} pulse_head;

typedef struct T_READER_THREAD_ARG{
	int offset; // how many packages to escape from the very beginning
	int points; // how many points in a package to retrieve
	int packages;	// how many packages to retrieve
	int channels;	// number of channels to obtain
	float ratio;	// beyond which ratio of the missing points should it be abandoned.
	int reg0;	// register 0 value setting
	int reg1;	// register 1 value setting
	int reg2;
	int reg3;
	int reg4;
	char file_name[FILE_NAME_MAX_LENGTH]; // file name to read data
	volatile int status; // status to control reader thread_fn
	float * queue[QUEUE_LENGTH]; // queue to store data from DMA
	volatile int queue_cur; // current element position in queue
	volatile int queue_lock[QUEUE_LENGTH]; // lock of queue elements
} reader_thread_arg_t;

/*
__uint64_t session(
	const char * file_name,
	int offset,
	int points, 
	int packages,
	int channels, 
	float ratio, 
	int reg0,
	int reg1
);

__uint64_t start(__uint64_t sess);

void stop(__uint64_t sess, __uint64_t tid);

int batch(__uint64_t sess, __uint64_t buf);
*/
