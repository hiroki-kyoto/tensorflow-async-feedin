#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define BUF_SIZE	(1024 * 512)
#define WRITEBARFLAG -198 
#define STARTFLAG -199
#define STOPFLAG  -200

#define CHNL_CNT 4

//-------------------------------

#define AD9520_REG_BASE     0x0000080C

#define AD9520_CONFIG_OFFSET 0x4

#define AD9265_REG_BASE     0x00000818
#define AD9265_CONFIG_OFFSET 0x4

#define READ 1
#define WRITE 0
//驱动约定的写一个寄存器的参数传递结构体
struct write_bar{
	int flag;	//驱动约定该值必须为WRITEBARFLAG= -198
	unsigned int bar;		//欲写入的bar编号：0-5
	unsigned int wr_addr;	//欲写入的寄存器bar内偏移地址
	unsigned int wr_data;	//欲写入的寄存器值
};

typedef struct{
	unsigned int clk_div;
	unsigned int miso_edge;
	unsigned int mosi_edge;
	unsigned int sck_reverse;
	unsigned int bit_num;
	unsigned int transfer;
	unsigned int chip_enable;
	unsigned int enable;
	unsigned int reset;
}SPI_CFG_REG;
typedef struct {
	unsigned int addr;
	unsigned int value;
}SPI_CFG;
SPI_CFG spi_cfg_array_92M[] = {
	{ 0x010, 0x02 },
	{ 0x011, 0x08 },
	{ 0x012, 0x00 },
	{ 0x013, 0x06 },
	{ 0x014, 0x0A },
	{ 0x015, 0x00 },
	{ 0x016, 0x05 },
	{ 0x017, 0x04 },
	{ 0x018, 0x81 },
	{ 0x019, 0x00 },
	{ 0x01A, 0x00 },
	{ 0x01B, 0xE2 },
	{ 0x01C, 0x02 },
	{ 0x01D, 0x08 },
	{ 0x01E, 0x01 },
	{ 0x0F0, 0x04 },
	{ 0x0F1, 0x04 },
	{ 0x0F2, 0x04 },
	{ 0x0F3, 0x04 },
	{ 0x0F4, 0x04 },
	{ 0x0F5, 0x04 },
	{ 0x0F6, 0x04 },
	{ 0x0F7, 0x04 },
	{ 0x190, 0x11 },  //ch 0
	{ 0x191, 0x80 },  //ch 0
	{ 0x192, 0x00 },  //ch 0
	{ 0x193, 0x11 },  //ch 1
	{ 0x194, 0x80 },  //ch 1
	{ 0x195, 0x00 },  //ch 1
	{ 0x196, 0x11 },
	{ 0x197, 0x80 },
	{ 0x198, 0x00 },
	{ 0x199, 0x11 },
	{ 0x19A, 0x80 },
	{ 0x19B, 0x00 },
	{ 0x1E0, 0x03 },
	{ 0x1E1, 0x01 },
	{ 0x230, 0x00 },
	{ 0x232, 0x01 },
	{ 0x018, 0x00 },
	{ 0x232, 0x01 },
	{ 0x018, 0x01 },
	{ 0x232, 0x01 }
};
void WDC_WriteAddr32(int fd, unsigned int bar, unsigned int address, unsigned int data)
{
           int num;
	struct write_bar write_buf[1];
	//对bar0的0x123地址写0xABCD
	write_buf[0].flag=WRITEBARFLAG;
	write_buf[0].bar=bar;
	write_buf[0].wr_addr=address;
	write_buf[0].wr_data=data;
        num=write(fd,write_buf,sizeof(struct write_bar));
}
void wait()
{
	usleep(30000);
}

SPI_CFG_REG setting;


void set_spi_frame(unsigned int addr, unsigned int rw, unsigned int w1w0, unsigned int data, unsigned int *spi_frame)
{
	int numofbytes;
	unsigned int datamask;

	switch (w1w0)
	{
	case 0:
		numofbytes = 1;
		datamask = 0xff;
		break;
	case 1:
		numofbytes = 2;
		datamask = 0xffff;
		break;
	case 2:
		numofbytes = 3;
		datamask = 0xffffff;
		break;
	default:
		numofbytes = 1;
		datamask = 0xff;
		break;
	}

	*spi_frame = ((rw & 0x1) << (numofbytes * 8 + 13 + 2) |
		(w1w0 & 0x3) << (numofbytes * 8 + 13) |
		(addr & 0x1fff) << (numofbytes * 8) |
		(data & datamask));
}

void set_spi_config_reg(int hDev, SPI_CFG_REG setting, unsigned int data)
{
	unsigned int val1 = (setting.enable & 0x00000001) << 30;
	unsigned int val2 = (setting.chip_enable & 0x00000001) << 29;
	unsigned int val3 = (setting.transfer & 0x00000001) << 27;
	unsigned int val4 = (setting.bit_num & 0x0000001f) << 20;
	unsigned int val5 = (setting.sck_reverse & 0x00000001) << 18;
	unsigned int val6 = (setting.mosi_edge & 0x00000001) << 17;
	unsigned int val7 = (setting.miso_edge & 0x00000001) << 16;
	unsigned int val8 = (setting.clk_div & 0x0000ffff);

	unsigned int regval = val1 | val2 | val3 | val4 | val5 | val6 | val7 | val8;

	unsigned int address = AD9520_REG_BASE + 0;
	//*(unsigned int *)address = data;
	WDC_WriteAddr32(hDev, 2, address, data);

	address = AD9520_REG_BASE + AD9520_CONFIG_OFFSET;

	//*(unsigned int *)address = regval;
	WDC_WriteAddr32(hDev, 2, address, regval);
}
void set_ad9265_spi_config_reg(int hDev,SPI_CFG_REG setting, unsigned int data)
{
	unsigned int val1 = (setting.enable & 0x00000001) << 30;
	unsigned int val2 = (setting.chip_enable & 0x00000001) << 29;
	unsigned int val3 = (setting.transfer & 0x00000001) << 27;
	unsigned int val4 = (setting.bit_num & 0x0000001f) << 20;
	unsigned int val5 = (setting.sck_reverse & 0x00000001) << 18;
	unsigned int val6 = (setting.mosi_edge & 0x00000001) << 17;
	unsigned int val7 = (setting.miso_edge & 0x00000001) << 16;
	unsigned int val8 = (setting.clk_div & 0x0000ffff);

	unsigned int regval = val1 | val2 | val3 | val4 | val5 | val6 | val7 | val8;

	unsigned int address = AD9265_REG_BASE + 0;
	//*(unsigned int *)address = data;
	WDC_WriteAddr32(hDev, 2, address, data);
	address = AD9265_REG_BASE + AD9265_CONFIG_OFFSET;

	//*(unsigned int *)address = regval;
	WDC_WriteAddr32(hDev, 2, address, regval);
}
void ad9520_init(int hDev)
{
	//*(unsigned int *)(AD9520_REG_BASE + 0x8) = 0x0f; // INT: 0x0f;   EXT: 0x0f= CLK_IN, 0x1f= REFIN1 (REFIN+)
	

	//setting.chip_enable = 0;
	unsigned int spi_frame = 0;

	set_spi_frame(0x000, WRITE, 0, 0xa5, &spi_frame);
	setting.transfer = 1;
	set_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(hDev, setting, spi_frame);
	wait();
	/*
	spi_frame = 0;
	set_spi_frame(0x004, WRITE, 0, 0x1, &spi_frame);
	setting.transfer = 1;
	set_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(hDev, setting, spi_frame);
	wait();
	*/
	spi_frame = 0;
	set_spi_frame(0x232, WRITE, 0, 0x1, &spi_frame);
	setting.transfer = 1;
	set_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(hDev, setting, spi_frame);
	wait();

	/*   set_spi_frame(0x003, READ, 0, 0, &spi_frame);
	setting.transfer = 1;
	set_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(hDev, setting, spi_frame);
	wait();*/
}
void ad9265_init(int hDev)
{
	//setting.chip_enable = 0;
	unsigned int spi_frame = 0;

	set_spi_frame(0x000, WRITE, 0, 0xa5, &spi_frame);
	setting.transfer = 1;
	//adc 1
	spi_frame = spi_frame | 0x00000000;
	set_ad9265_spi_config_reg(hDev,setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	spi_frame = spi_frame | 0x40000000;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	spi_frame = spi_frame | 0x80000000;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	spi_frame = spi_frame | 0xC0000000;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();



	set_spi_frame(0x14, WRITE, 0, 0x01, &spi_frame); //00: offset binary 01: twos complement 10:gray code 11: offset binary
	setting.transfer = 1;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	spi_frame = spi_frame | 0x40000000;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	spi_frame = spi_frame | 0x80000000;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	spi_frame = spi_frame | 0xC0000000;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();

	set_spi_frame(0xff, WRITE, 0, 0x01, &spi_frame);
	setting.transfer = 1;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	spi_frame = spi_frame | 0x40000000;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	spi_frame = spi_frame | 0x80000000;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	spi_frame = spi_frame | 0xC0000000;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_ad9265_spi_config_reg(hDev, setting, spi_frame);
	wait();
}
int SetInternalSpiRegisters(int fd)
{
	//   print("---Entering main---\n\r");

	//int n = 0;
	ad9265_init(fd);
	WDC_WriteAddr32(fd, 2, AD9520_REG_BASE + 0x8, 0x1f);
	ad9520_init(fd);
	
	unsigned int spi_frame;

	spi_frame = 0;

	// for external clock
	//setting.transfer = 1;
	//set_spi_frame(0x010, WRITE, 0, 0x02, &spi_frame); 
	//set_spi_config_reg(setting, spi_frame);
	//wait();
	//setting.transfer = 0;
	//set_spi_config_reg(setting, spi_frame);
	//wait();

	// ---------------------------------------------------
	// for internal clock start
	setting.transfer = 1;
	set_spi_frame(0x010, WRITE, 0, 0x7C, &spi_frame);
	set_spi_config_reg(fd,setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();



	setting.transfer = 1;
	set_spi_frame(0x011, WRITE, 0, 0x0A, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x012, WRITE, 0, 0x00, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x013, WRITE, 0, 0x00, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x014, WRITE, 0, 0x0A, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x015, WRITE, 0, 0x00, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x016, WRITE, 0, 0x05, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x017, WRITE, 0, 0x04, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x018, WRITE, 0, 0x81, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x019, WRITE, 0, 0x00, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x01A, WRITE, 0, 0x00, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x01B, WRITE, 0, 0xE2, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x01C, WRITE, 0, 0x44, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x01D, WRITE, 0, 0x08, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x01E, WRITE, 0, 0x01, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	// for internal clock end
	//------------------------------  OUT0  -----------------------------------------------------

	setting.transfer = 1;
	set_spi_frame(0x0F0, WRITE, 0, 0x04, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();


	//------------------------------  OUT2  -----------------------------------------------------

	setting.transfer = 1;
	set_spi_frame(0x0F2, WRITE, 0, 0x04, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	//------------------------------  OUT4  -----------------------------------------------------

	setting.transfer = 1;
	set_spi_frame(0x0F4, WRITE, 0, 0x04, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	//------------------------------  OUT6  -----------------------------------------------------

	setting.transfer = 1;
	set_spi_frame(0x0F6, WRITE, 0, 0x04, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();


	//setting.transfer = 1;
	//set_spi_frame(0x0FC, WRITE, 0, 0x00, &spi_frame);
	//set_spi_config_reg(setting, spi_frame);
	//wait();
	//setting.transfer = 0;
	//set_spi_config_reg(setting, spi_frame);
	//wait();

	//setting.transfer = 1;
	//set_spi_frame(0x0FD, WRITE, 0, 0x00, &spi_frame);
	//set_spi_config_reg(setting, spi_frame);
	//wait();
	//setting.transfer = 0;
	//set_spi_config_reg(setting, spi_frame);
	//wait();

	//------------------------------  OUT8  -----------------------------------------------------
	/*
	setting.transfer = 1;
	set_spi_frame(0x0F8, WRITE, 0, 0x04, &spi_frame);
	set_spi_config_reg(setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(setting, spi_frame);
	wait();
	*/
	//------------------------------  OUT10  -----------------------------------------------------
	/*
	setting.transfer = 1;
	set_spi_frame(0x0FA, WRITE, 0, 0x04, &spi_frame);
	set_spi_config_reg(setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(setting, spi_frame);
	wait();
	*/

	//------------------------------------------------------------------------------------------
	//--------------------------------Channel 1-------------------------------------------------
	//------------------------------------------------------------------------------------------
	//------------------------------------
	// for internal clock
	setting.transfer = 1;
	set_spi_frame(0x190, WRITE, 0, 0x11, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	//-----------------------------------------------------

	// ------------------------------------------------
	setting.transfer = 1;
	//set_spi_frame(0x191, WRITE, 0, 0x80, &spi_frame); //external
	set_spi_frame(0x191, WRITE, 0, 0x00, &spi_frame); //internal
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();


	setting.transfer = 1;
	set_spi_frame(0x192, WRITE, 0, 0x00, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();


	//------------------------------------------------------------------------------------------
	//--------------------------------Channel 2-------------------------------------------------
	//------------------------------------------------------------------------------------------
	// for internal 
	setting.transfer = 1;
	set_spi_frame(0x193, WRITE, 0, 0x11, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();


	setting.transfer = 1;
	//set_spi_frame(0x194, WRITE, 0, 0x80, &spi_frame); // external
	set_spi_frame(0x194, WRITE, 0, 0x00, &spi_frame); // internal
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x195, WRITE, 0, 0x00, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	//------------------------------------------------------------------------------------------
	//--------------------------------Channel 3-------------------------------------------------
	//------------------------------------------------------------------------------------------
	//for internal
	setting.transfer = 1;
	set_spi_frame(0x196, WRITE, 0, 0x11, &spi_frame); //0x00
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	//---------------------

	setting.transfer = 1;
	//set_spi_frame(0x197, WRITE, 0, 0x80, &spi_frame); //external
	set_spi_frame(0x197, WRITE, 0, 0x00, &spi_frame); //internal
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x198, WRITE, 0, 0x00, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	//------------------------------------------------------------------------------------------
	//--------------------------------Channel 4-------------------------------------------------
	//------------------------------------------------------------------------------------------
	// for internal 
	setting.transfer = 1;
	set_spi_frame(0x199, WRITE, 0, 0x11, &spi_frame); //0x00
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	// ----------------------------

	setting.transfer = 1;
	//set_spi_frame(0x19A, WRITE, 0, 0x80, &spi_frame); //external
	set_spi_frame(0x19A, WRITE, 0, 0x00, &spi_frame); //internal
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x19B, WRITE, 0, 0x00, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	//------------------------------------------------------------------------------------------
	// internal
	setting.transfer = 1;
	set_spi_frame(0x1E0, WRITE, 0, 0x03, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x1E1, WRITE, 0, 0x02, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	//--------------------------------------------------------

	// for external
	//setting.transfer = 1;
	//set_spi_frame(0x1E1, WRITE, 0, 0x01, &spi_frame);
	//set_spi_config_reg(setting, spi_frame);
	//wait();
	//setting.transfer = 0;
	//set_spi_config_reg(setting, spi_frame);
	//wait();


	setting.transfer = 1;
	set_spi_frame(0x230, WRITE, 0, 0x00, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x232, WRITE, 0, 0x01, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	//-----------------------------------------------------------------------------------
	//------------------------------ VCO Calibration-------------------------------------
	//-----------------------------------------------------------------------------------

	setting.transfer = 1;
	set_spi_frame(0x018, WRITE, 0, 0x00, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();


	setting.transfer = 1;
	set_spi_frame(0x232, WRITE, 0, 0x01, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();

	setting.transfer = 1;
	set_spi_frame(0x018, WRITE, 0, 0x01, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();


	setting.transfer = 1;
	set_spi_frame(0x232, WRITE, 0, 0x01, &spi_frame);
	set_spi_config_reg(fd, setting, spi_frame);
	wait();
	setting.transfer = 0;
	set_spi_config_reg(fd, setting, spi_frame);
	wait();


	//-----------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------

	//print("---Exiting main---\n\r");

	return 0;
}
int SetExternalSpiRegisters(int fd)
{
	//   print("---Entering main---\n\r");

	int n = 0;
	ad9265_init(fd);
	WDC_WriteAddr32(fd, 2, AD9520_REG_BASE + 0x8, 0x0f);
	ad9520_init(fd);
	unsigned int spi_frame;
	spi_frame = 0;
     
	for (n = 0; n < sizeof(spi_cfg_array_92M) / sizeof(SPI_CFG); n++)
		{
			setting.transfer = 1;
			set_spi_frame(spi_cfg_array_92M[n].addr, WRITE, 0, spi_cfg_array_92M[n].value, &spi_frame);
			set_spi_config_reg(fd, setting, spi_frame);
			wait();
			setting.transfer = 0;
			set_spi_config_reg(fd, setting, spi_frame);
			wait();
		}
	return 0;
}
void startDma(int fd)
{       
        int num;
	struct write_bar write_buf[1];
	//对bar0的0x123地址写0xABCD
	write_buf[0].flag=STARTFLAG;
	write_buf[0].bar=-1;
	write_buf[0].wr_addr=-1;
	write_buf[0].wr_data=-1;
        num=write(fd,write_buf,sizeof(struct write_bar));
}
void stopDma(int fd)
{
        int num;
	struct write_bar write_buf[1];
	//对bar0的0x123地址写0xABCD
	write_buf[0].flag=STOPFLAG;
	write_buf[0].bar=-1;
	write_buf[0].wr_addr=-1;
	write_buf[0].wr_data=-1;
        num=write(fd,write_buf,sizeof(struct write_bar));
}


