#ifndef _GCODE_H_
#define _GCODE_H_

#define TFT28		0
#define TFT24		1
#define ROBIN		2

typedef enum
{
	PRINTER_NOT_CONNECT,
	PRINTER_IDLE,
	PRINTER_PRINTING,
	PRINTER_PAUSE,
} PRINT_STATE;


typedef struct
{
	int print_rate;
	int print_hours;
	int print_mins;
	int print_seconds;
	String file_name;
	int file_size;
} PRINT_FILE_INF;

typedef struct
{
	float curSprayerTemp[2];	// 2个喷头温度
	float curBedTemp;	//热床温度
	float desireSprayerTemp[2];// 2个喷头目标温度
	float desireBedTemp;// 热床目标温度
	
	String sd_file_list;
	//String sd_file_list_t; //缓存
	//String udisk_file_list;	
	//String udisk_file_list_t;//缓存

	PRINT_STATE print_state;	//打印状态
	PRINT_FILE_INF print_file_inf;

	
} PRINT_INF;

extern char M3_TYPE;
extern boolean GET_VERSION_OK;

extern PRINT_INF gPrinterInf;
extern bool file_list_flag;
extern bool getting_file_flag;

extern File treeFile;

#ifdef __cplusplus
extern "C" {
#endif

extern void paser_cmd(uint8_t *cmdRxBuf);
extern void net_print(const uint8_t *sbuf, uint32_t len);

#ifdef __cplusplus
}
#endif
#endif

