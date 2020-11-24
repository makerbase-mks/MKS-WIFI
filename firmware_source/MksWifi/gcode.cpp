#include <stdint.h>
#include <string.h>
#include <math.h>
#include <WString.h>
#include <HardwareSerial.h>
#include <FS.h>
#include "gcode.h"


uint8_t temp_update_flag = 0;

PRINT_INF gPrinterInf;


uint8_t  DecStr2Float(int8_t * buf,  float  *result)
{
	int  index = 0;
	
	float  retVal = 0;
	int8_t  dot_flag = 0;
	int8_t  negat_flag = 0;
		
	if(buf == 0  ||  result == 0)

	{
		return  0;
	}

	do
	{
		if((buf[index] <= '9')  &&  (buf[index]  >= '0'))
		{
			if(dot_flag)
			{
				retVal  += (float)((buf[index] - '0') * pow(10, (0 - dot_flag)));
				dot_flag++;
			}
			else
			{
				retVal  *=  10;
				retVal  += buf[index] - '0';
			}
			
		}
		else if(buf[index]  == '.')
		{
			dot_flag = 1;
		}
		else if(buf[index] == '-')
		{
				negat_flag = 1;
		}
		else
		{
				if(negat_flag)
				{
					*result = (float)0 - retVal;
				}
				else
				{
						*result = retVal;
				}
			
			return 1;
		}
		index++;
			
	} while(1);
	
}

uint8_t  DecStr2Int(int8_t * buf,  int  *result)
{
	int  index = 0;
	
	int  retVal = 0;
	int8_t  dot_flag = 0;
	int8_t  negat_flag = 0;
		
	if(buf == 0  ||  result == 0)
	{
		return  0;
	}

	do
	{
		if((buf[index] <= '9')  &&  (buf[index]  >= '0'))
		{
			
			retVal  *=  10;
			retVal  += buf[index] - '0';
			
		}
		else if(buf[index] == '-')
		{
				negat_flag = 1;
		}
		else
		{
				if(negat_flag)
				{
					*result = 0 - retVal;
				}
				else
				{
						*result = retVal;
				}
			return 1;
		}
		index++;
			
	} while(1);
	
}

void strDelChar(char *a, int32_t len, char ch)
{ 
	int i, j;
	
	for(i = 0, j = 0; i < len; i++)    
		if(a[i] != ch)        
			a[j++] = a[i];   
	
	a[j] = '\0';  
	
}

static uint8_t get_temper_flg = 0;
uint8_t FanSpeed_bak = 0;

bool file_list_flag = false;
bool getting_file_flag = false;

//File treeFile;

void paser_cmd(uint8_t *cmdRxBuf)
{
		
	int8_t *tmpStr = 0;
	int8_t *tmpStr_e = 0;
	
	float  tmpTemp = 0;
	int8_t rcv_ack_flag = 0;
	int32_t i, j, k;
	int8_t inc_flag = 0;
	int8_t num_valid = 0;
	//int8_t cmdRxBuf[128] = {0};
	int8_t  tempBuf[100] = {0};
		
	get_temper_flg = 0;
	i = 0;

		

	String line((const char *)cmdRxBuf);
	//line.trim();

	
	
	if(file_list_flag)
	{
		if(line.startsWith("End file list"))
		{
			//gPrinterInf.sd_file_list.copy(gPrinterInf.sd_file_list_t.c_str(), gPrinterInf.sd_file_list_t.length());
			//gPrinterInf.udisk_file_list.copy(gPrinterInf.udisk_file_list_t.c_str(), gPrinterInf.udisk_file_list_t.length());
		//	gPrinterInf.sd_file_list.remove(0, gPrinterInf.sd_file_list.length());
		//	gPrinterInf.sd_file_list.concat(gPrinterInf.sd_file_list_t);
	/*	if(!treeFile)
		{
			treeFile.close();
		}*/
			file_list_flag = false;
			getting_file_flag = false;
//////////			net_print((const uint8_t *)"paser:end", strlen("paser:end"));
		}
		else
		{
			//net_print((const uint8_t *)"paser:", strlen("paser:"));
			//net_print((const uint8_t *)(const char *)(line.c_str()), strlen((const char *)(line.c_str())));
		
			if(line.endsWith(".g\n") || line.endsWith(".G\n")  
				|| line.endsWith(".gc\n")  || line.endsWith(".GC\n") 
				|| line.endsWith(".gco\n")  || line.endsWith(".GCO\n") 
				|| line.endsWith(".gcode\n")  || line.endsWith(".GCODE\n") 
				|| line.endsWith(".DIR\n"))
			{

				if(gPrinterInf.sd_file_list.length() + line.length() < 1024)
				{
					gPrinterInf.sd_file_list.concat(line);
				}
			//	Serial.print("sd_file_list_t:");
			//	Serial.println(gPrinterInf.sd_file_list_t);
				//net_print((const uint8_t *)"sd_file_list_t:", strlen("sd_file_list_t:"));
				//net_print((const uint8_t *)(const char *)(gPrinterInf.sd_file_list_t.c_str()), 
				//	strlen((const char *)(gPrinterInf.sd_file_list_t.c_str())));
			/*	if(treeFile)
				{
					treeFile.write((const uint8_t*)line.c_str(), line.length());
				}*/
				return;
			}
			
  
		}
	}

	if(line.startsWith("Begin file list"))
	{

		gPrinterInf.sd_file_list.remove(0, gPrinterInf.sd_file_list.length());
	//	gPrinterInf.udisk_file_list_t.remove(0, gPrinterInf.udisk_file_list_t.length());
	
		file_list_flag = true;
		
		return;
		
	}
	if(line.startsWith("M997 "))
	{
	//	net_print((const uint8_t *)line.c_str(), strlen((const char *)line.c_str()));
		if(line.startsWith("IDLE", 5))
		{
			gPrinterInf.print_state = PRINTER_IDLE;
		}
		else if(line.startsWith("PAUSE", 5))
		{
			gPrinterInf.print_state = PRINTER_PAUSE;
		}
		else if(line.startsWith("PRINTING", 5))
		{
			gPrinterInf.print_state = PRINTER_PRINTING;
		}
		return;
	}
	else if(line.startsWith("M994 ")) //file name and file size
	{
		line = line.substring(5);
		int index = line.indexOf(';');
		if(index == -1)
			return;
		line.trim();
		
		gPrinterInf.print_file_inf.file_name = line.substring(0, index);

		line = line.substring(index + 1);
		
		gPrinterInf.print_file_inf.file_size = line.substring(0).toInt();

		return;
		
	}
	else if(line.startsWith("M992 "))
	{
		line = line.substring(5);
		int index = line.indexOf(':');
		if(index == -1)
			return;

		line.trim();
		
		gPrinterInf.print_file_inf.print_hours = line.substring(0, index).toInt();

		line = line.substring(index + 1);
		index = line.indexOf(':');
		if(index == -1)
			return;

		line.trim();
		gPrinterInf.print_file_inf.print_mins = line.substring(0, index).toInt();

		line = line.substring(index + 1);
		gPrinterInf.print_file_inf.print_seconds = line.toInt();		
		
		return;
	}
	else if(line.startsWith("M27 "))
	{
		line = line.substring(4);
		gPrinterInf.print_file_inf.print_rate = line.toInt();
		return;
	}
	else if(line.startsWith("FIRMWARE_NAME:"))
	{
		if(line.startsWith("Robin", strlen("FIRMWARE_NAME:")))
		{
			M3_TYPE = ROBIN;
		}
		else if(line.startsWith("TFT24", strlen("FIRMWARE_NAME:")))
		{
			M3_TYPE = TFT24;
		}
		else
		{
			M3_TYPE = TFT28;
		}
		GET_VERSION_OK = true;
	}
	
	tmpStr = (int8_t *)strstr((const char *)&cmdRxBuf[i], "T:");
	if(tmpStr)
	{
		memset(tempBuf, 0, sizeof(tempBuf));
		k = 0;
		num_valid = 0;
		for(j = 2; tmpStr[j] != ' '; j++)
		{
			
			if(tmpStr[j] == '\0')
			{
				break;
			}
			
			tempBuf[k] = tmpStr[j];
			num_valid = 1;
			k++;
		}
		if(num_valid)
		{
			if(DecStr2Float(tempBuf, &tmpTemp) !=   0)//��ǰ�¶�
			{
				//if((int)tmpTemp != 0)
				{
					
					tmpStr_e = (int8_t *)strstr((const char *)&cmdRxBuf[i], "E:");
					if(tmpStr_e)
					{
						if(*(tmpStr_e+2) =='0')
						{
							gPrinterInf.curSprayerTemp[0] = tmpTemp;
							
						}
						else if(*(tmpStr_e+2) =='1')
						{
							gPrinterInf.curSprayerTemp[1] = tmpTemp;																	
						}
					}
					else
					{
						gPrinterInf.curSprayerTemp[0] = tmpTemp;						
					}
				
					
					temp_update_flag = 1;
				}
			}
			if(tmpStr[j + 1] == '/')
			{
				j += 2;
				memset(tempBuf, 0, sizeof(tempBuf));
				k = 0;
				num_valid = 0;
				for(; tmpStr[j] != ' '; j++)
				{
					
					if(tmpStr[j] == '\0')
					{
						break;
					}
					tempBuf[k] = tmpStr[j];
					num_valid = 1;
					k++;
					
				}
				if(num_valid)
				{
					if(DecStr2Float(tempBuf, &tmpTemp)	 !=   0)//Ŀ���¶�
					{
						//if((int)tmpTemp != 0)
						{
							tmpStr_e = (int8_t *)strstr((const char *)&cmdRxBuf[i], "E:");
							if(tmpStr_e)
							{
								if(*(tmpStr_e+2) =='0')
								{
									gPrinterInf.desireSprayerTemp[0] = tmpTemp;
									
									temp_update_flag = 1;	
								}
								else	if(*(tmpStr_e+2) =='1')
								{
									gPrinterInf.desireSprayerTemp[1] = tmpTemp;
									
									temp_update_flag = 1;	
								}
							}
							else
							{
									gPrinterInf.desireSprayerTemp[0] = tmpTemp;								
									temp_update_flag = 1;	
							}
							
						}
					}
				}
			}
		}
	}		


	tmpStr = (int8_t *)strstr((const char *)&cmdRxBuf[i], "B:");
	if(tmpStr)
	{
			
			memset(tempBuf, 0, sizeof(tempBuf));
			k = 0;
			num_valid = 0;
			for(j = 2; tmpStr[j] != ' '; j++)
			{
				
				if(tmpStr[j] == '\0')
				{
					break;
				}
				tempBuf[k] = tmpStr[j];
				num_valid = 1;
				k++;
				
			}
			if(num_valid)
			{
				if(DecStr2Float(tempBuf, &tmpTemp)	 != 	0)
				{
				//	if((int)tmpTemp != 0)
					{
						gPrinterInf.curBedTemp = tmpTemp;
					
						temp_update_flag = 1; 						
					}
	
				}
				if(tmpStr[j + 1] == '/')
				{
					j += 2;
					memset(tempBuf, 0, sizeof(tempBuf));
					k = 0;
					num_valid = 0;
					for(; tmpStr[j] != ' '; j++)
					{
						
						if(tmpStr[j] == '\0')
						{ 							
							break;
						}
						tempBuf[k] = tmpStr[j];
						num_valid = 1;
						k++;
							
					}
					if(num_valid)
					{
						if(DecStr2Float(tempBuf, &tmpTemp)	!=	 0)
						{
						//	if((int)tmpTemp != 0)
							{
								gPrinterInf.desireBedTemp = tmpTemp;
																
								temp_update_flag = 1; 									
							}
						}
					}
				}
			}
		}
	
		tmpStr = (int8_t *)strstr((const char *)&cmdRxBuf[i], "T0:");
		if( tmpStr)
		{
			
			memset(tempBuf, 0, sizeof(tempBuf));
			k = 0;
			num_valid = 0;
			for(j = 3; tmpStr[j] != ' '; j++)
			{
				
				if(tmpStr[j] == '\0')
				{
					break;
				}
				
				tempBuf[k] = tmpStr[j];
				num_valid = 1;
				k++;
				
			}
			if(num_valid)
			{
				if(DecStr2Float(tempBuf, &tmpTemp)	 != 	0)
				{
					if((int)tmpTemp != 0)
					{
						gPrinterInf.curSprayerTemp[0] = tmpTemp;
						
						temp_update_flag = 1;
					}
				}
				if(tmpStr[j + 1] == '/')
				{
					j += 2;
					memset(tempBuf, 0, sizeof(tempBuf));
					k = 0;
					num_valid = 0;
					for(; tmpStr[j] != ' '; j++)
					{
						
						if(tmpStr[j] == '\0')
						{
							break;
						}
						tempBuf[k] = tmpStr[j];
						num_valid = 1;
						k++;
						
					}
					if(num_valid)
					{
						if(DecStr2Float(tempBuf, &tmpTemp)	 != 	0)
						{
							if((int)tmpTemp != 0)
							{
								gPrinterInf.desireSprayerTemp[0] = tmpTemp;
																
								temp_update_flag = 1;
							}
	
						}
					}
				}
			}
		}
		
		tmpStr = (int8_t *)strstr((const char *)&cmdRxBuf[i], "T1:");
		if( tmpStr)
		{

			memset(tempBuf, 0, sizeof(tempBuf));
			k = 0;
			num_valid = 0;
			for(j = 3; tmpStr[j] != ' '; j++)
			{
				
				if(tmpStr[j] == '\0')
				{
					break;
				}
				
				tempBuf[k] = tmpStr[j];
				num_valid = 1;
				k++;
				
			}
			if(num_valid)
			{
				if(DecStr2Float(tempBuf, &tmpTemp)	 !=   0)
				{
					if((int)tmpTemp != 0)
					{
						gPrinterInf.curSprayerTemp[1] = tmpTemp;
					
						temp_update_flag = 1;
					}
				}
				if(tmpStr[j + 1] == '/')
				{
					j += 2;
					memset(tempBuf, 0, sizeof(tempBuf));
					k = 0;
					num_valid = 0;
					for(; tmpStr[j] != ' '; j++)
					{
						
						if(tmpStr[j] == '\0')
						{
							break;
						}
						tempBuf[k] = tmpStr[j];
						num_valid = 1;
						k++;
						
					}
					if(num_valid)
					{
						if(DecStr2Float(tempBuf, &tmpTemp)	 !=   0)
						{
							if((int)tmpTemp != 0)
							{
								gPrinterInf.desireSprayerTemp[1] = tmpTemp;
																		
								temp_update_flag = 1;
							}

						}
					}
				}
			}
		}

		
			
	
}
