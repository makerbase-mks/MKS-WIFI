//#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
//#include <DNSServer.h>
#include "RepRapWebServer.h"
#include "MksHTTPUpdateServer.h"
#include <EEPROM.h>
#include <FS.h>
#include <ESP8266HTTPClient.h>
//#include <ESP8266SSDP.h>
#include "PooledStrings.cpp"
#include <WiFiUdp.h>
//#include "SPITransaction.h"
#include "Config.h"
#include "gcode.h"
//#include "tcp_handler.h";
//#include "MksWifi.h"

extern "C" {
#include "user_interface.h"     // for struct rst_info
}

char* firmwareVersion = "C1.0.4_201109_beta";



char M3_TYPE = TFT28;


boolean GET_VERSION_OK = false;


#ifdef SHOW_PASSWORDS
# define PASSWORD_INPUT_TYPE  "\"text\""
#else
# define PASSWORD_INPUT_TYPE  "\"password\""
#endif

char wifi_mode[] = {"wifi_mode_sta"};
char moduleId[21] = {0};

char softApName[32] = "MKSWIFI";
char softApKey[64] = {0};

uint8_t manual_valid = 0xff; //whether it use static ip
uint32_t ip_static, subnet_static, gateway_staic, dns_static;

#define BUTTON_PIN -1
#define MAX_WIFI_FAIL 50
#define MAX_LOGGED_IN_CLIENTS 3


#define BAK_ADDRESS_WIFI_SSID			0
#define BAK_ADDRESS_WIFI_KEY			(BAK_ADDRESS_WIFI_SSID + 32)
#define BAK_ADDRESS_WEB_HOST			(BAK_ADDRESS_WIFI_KEY+64)
#define BAK_ADDRESS_WIFI_MODE		(BAK_ADDRESS_WEB_HOST+64)
#define BAK_ADDRESS_WIFI_VALID		(BAK_ADDRESS_WIFI_MODE + 16)
#define BAK_ADDRESS_MODULE_ID		(BAK_ADDRESS_WIFI_VALID + 16)
#define BAK_ADDRESS_RESERVE1		(BAK_ADDRESS_MODULE_ID + 32)
#define BAK_ADDRESS_RESERVE2		(BAK_ADDRESS_RESERVE1 + 16)
#define BAK_ADDRESS_RESERVE3		(BAK_ADDRESS_RESERVE2 + 96)
#define BAK_ADDRESS_RESERVE4		(BAK_ADDRESS_RESERVE3 + 16)
#define BAK_ADDRESS_MANUAL_IP_FLAG	(BAK_ADDRESS_RESERVE4 + 1)
#define BAK_ADDRESS_MANUAL_IP			(BAK_ADDRESS_MANUAL_IP_FLAG + 1)
#define BAK_ADDRESS_MANUAL_MASK		(BAK_ADDRESS_MANUAL_IP + 4)
#define BAK_ADDRESS_MANUAL_GATEWAY	(BAK_ADDRESS_MANUAL_MASK + 4)
#define BAK_ADDRESS_MANUAL_DNS		(BAK_ADDRESS_MANUAL_GATEWAY + 4)

#define LIST_MIN_LEN_SAVE_FILE	100
#define LIST_MAX_LEN_SAVE_FILE  (1024 * 100)

char ssid[32], pass[64], webhostname[64];
//IPAddress sessions[MAX_LOGGED_IN_CLIENTS];
uint8_t loggedInClientsNum = 0;
//MDNSResponder mdns;
#define FILE_DOWNLOAD_PORT	11188

MksHTTPUpdateServer httpUpdater;

char cloud_host[96] = "baizhongyun.cn";
int cloud_port = 12345;
boolean cloud_enable_flag = false;
int cloud_link_state = 0; // 0:??¨®?¡ê?1:¨º1?¨¹¡ê??¡ä¨¢??¨®¡ê?2:¨°?¨¢??¨®¡ê??¡ä¡ã¨®?¡§¡ê?3:¨°?¡ã¨®?¡§

RepRapWebServer server(80);

WiFiServer tcp(8080);
WiFiClient cloud_client;
//DNSServer dns;
String wifiConfigHtml;

volatile bool verification_flag = false;


IPAddress apIP(192, 168, 4, 1);

#define MAX_SRV_CLIENTS 	1

char filePath[100];

#define QUEUE_MAX_NUM	 10

struct QUEUE
{
	char buf[QUEUE_MAX_NUM][100];
	int rd_index;
	int wt_index;
} ;

struct QUEUE cmd_queue;


char cmd_fifo[100] = {0};
int cmd_index = 0;


#define UDP_PORT	8989


WiFiUDP node_monitor;

#define TCP_FRAG_LEN	1400

WiFiClient serverClients[MAX_SRV_CLIENTS];

String monitor_tx_buf = "";
String monitor_rx_buf = "";

char uart_send_package[1024];
uint32_t uart_send_size;


char uart_send_package_important[1024]; //for the message that cannot missed
uint32_t uart_send_length_important;


char jsBuffer[1024];

char cloud_file_id[40];
char cloud_user_id[40];
char cloud_file_url[96];

char unbind_exec = 0;

bool upload_error = false;
bool upload_success = false;

uint32_t lastBeatTick = 0;
uint32_t lastStaticIpInfTick = 0;

unsigned long socket_busy_stamp = 0;

int package_file_first(char *fileName);
int package_file_fragment(uint8_t *dataField, uint32_t fragLen, int32_t fragment);

void esp_data_parser(char *cmdRxBuf, int len);
String fileUrlEncode(String str);
String fileUrlEncode(char *array);

void cloud_handler();
void cloud_get_file_list();


typedef enum
{
	TRANSFER_IDLE,
	TRANSFER_BEGIN,	//?a篓垄???篓篓Y隆盲拢陇?篓垄篓掳??隆茫娄脤?D-篓掳篓娄(D篓篓篓掳a?篓庐篓潞?M110)
	TRANSFER_GET_FILE, //?篓垄篓篓????t篓潞y?Y隆锚?篓庐D篓潞y?Y篓陇-??ready篓掳y??
	TRANSFER_READY,	//娄脤篓篓隆盲ystm32篓庐|隆盲e篓掳y??D?o?
	TRANSFER_FRAGMENT
	
} TRANS_STATE;

typedef enum
{
	CLOUD_NOT_CONNECT,
	CLOUD_IDLE,
	CLOUD_DOWNLOADING,
	CLOUD_DOWN_WAIT_M3,
	CLOUD_DOWNLOAD_FINISH,
	CLOUD_WAIT_PRINT,
	CLOUD_PRINTING,
	CLOUD_GET_FILE,
} CLOUD_STATE;

CLOUD_STATE cloud_state = CLOUD_NOT_CONNECT;


TRANS_STATE transfer_state = TRANSFER_IDLE;
int file_fragment = 0;



File dataFile;
int transfer_frags = 0;

char uart_rcv_package[1024];
int uart_rcv_index = 0;

boolean printFinishFlag = false;


boolean transfer_file_flag = false;

boolean rcv_end_flag = false;

uint8_t dbgStr[100] ;



enum class OperatingState
{
    Unknown = 0,
    Client = 1,
    AccessPoint = 2    
};

OperatingState currentState = OperatingState::Unknown;

ADC_MODE(ADC_VCC);          // need this for the ESP.getVcc() call to work

void fsHandler();
void handleGcode();
void handleRrUpload();
void  handleUpload();

void urldecode(String &input);
void urlencode(String &input);
void StartAccessPoint();
void SendInfoToSam();
bool TryToConnect();
void onWifiConfig();

void cloud_down_file(const char *url);


#define FILE_FIFO_SIZE	(4096)
#define BUF_INC_POINTER(p)	((p + 1 == FILE_FIFO_SIZE) ? 0:(p + 1))

int NET_INF_UPLOAD_CYCLE = 10000;
 class FILE_FIFO
{
  public:  
	int push(char *buf, int len)
	{
		int i = 0;
		while(i < len )
		{
			if(rP != BUF_INC_POINTER(wP))
			{
				fifo[wP] = *(buf + i) ;

				wP = BUF_INC_POINTER(wP);

				i++;
			}
			else
			{
				break;
			}
			
		}
		return i;
	}
	
	int pop(char * buf, int len)
	{	
		int i = 0;
		
		while(i < len)
		{
			if(rP != wP)
			{
				buf[i] = fifo[rP];
				rP= BUF_INC_POINTER(rP);
				i++;				
			}
			else
			{
				break;
			}
		}
		return i;
		
	}
	
	void reset()
	{		
		wP = 0;	
		rP = 0;
		memset(fifo, 0, FILE_FIFO_SIZE);
	}

	uint32_t left()
	{		
		if(rP >  wP)
			return rP - wP - 1;
		else
			return FILE_FIFO_SIZE + rP - wP - 1;
			
	}
	
	boolean is_empty()
	{
		if(rP == wP)
			return true;
		else
			return false;
	}

private:
	char fifo[FILE_FIFO_SIZE];		//
	uint32_t wP;	
	uint32_t rP;

};

class FILE_FIFO gFileFifo; //?-??隆卤????篓娄鈧??篓C??????隆茫??????fifo




void init_queue(struct QUEUE *h_queue)
{
	if(h_queue == 0)
		return;
	
	h_queue->rd_index = 0;
	h_queue->wt_index = 0;
	memset(h_queue->buf, 0, sizeof(h_queue->buf));
}

int push_queue(struct QUEUE *h_queue, char *data_to_push, int data_len)
{
	if(h_queue == 0)
		return -1;

	if(data_len > sizeof(h_queue->buf[h_queue->wt_index]))
		return -1;

	if((h_queue->wt_index + 1) % QUEUE_MAX_NUM == h_queue->rd_index)
		return -1;

	memset(h_queue->buf[h_queue->wt_index], 0, sizeof(h_queue->buf[h_queue->wt_index]));
	memcpy(h_queue->buf[h_queue->wt_index], data_to_push, data_len);

	h_queue->wt_index = (h_queue->wt_index + 1) % QUEUE_MAX_NUM;
	
	return 0;
}

int pop_queue(struct QUEUE *h_queue, char *data_for_pop, int data_len)
{
	if(h_queue == 0)
		return -1;

	if(data_len < strlen(h_queue->buf[h_queue->rd_index]))
		return -1;

	if(h_queue->rd_index == h_queue->wt_index)
		return -1;

	memset(data_for_pop, 0, data_len);
	memcpy(data_for_pop, h_queue->buf[h_queue->rd_index], strlen(h_queue->buf[h_queue->rd_index]));

	h_queue->rd_index = (h_queue->rd_index + 1) % QUEUE_MAX_NUM;
	
	return 0;
}
bool smartConfig()
{
	WiFi.mode(WIFI_STA);
	WiFi.beginSmartConfig();


	//Serial.println("config smart");
	int now = millis();
	while (1)
	{
		if(get_printer_reply() > 0)
		{
			esp_data_parser((char *)uart_rcv_package, uart_rcv_index);
		}
		uart_rcv_index = 0;
		
		delay(1000);
		if (WiFi.smartConfigDone())
		{
			
			

			WiFi.stopSmartConfig();
			//Serial.println("config smart ok");
			return true;;
		}

		if(millis() - now > 120000) // 2min
		{
			WiFi.stopSmartConfig();
			//Serial.println("config smart fail");
			return false;
		}
	}
}


void net_env_prepare()
{
	if(currentState == OperatingState::Client)
	{	
	/*	if (mdns.begin(webhostname, WiFi.localIP()))
		{
			MDNS.addService("http", "tcp", 80);
		}*/
	}
	else
	{
	//	if (mdns.begin(webhostname, WiFi.softAPIP()))
		{
		//	MDNS.addService("http", "tcp", 80);
		}
	}
	/*SSDP.setSchemaURL("description.xml");
	SSDP.setHTTPPort(80);
	SSDP.setName(webhostname);
	SSDP.setSerialNumber(WiFi.macAddress());
	SSDP.setURL("mks.htm");
	SSDP.begin();*/

	if(verification_flag)
	{
		SPIFFS.begin();
		server.onNotFound(fsHandler);
	}
	
	
	server.servePrinter(true);

	
	
#if 0
	
	//self defined
	server.on("/config", HTTP_ANY, handleConfig, NULL);

	

	//smoothie style
	/*server.onPrefix("/command", HTTP_ANY, handleGcode, handleRrUpload);	
	server.on("/description.xml", HTTP_GET, [](){SSDP.schema(server.client());});*/

	
	{
		

		
		//octoprint style
		server.on("/api/printer", HTTP_GET, handleApiPrinter, NULL); //get printer state
		server.on("/api/printer/printhead", HTTP_POST, handleApiPrinterCtrl, NULL); //move  or home
		server.on("/api/printer/tool", HTTP_POST, handleApiToolRelative, NULL); //tools(extructer) relative
		server.on("/api/printer/bed", HTTP_POST, handleApiBedRelative, NULL); //bed relative
		server.on("/api/printer/command", HTTP_POST, handleApiSendCmd, NULL); //send any command
		server.onPrefix("/api/files", HTTP_POST, handleApiChooseFileToPrint, NULL); //choose file to print
		server.onPrefix("/api/files", HTTP_GET, handleApiFileList, NULL); //get file list
		server.on("/api/job", HTTP_POST, handleApiPrint, NULL); //pause or resume or stop print
		server.on("/api/job", HTTP_GET, handleApiPrintInf, NULL); //get print information
		server.on("/api/logs", HTTP_GET, handleApiLogs, NULL); //get print information

	}
	

	
	#endif
	
	
	onWifiConfig();

	server.onPrefix("/upload", HTTP_ANY, handleUpload, handleRrUpload);		
	

	server.begin();
	tcp.begin();

	
	
  	node_monitor.begin(UDP_PORT);

	
}

void reply_search_handler()
{
	char packetBuffer[200];
	 int packetSize = node_monitor.parsePacket();
	 char  ReplyBuffer[50] = "mkswifi:";
	 
	 
	  if (packetSize)
	  {
	    //Serial.print("Received packet of size ");
	    //Serial.println(packetSize);
	    //Serial.print("From ");
	    IPAddress remote = node_monitor.remoteIP();
	    for (int i = 0; i < 4; i++)
	    {
	      //Serial.print(remote[i], DEC);
	      if (i < 3)
	      {
	        //Serial.print(".");
	      }
	    }
	    //Serial.print(", port ");
	    //Serial.println(node_monitor.remotePort());

	    // read the packet into packetBufffer
	    node_monitor.read(packetBuffer, sizeof(packetBuffer));
	    //Serial.println("Contents:");
	    //Serial.println(packetBuffer);

		if(strstr(packetBuffer, "mkswifi"))
		{
			memcpy(&ReplyBuffer[strlen("mkswifi:")], moduleId, strlen(moduleId)); 
			ReplyBuffer[strlen("mkswifi:") + strlen(moduleId)] = ',';
			if(currentState == OperatingState::Client)
			{
				strcpy(&ReplyBuffer[strlen("mkswifi:") + strlen(moduleId) + 1], WiFi.localIP().toString().c_str()); 
				ReplyBuffer[strlen("mkswifi:") + strlen(moduleId) + strlen(WiFi.localIP().toString().c_str()) + 1] = '\n';
			} 
			else if(currentState == OperatingState::AccessPoint)
			{
				strcpy(&ReplyBuffer[strlen("mkswifi:") + strlen(moduleId) + 1], WiFi.softAPIP().toString().c_str()); 
				ReplyBuffer[strlen("mkswifi:") + strlen(moduleId) + strlen(WiFi.softAPIP().toString().c_str()) + 1] = '\n';
			}
			

	    // send a reply, to the IP address and port that sent us the packet we received
		    node_monitor.beginPacket(node_monitor.remoteIP(), node_monitor.remotePort());
		    node_monitor.write(ReplyBuffer, strlen(ReplyBuffer));
		    node_monitor.endPacket();
		}
	  }
}

void verification()
{
	//Todo
	verification_flag = true;
	
}

void var_init()
{
	gPrinterInf.curBedTemp = 0.0;
	gPrinterInf.curSprayerTemp[0] = 0.0;
	gPrinterInf.curSprayerTemp[1] = 0.0;
	gPrinterInf.desireSprayerTemp[0] = 0.0;
	gPrinterInf.desireSprayerTemp[1] = 0.0;
	gPrinterInf.print_state = PRINTER_NOT_CONNECT;
	
}



void setup() {

	var_init();

	Serial.begin(115200);
//	 Serial.begin(1958400);
//	 Serial.begin(4500000);
//	Serial.begin(1958400);
	delay(20);
	EEPROM.begin(512);	

	

	//Serial.println("verification");
	verification();
	
	
	//Serial.println("WIFI START now");
	String macStr= WiFi.macAddress();
	//				Serial.println(macStr);
	
	pinMode(McuTfrReadyPin, INPUT);
	pinMode(EspReqTransferPin, OUTPUT);

	digitalWrite(EspReqTransferPin, HIGH);

	bool success = TryToConnect();
	if (success)
	{
	///	cloud_down_file("/upload/file/gcode/menson/2017-03-20/dgbt.gcode");
		
	//	package_net_para();
		
	//	Serial.write(uart_send_package, uart_send_size);

		
	//	net_env_prepare();   


	}
	/*else if (smartConfig())
	{
		
		while (WiFi.status() != WL_CONNECTED)
		{
			delay(100);
		}
		//Serial.printf("smart:ssid,%s, key:%s\n", WiFi.SSID().c_str(), WiFi.psk().c_str());
		memcpy(ssid, WiFi.SSID().c_str(), strlen(WiFi.SSID().c_str()) + 1);
		memcpy(pass, WiFi.psk().c_str(), strlen(WiFi.psk().c_str()) + 1);

		EEPROM.put(0, ssid);
		EEPROM.put(32, pass);
		EEPROM.put(32+64+64, wifi_mode);
		EEPROM.commit();

	
		Serial.write(uart_send_package, sizeof(uart_send_package));
		
		net_env_prepare();  

		
	}*/
	else
	{
		
		StartAccessPoint();		
		
		currentState = OperatingState::AccessPoint;

	//	package_net_para();
		
	//	Serial.write(uart_send_package, uart_send_size);

		
	}

	

	package_net_para();
		
	Serial.write(uart_send_package, uart_send_size);
	
	net_env_prepare();   
	
	httpUpdater.setup(&server);
	delay(500);
/*
	 treeFile = SPIFFS.open("/tree.txt", "w");
	if(treeFile)
	{
		file_list_flag = true;
		Serial.println("open ok");
	}
	else
	{
		Serial.println("open failed");
	}
	*/
	/*EEPROM.put(0, "00000000");
	EEPROM.put(32, "00000000");
	EEPROM.commit();*/

}

//uint8_t uart_rcv_buf[200];
//uint32_t rcv_ptr = 0;


void net_print(const uint8_t *sbuf, uint32_t len)
{
	int i;
	
	for(i = 0; i < MAX_SRV_CLIENTS; i++){
			
	  if (serverClients[i] && serverClients[i].connected()){
		serverClients[i].write(sbuf, len);
		delay(1);
		
	  }
	}
}

void query_printer_inf()
{
	static int last_query_temp_time = 0;
	static int last_query_file_time = 0;

	if((!transfer_file_flag) &&  (transfer_state == TRANSFER_IDLE))
	{		
		
		if((gPrinterInf.print_state == PRINTER_PRINTING) || (gPrinterInf.print_state == PRINTER_PAUSE))
		//if(!printFinishFlag)
		{
			if(millis() - last_query_temp_time > 5000) //every 5 seconds
			{
				if(GET_VERSION_OK)
					package_gcode("M27\nM992\nM994\nM991\nM997\n", false);
				else
					package_gcode("M27\nM992\nM994\nM991\nM997\nM115\n", false);
				
				/*transfer_state = TRANSFER_READY;
				digitalWrite(EspReqTransferPin, LOW);*/

				last_query_temp_time = millis();
			}
		}
		else
		{
			if(millis() - last_query_temp_time > 5000) //every 5 seconds
			{
				
				if(GET_VERSION_OK)
					//package_gcode("M27\nM997\n");
					package_gcode("M991\nM27\nM997\n", false);
				else
					//package_gcode("M27\nM997\nM115\n");
					package_gcode("M991\nM27\nM997\nM115\n", false);
				
				/*transfer_state = TRANSFER_READY;
				digitalWrite(EspReqTransferPin, LOW);*/

				last_query_temp_time = millis();
			}
			
		}
	}
	if((!transfer_file_flag) &&  (transfer_state == TRANSFER_IDLE))
	{

		//beat package
		if(millis() - lastBeatTick > NET_INF_UPLOAD_CYCLE)
		{
			package_net_para();
			/*transfer_state = TRANSFER_READY;
			digitalWrite(EspReqTransferPin, LOW);*/
			lastBeatTick = millis();
		}
	
	}
	if((manual_valid == 0xa) && (!transfer_file_flag) &&  (transfer_state == TRANSFER_IDLE))
	{

		//beat package
		if(millis() - lastStaticIpInfTick > (NET_INF_UPLOAD_CYCLE + 2))
		{
			package_static_ip_info();
			/*transfer_state = TRANSFER_READY;
			digitalWrite(EspReqTransferPin, LOW);*/
			lastStaticIpInfTick = millis();
		}
	
	}

	
}

int get_printer_reply()
{
	size_t len = Serial.available();

	if(len > 0){
		
		len = ((uart_rcv_index + len) < sizeof(uart_rcv_package)) ? len : (sizeof(uart_rcv_package) - uart_rcv_index);

				
		Serial.readBytes(&uart_rcv_package[uart_rcv_index], len);

		uart_rcv_index += len;	

		if(uart_rcv_index >= sizeof(uart_rcv_package))
		{			
			return sizeof(uart_rcv_package);
		}
		

		
	}
	return uart_rcv_index;

}

void loop()
{
	int i;

	//output_json();
	
	#if 1
	
	
	switch (currentState)
	{
		case OperatingState::Client:
			server.handleClient();
			if(verification_flag)
			{
				cloud_handler();
			}
			break;

		case OperatingState::AccessPoint:
			server.handleClient();
		//	dns.processNextRequest();
			break;

		default:
			break;
	}


	
	
	
	//	if(transfer_state == TRANSFER_IDLE)
		{
			if (tcp.hasClient()){
				for(i = 0; i < MAX_SRV_CLIENTS; i++){
				  //find free/disconnected spot
				#if 0
				  if (!serverClients[i] || !serverClients[i].connected()){
					if(serverClients[i]) serverClients[i].stop();
					serverClients[i] = tcp.available();
					continue;
				  }
				  #else
				  if(serverClients[i].connected()) 
				  {
					serverClients[i].stop();
				  }
				  #endif
				  serverClients[i] = tcp.available();
				}
				if (tcp.hasClient())
				{
					//no free/disconnected spot so reject
					WiFiClient serverClient = tcp.available();
					serverClient.stop();
					
				}
			}
			memset(dbgStr, 0, sizeof(dbgStr));
			for(i = 0; i < MAX_SRV_CLIENTS; i++)
			{
				if (serverClients[i] && serverClients[i].connected())
				{
					uint32_t readNum = serverClients[i].available();

					if(readNum > FILE_FIFO_SIZE)
					{
					//	net_print((const uint8_t *) "readNum > FILE_FIFO_SIZE\n");
						serverClients[i].flush();
					//	Serial.println("flush"); 
						continue;
					}

				
					if(readNum > 0)
					{
						char * point;
						
					  	uint8_t readStr[readNum + 1] ;

						uint32_t readSize;
						
						readSize = serverClients[i].read(readStr, readNum);
							
						readStr[readSize] = 0;
						

						
						//transfer file
						#if 0
						if(strstr((const char *)readStr, "M29") != 0)
						{
							if(!verification_flag)
							{
								break;
							}
							if(transfer_state != TRANSFER_IDLE)
							{
								rcv_end_flag = true;
								net_print((const uint8_t *) "ok\n", strlen((const char *)"ok\n"));
								break;
							}
						}
						#endif
						
					
						
						if(transfer_file_flag)
						{
						
							if(!verification_flag)
							{
								break;
							}
							if(gFileFifo.left() >= readSize)
							{
							
								gFileFifo.push((char *)readStr, readSize);
								transfer_frags += readSize;
								
								
							}
						
						}
						else
						{
							

							if(verification_flag)
							{
								int j = 0;
								char cmd_line[100] = {0};
								String gcodeM3 = "";
							
							#if 0
								if(transfer_state == TRANSFER_BEGIN)
								{
									if(strstr((const char *)readStr, "M110") != 0)
									{

										file_fragment = 0;
										rcv_end_flag = false;
										transfer_file_flag = true;

										if(package_file_first(filePath) == 0)
										{
											/*transfer_state = TRANSFER_READY;
											digitalWrite(EspReqTransferPin, LOW);*/
										}
										else
										{
											transfer_file_flag = false;
											transfer_state = TRANSFER_IDLE;
										}
										net_print((const uint8_t *) "ok\n", strlen((const char *)"ok\n"));
										break;
									}
								}

							#endif
								
								init_queue(&cmd_queue);
								
								cmd_index = 0;
								memset(cmd_fifo, 0, sizeof(cmd_fifo));
								while(j < readSize)
								{
									if((readStr[j] == '\r') || (readStr[j] == '\n'))
									{
										if((cmd_index) > 1)
										{
											cmd_fifo[cmd_index] = '\n';
											cmd_index++;

											
											push_queue(&cmd_queue, cmd_fifo, cmd_index);
										}
										memset(cmd_fifo, 0, sizeof(cmd_fifo));
										cmd_index = 0;
									}
									else if(readStr[j] == '\0')
										break;
									else
									{
										if(cmd_index >= sizeof(cmd_fifo))
										{
											memset(cmd_fifo, 0, sizeof(cmd_fifo));
											cmd_index = 0;
										}
										cmd_fifo[cmd_index] = readStr[j];
										cmd_index++;
									}

									j++;

									do_transfer();
									yield();
								
								}
								while(pop_queue(&cmd_queue, cmd_line, sizeof(cmd_line)) >= 0)		
								{
								#if 0
									point = strstr((const char *)cmd_line, "M28 ");
									if(point != 0)						
									{
										if((strstr((const char *)cmd_line, ".g") || strstr((const char *)cmd_line, ".G")))
										{
											int index = 0;
											char *fileName;
											
											point += 3;
											while(*(point + index) == ' ')
												index++;
											
											memcpy((char *)filePath, (const char *)(point + index), readSize - (int)(point + index - (int)(&cmd_line[0])));
											
											gFileFifo.reset();

											transfer_frags = 0;

																		
											transfer_state = TRANSFER_BEGIN;

											sprintf((char *)dbgStr, "Writing to file:%s\n", (char *)filePath);
											
											net_print((const uint8_t *)dbgStr, strlen((const char *)dbgStr));
										}
										
									}
									else
								#endif
									{
										/*transfer gcode*/
										//Serial.write(cmd_line, readNum);
										if((strchr((const char *)cmd_line, 'G') != 0) 
											|| (strchr((const char *)cmd_line, 'M') != 0)
											|| (strchr((const char *)cmd_line, 'T') != 0))
										{
											if(strchr((const char *)cmd_line, '\n') != 0 )
											{
												String gcode((const char *)cmd_line);

											//	sprintf((char *)dbgStr, "read %d: %s\n", readNum, cmd_line);

										//		net_print((const uint8_t *)dbgStr, strlen((char *)dbgStr));

												if(gcode.startsWith("M998") && (M3_TYPE == ROBIN))
												{
													net_print((const uint8_t *) "ok\r\n", strlen((const char *)"ok\r\n"));
												}
												else if(gcode.startsWith("M997"))
												{
													if(gPrinterInf.print_state == PRINTER_IDLE)
													//	net_print((const uint8_t *) "M997 IDLE\r\n", strlen((const char *)"M997 IDLE\r\n"));
														strcpy((char *)dbgStr, "M997 IDLE\r\n");
													else if(gPrinterInf.print_state == PRINTER_PRINTING)
													//	net_print((const uint8_t *) "M997 PRINTING\r\n", strlen((const char *)"M997 PRINTING\r\n"));
														strcpy((char *)dbgStr, "M997 PRINTING\r\n");
													else if(gPrinterInf.print_state == PRINTER_PAUSE)
														//net_print((const uint8_t *) "M997 PAUSE\r\n", strlen((const char *)"M997 PAUSE\r\n"));
														strcpy((char *)dbgStr, "M997 PAUSE\r\n");
													else
														strcpy((char *)dbgStr, "M997 NOT CONNECTED\r\n");
												//	net_print((const uint8_t *) "ok\r\n", strlen((const char *)"ok\r\n"));
												}
												else if(gcode.startsWith("M27"))
												{
													memset(dbgStr, 0, sizeof(dbgStr));
													sprintf((char *)dbgStr, "M27 %d\r\n", gPrinterInf.print_file_inf.print_rate);
												//	net_print((const uint8_t *) dbgStr, strlen((const char *)dbgStr));
												//	net_print((const uint8_t *) "ok\r\n", strlen((const char *)"ok\r\n"));
												}
												else if(gcode.startsWith("M992"))
												{
													memset(dbgStr, 0, sizeof(dbgStr));
													sprintf((char *)dbgStr, "M992 %02d:%02d:%02d\r\n", 
														gPrinterInf.print_file_inf.print_hours, gPrinterInf.print_file_inf.print_mins, gPrinterInf.print_file_inf.print_seconds);
												//	net_print((const uint8_t *) dbgStr, strlen((const char *)dbgStr));
												//	net_print((const uint8_t *) "ok\r\n", strlen((const char *)"ok\r\n"));
												}
												else if(gcode.startsWith("M994"))
												{
													memset(dbgStr, 0, sizeof(dbgStr));
													sprintf((char *)dbgStr, "M994 %s;%d\r\n", 
														gPrinterInf.print_file_inf.file_name.c_str(), gPrinterInf.print_file_inf.file_size);														
												//	net_print((const uint8_t *) dbgStr, strlen((const char *)dbgStr));
												//	net_print((const uint8_t *) "ok\r\n", strlen((const char *)"ok\r\n"));
												}
												else  if(gcode.startsWith("M115"))
												{
													memset(dbgStr, 0, sizeof(dbgStr));
													if(M3_TYPE == ROBIN)
														strcpy((char *)dbgStr, "FIRMWARE_NAME:Robin\r\n");
													else if(M3_TYPE == TFT28)
														strcpy((char *)dbgStr, "FIRMWARE_NAME:TFT28/32\r\n");
													else if(M3_TYPE == TFT24)
														strcpy((char *)dbgStr, "FIRMWARE_NAME:TFT24\r\n");
													
													
												//	net_print((const uint8_t *) dbgStr, strlen((const char *)dbgStr));
												//	net_print((const uint8_t *) "ok\r\n", strlen((const char *)"ok\r\n"));
												}
												/*else if(gcode.startsWith("M105"))
												{
													memset(dbgStr, 0, sizeof(dbgStr));
													sprintf((char *)dbgStr, "T:%d /%d B:%d /%d T0:%d /%d T1:%d /%d @:0 B@:0\r\n", 
														(int)gPrinterInf.curSprayerTemp[0], (int)gPrinterInf.desireSprayerTemp[0], (int)gPrinterInf.curBedTemp, (int)gPrinterInf.desireBedTemp,
														(int)gPrinterInf.curSprayerTemp[0], (int)gPrinterInf.desireSprayerTemp[0], (int)gPrinterInf.curSprayerTemp[1], (int)gPrinterInf.desireSprayerTemp[1]);
														
												//	net_print((const uint8_t *) dbgStr, strlen((const char *)dbgStr));
												//	net_print((const uint8_t *) "ok\r\n", strlen((const char *)"ok\r\n"));
												}*/
												else
												{	
													if(gPrinterInf.print_state == PRINTER_IDLE)
													{
														if(gcode.startsWith("M23") || gcode.startsWith("M24"))
														 {
														 	gPrinterInf.print_state = PRINTER_PRINTING;
															gPrinterInf.print_file_inf.file_name = "";
															gPrinterInf.print_file_inf.file_size = 0;
															gPrinterInf.print_file_inf.print_rate = 0;
															gPrinterInf.print_file_inf.print_hours = 0;
															gPrinterInf.print_file_inf.print_mins = 0;
															gPrinterInf.print_file_inf.print_seconds = 0;

															printFinishFlag = false;
														 }
													}
													gcodeM3.concat(gcode);
													
												}
												
											}
										}
									}
									if(strlen((const char *)dbgStr) > 0)
									{
										net_print((const uint8_t *) "ok\r\n", strlen((const char *)"ok\r\n"));
									
										net_print((const uint8_t *) dbgStr, strlen((const char *)dbgStr));		
										memset(dbgStr, 0, sizeof(dbgStr));			
										
									}
										

								
									do_transfer();
									yield();
								
									
									
								}
								
								if(gcodeM3.length() > 2)
								{
									package_gcode(gcodeM3, true);
									//Serial.write(uart_send_package, sizeof(uart_send_package));
									/*transfer_state = TRANSFER_READY;
									digitalWrite(EspReqTransferPin, LOW);*/
									do_transfer();

									socket_busy_stamp = millis();
								}
								
								
							}
						}
					
					}
				}
			}
		/*	if(strlen((const char *)dbgStr) > 0)
			{
				net_print((const uint8_t *) dbgStr, strlen((const char *)dbgStr));
				net_print((const uint8_t *) "ok\r\n", strlen((const char *)"ok\r\n"));
			}*/
		}
		//sprintf((char *)dbgStr, "state:%d\n", transfer_state);
		//net_print((const uint8_t *)dbgStr);


			
			do_transfer();
		
			

			if(get_printer_reply() > 0)
			{
				esp_data_parser((char *)uart_rcv_package, uart_rcv_index);
			}

			uart_rcv_index = 0;

		if(verification_flag)
		{
			query_printer_inf();	
			if(millis() - socket_busy_stamp > 5000)
			{				
				reply_search_handler();
			}
			cloud_down_file();
			cloud_get_file_list();
		}
		else
		{
			verification();
		}
	
	yield();
	#endif

}

#define FILE_BLOCK_SIZE	(1024 - 5 - 4)


#define UART_PROTCL_HEAD_OFFSET		0
#define UART_PROTCL_TYPE_OFFSET		1
#define UART_PROTCL_DATALEN_OFFSET	2
#define UART_PROTCL_DATA_OFFSET		4

#define UART_PROTCL_HEAD	(char)0xa5
#define UART_PROTCL_TAIL	(char)0xfc

#define UART_PROTCL_TYPE_NET			(char)0x0
#define UART_PROTCL_TYPE_GCODE		(char)0x1
#define UART_PROTCL_TYPE_FIRST			(char)0x2
#define UART_PROTCL_TYPE_FRAGMENT		(char)0x3
#define UART_PROTCL_TYPE_HOT_PORT		(char)0x4
#define UART_PROTCL_TYPE_STATIC_IP		(char)0x5

int package_net_para()
{
	int dataLen;
	int wifi_name_len;
	int wifi_key_len;
	
	memset(uart_send_package, 0, sizeof(uart_send_package));
	uart_send_package[UART_PROTCL_HEAD_OFFSET] = UART_PROTCL_HEAD;
	uart_send_package[UART_PROTCL_TYPE_OFFSET] = UART_PROTCL_TYPE_NET;

	if(currentState == OperatingState::Client)
	{
		
		if(WiFi.status() == WL_CONNECTED)
		{
			uart_send_package[UART_PROTCL_DATA_OFFSET] = WiFi.localIP()[0];
			uart_send_package[UART_PROTCL_DATA_OFFSET + 1] = WiFi.localIP()[1];
			uart_send_package[UART_PROTCL_DATA_OFFSET + 2] = WiFi.localIP()[2];
			uart_send_package[UART_PROTCL_DATA_OFFSET + 3] = WiFi.localIP()[3];
			uart_send_package[UART_PROTCL_DATA_OFFSET + 6] = 0x0a;
		}
		else
		{
			uart_send_package[UART_PROTCL_DATA_OFFSET] = 0;
			uart_send_package[UART_PROTCL_DATA_OFFSET + 1] = 0;
			uart_send_package[UART_PROTCL_DATA_OFFSET + 2] = 0;
			uart_send_package[UART_PROTCL_DATA_OFFSET + 3] = 0;
			uart_send_package[UART_PROTCL_DATA_OFFSET + 6] = 0x05;
		}

		uart_send_package[UART_PROTCL_DATA_OFFSET + 7] = 0x02;

		wifi_name_len = strlen(ssid);
		wifi_key_len = strlen(pass);

		uart_send_package[UART_PROTCL_DATA_OFFSET + 8] = wifi_name_len;

		strcpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + 9], ssid);

		uart_send_package[UART_PROTCL_DATA_OFFSET + 9 + wifi_name_len] = wifi_key_len;

		strcpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + 10 + wifi_name_len], pass);	

		
	} 
	else if(currentState == OperatingState::AccessPoint)
	{
		uart_send_package[UART_PROTCL_DATA_OFFSET] = WiFi.softAPIP()[0];
		uart_send_package[UART_PROTCL_DATA_OFFSET + 1] = WiFi.softAPIP()[1];
		uart_send_package[UART_PROTCL_DATA_OFFSET + 2] = WiFi.softAPIP()[2];
		uart_send_package[UART_PROTCL_DATA_OFFSET + 3] = WiFi.softAPIP()[3];
		
		uart_send_package[UART_PROTCL_DATA_OFFSET + 6] = 0x0a;
		uart_send_package[UART_PROTCL_DATA_OFFSET + 7] = 0x01;

		
		wifi_name_len = strlen(softApName);
		wifi_key_len = strlen(softApKey);

		uart_send_package[UART_PROTCL_DATA_OFFSET + 8] = wifi_name_len;

		strcpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + 9], softApName);

		uart_send_package[UART_PROTCL_DATA_OFFSET + 9 + wifi_name_len] = wifi_key_len;

		strcpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + 10 + wifi_name_len], softApKey);	
		
		
	}

	int host_len = strlen((const char *)cloud_host);
	
	if(cloud_enable_flag)
	{
		if(cloud_link_state == 3)
			uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + 10] =0x12;
		else if( (cloud_link_state == 1) || (cloud_link_state == 2))
			uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + 10] =0x11;
		else if(cloud_link_state == 0)
			uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + 10] =0x10;
	}
	else
	{
		uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + 10] =0x0;
	
	}

	uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + 11] = host_len;
	memcpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + 12], cloud_host, host_len);
	uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + host_len + 12] = cloud_port & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + host_len + 13] = (cloud_port >> 8 ) & 0xff;

	int id_len = strlen((const char *)moduleId);
	uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + host_len + 14]  = id_len;
	memcpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + host_len + 15], moduleId, id_len);
		
	int ver_len = strlen((const char *)firmwareVersion);
	uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + host_len + id_len + 15]  = ver_len;
	memcpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + wifi_name_len + wifi_key_len + host_len + id_len + 16], firmwareVersion, ver_len);
		
	dataLen = wifi_name_len + wifi_key_len + host_len + id_len + ver_len + 16;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 4] = 8080 & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 5] = (8080 >> 8 )& 0xff;

	if(!verification_flag)
		uart_send_package[UART_PROTCL_DATA_OFFSET + 6] = 0x0e;

	
	uart_send_package[UART_PROTCL_DATALEN_OFFSET] = dataLen & 0xff;
	uart_send_package[UART_PROTCL_DATALEN_OFFSET + 1] = dataLen >> 8;
	
	
	uart_send_package[dataLen + 4] = UART_PROTCL_TAIL;

	uart_send_size = dataLen + 5;
}

int package_static_ip_info()
{
	int dataLen;
	int wifi_name_len;
	int wifi_key_len;
	
	memset(uart_send_package, 0, sizeof(uart_send_package));
	uart_send_package[UART_PROTCL_HEAD_OFFSET] = UART_PROTCL_HEAD;
	uart_send_package[UART_PROTCL_TYPE_OFFSET] = UART_PROTCL_TYPE_STATIC_IP;

	uart_send_package[UART_PROTCL_DATA_OFFSET] = ip_static & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 1] = (ip_static >> 8) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 2] = (ip_static >> 16) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 3] = (ip_static >> 24) & 0xff;

	uart_send_package[UART_PROTCL_DATA_OFFSET + 4] = subnet_static & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 5] = (subnet_static >> 8) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 6] = (subnet_static >> 16) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 7] = (subnet_static >> 24) & 0xff;

	uart_send_package[UART_PROTCL_DATA_OFFSET + 8] = gateway_staic & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 9] = (gateway_staic >> 8) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 10] = (gateway_staic >> 16) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 11] = (gateway_staic >> 24) & 0xff;

	uart_send_package[UART_PROTCL_DATA_OFFSET + 12] = dns_static & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 13] = (dns_static >> 8) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 14] = (dns_static >> 16) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 15] = (dns_static >> 24) & 0xff;

	uart_send_package[UART_PROTCL_DATALEN_OFFSET] = 16;
	uart_send_package[UART_PROTCL_DATALEN_OFFSET + 1] = 0;	
	
	uart_send_package[UART_PROTCL_DATA_OFFSET + 16] = UART_PROTCL_TAIL;

	uart_send_size = UART_PROTCL_DATA_OFFSET + 17;
}

int package_gcode(String gcodeStr, boolean important)
{
	int dataLen;
	const char *dataField = gcodeStr.c_str();
	
	uint32_t buffer_offset;
	
	dataLen = strlen(dataField);
	
	if(dataLen > 1019)
		return -1;

	if(important)
	{	
		buffer_offset = uart_send_length_important;
	}
	else
	{		
		buffer_offset = 0;
		memset(uart_send_package, 0, sizeof(uart_send_package));
	}
	
	if(dataLen + buffer_offset > 1019)
		return -1;
	
	//net_print((const uint8_t *)"dataField:", strlen("dataField:"));
	//net_print((const uint8_t *)dataField, strlen(dataField));
	//net_print((const uint8_t *)"\n", 1);
	if(important)
	{
		uart_send_package_important[UART_PROTCL_HEAD_OFFSET + buffer_offset] = UART_PROTCL_HEAD;
		uart_send_package_important[UART_PROTCL_TYPE_OFFSET + buffer_offset] = UART_PROTCL_TYPE_GCODE;
		uart_send_package_important[UART_PROTCL_DATALEN_OFFSET + buffer_offset] = dataLen & 0xff;
		uart_send_package_important[UART_PROTCL_DATALEN_OFFSET + buffer_offset + 1] = dataLen >> 8;
		
		strncpy(&uart_send_package_important[UART_PROTCL_DATA_OFFSET + buffer_offset], dataField, dataLen);
		
		uart_send_package_important[dataLen + buffer_offset + 4] = UART_PROTCL_TAIL;

		uart_send_length_important += dataLen + 5;
	}
	else
	{	
		uart_send_package[UART_PROTCL_HEAD_OFFSET + buffer_offset] = UART_PROTCL_HEAD;
		uart_send_package[UART_PROTCL_TYPE_OFFSET + buffer_offset] = UART_PROTCL_TYPE_GCODE;
		uart_send_package[UART_PROTCL_DATALEN_OFFSET + buffer_offset] = dataLen & 0xff;
		uart_send_package[UART_PROTCL_DATALEN_OFFSET + buffer_offset + 1] = dataLen >> 8;
		
		strncpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + buffer_offset], dataField, dataLen);
		
		uart_send_package[dataLen + buffer_offset + 4] = UART_PROTCL_TAIL;

		uart_send_size = dataLen + 5;
	}

	

	if(monitor_tx_buf.length() + gcodeStr.length() < 300)
	{
		monitor_tx_buf.concat(gcodeStr);
	}
	else
	{
		//net_print((const uint8_t *)"overflow", strlen("overflow"));
	}
	

	return 0;
}

int package_gcode(char *dataField, boolean important)
{	
	uint32_t buffer_offset;
	int dataLen = strlen((const char *)dataField);

	if(important)
	{		
		
		buffer_offset = uart_send_length_important;
	}
	else
	{
		buffer_offset = 0;
		memset(uart_send_package, 0, sizeof(uart_send_package));
	}
	if(dataLen + buffer_offset > 1019)
		return -1;
	//net_print((const uint8_t *)"dataField:", strlen("dataField:"));
	//net_print((const uint8_t *)dataField, strlen(dataField));
	//net_print((const uint8_t *)"\n", 1);

	/**(buffer_to_send + UART_PROTCL_HEAD_OFFSET + buffer_offset) = UART_PROTCL_HEAD;
	*(buffer_to_send + UART_PROTCL_TYPE_OFFSET + buffer_offset) = UART_PROTCL_TYPE_GCODE;
	*(buffer_to_send + UART_PROTCL_DATALEN_OFFSET + buffer_offset) = dataLen & 0xff;
	*(buffer_to_send + UART_PROTCL_DATALEN_OFFSET + buffer_offset + 1) = dataLen >> 8;
	strncpy(buffer_to_send + UART_PROTCL_DATA_OFFSET + buffer_offset, dataField, dataLen);
	
	*(buffer_to_send + dataLen + buffer_offset + 4) = UART_PROTCL_TAIL;
*/
	
	if(important)
	{
		uart_send_package_important[UART_PROTCL_HEAD_OFFSET + buffer_offset] = UART_PROTCL_HEAD;
		uart_send_package_important[UART_PROTCL_TYPE_OFFSET + buffer_offset] = UART_PROTCL_TYPE_GCODE;
		uart_send_package_important[UART_PROTCL_DATALEN_OFFSET + buffer_offset] = dataLen & 0xff;
		uart_send_package_important[UART_PROTCL_DATALEN_OFFSET + buffer_offset + 1] = dataLen >> 8;
		
		strncpy(&uart_send_package_important[UART_PROTCL_DATA_OFFSET + buffer_offset], dataField, dataLen);
		
		uart_send_package_important[dataLen + buffer_offset + 4] = UART_PROTCL_TAIL;

		uart_send_length_important += dataLen + 5;
	}
	else
	{	
		uart_send_package[UART_PROTCL_HEAD_OFFSET + buffer_offset] = UART_PROTCL_HEAD;
		uart_send_package[UART_PROTCL_TYPE_OFFSET + buffer_offset] = UART_PROTCL_TYPE_GCODE;
		uart_send_package[UART_PROTCL_DATALEN_OFFSET + buffer_offset] = dataLen & 0xff;
		uart_send_package[UART_PROTCL_DATALEN_OFFSET + buffer_offset + 1] = dataLen >> 8;
		
		strncpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + buffer_offset], dataField, dataLen);
		
		uart_send_package[dataLen + buffer_offset + 4] = UART_PROTCL_TAIL;

		uart_send_size = dataLen + 5;
	}
	
	
	
	if(monitor_tx_buf.length() + strlen(dataField) < 300)
	{
		monitor_tx_buf.concat(dataField);
	}
	else
	{
		//net_print((const uint8_t *)"overflow", strlen("overflow"));
	}
	return 0;
}



int package_file_first(File *fileHandle, char *fileName)
{
	int fileLen;
	char *ptr;
	int fileNameLen;
	int dataLen;
	char dbgStr[100] = {0};
	
	if(fileHandle == 0)
		return -1;
	fileLen = fileHandle->size();
	
	//net_print((const uint8_t *)"package_file_first:\n");
	
	//strcpy(fileName, (const char *)fileHandle->name());
	//sprintf(dbgStr, "fileLen:%d", fileLen);
	//net_print((const uint8_t *)dbgStr);
	//net_print((const uint8_t *)"\n");
	while(1)
	{
		ptr = (char *)strchr(fileName, '/');
		if(ptr == 0)
			break;
		else
		{
			strcpy(fileName, fileName + (ptr - fileName+ 1));
		}
	}
//	net_print((const uint8_t *)"fileName:");
	//net_print((const uint8_t *)fileName);
	//net_print((const uint8_t *)"\n");
	fileNameLen = strlen(fileName);

	dataLen = fileNameLen + 5;

	memset(uart_send_package, 0, sizeof(uart_send_package));
	uart_send_package[UART_PROTCL_HEAD_OFFSET] = UART_PROTCL_HEAD;
	uart_send_package[UART_PROTCL_TYPE_OFFSET] = UART_PROTCL_TYPE_FIRST;
	uart_send_package[UART_PROTCL_DATALEN_OFFSET] = dataLen & 0xff;
	uart_send_package[UART_PROTCL_DATALEN_OFFSET + 1] = dataLen >> 8;

	uart_send_package[UART_PROTCL_DATA_OFFSET] = fileNameLen;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 1] = fileLen & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 2] = (fileLen >> 8) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 3] = (fileLen >> 16) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 4] = (fileLen >> 24) & 0xff;
	strncpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + 5], fileName, fileNameLen);
	
	uart_send_package[dataLen + 4] = UART_PROTCL_TAIL;
	
	uart_send_size = dataLen + 5;

	return 0;
}

int package_file_first(char *fileName, int postLength)
{
	int fileLen;
	char *ptr;
	int fileNameLen;
	int dataLen;

	
	fileLen = postLength;
	
//	Serial.print("package_file_first:");
	
	while(1)
	{
		ptr = (char *)strchr(fileName, '/');
		if(ptr == 0)
			break;
		else
		{
			cut_msg_head((uint8_t *)fileName, strlen(fileName),  ptr - fileName+ 1);
		}
	}
//	Serial.print("fileName:");
//	Serial.println(fileName);
	
	fileNameLen = strlen(fileName);

	dataLen = fileNameLen + 5;

	memset(uart_send_package, 0, sizeof(uart_send_package));
	uart_send_package[UART_PROTCL_HEAD_OFFSET] = UART_PROTCL_HEAD;
	uart_send_package[UART_PROTCL_TYPE_OFFSET] = UART_PROTCL_TYPE_FIRST;
	uart_send_package[UART_PROTCL_DATALEN_OFFSET] = dataLen & 0xff;
	uart_send_package[UART_PROTCL_DATALEN_OFFSET + 1] = dataLen >> 8;

	uart_send_package[UART_PROTCL_DATA_OFFSET] = fileNameLen;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 1] = fileLen & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 2] = (fileLen >> 8) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 3] = (fileLen >> 16) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 4] = (fileLen >> 24) & 0xff;
	memcpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + 5], fileName, fileNameLen);
	
	uart_send_package[dataLen + 4] = UART_PROTCL_TAIL;
	
	uart_send_size = dataLen + 5;

	return 0;
}


int package_file_fragment(uint8_t *dataField, uint32_t fragLen, int32_t fragment)
{
	int dataLen;
	char dbgStr[100] = {0};

	dataLen = fragLen + 4;

	//sprintf(dbgStr, "fragment:%d\n", fragment);
	//net_print((const uint8_t *)dbgStr);
	
	memset(uart_send_package, 0, sizeof(uart_send_package));
	uart_send_package[UART_PROTCL_HEAD_OFFSET] = UART_PROTCL_HEAD;
	uart_send_package[UART_PROTCL_TYPE_OFFSET] = UART_PROTCL_TYPE_FRAGMENT;
	uart_send_package[UART_PROTCL_DATALEN_OFFSET] = dataLen & 0xff;
	uart_send_package[UART_PROTCL_DATALEN_OFFSET + 1] = dataLen >> 8;

	uart_send_package[UART_PROTCL_DATA_OFFSET] = fragment & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 1] = (fragment >> 8) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 2] = (fragment >> 16) & 0xff;
	uart_send_package[UART_PROTCL_DATA_OFFSET + 3] = (fragment >> 24) & 0xff;
	
	memcpy(&uart_send_package[UART_PROTCL_DATA_OFFSET + 4], (const char *)dataField, fragLen);
	
	uart_send_package[dataLen + 4] = UART_PROTCL_TAIL;
	
	uart_send_size = 1024;
	

	return 0;
}

unsigned long startTick = 0;
size_t readBytes;
uint8_t blockBuf[FILE_BLOCK_SIZE] = {0};
	

void do_transfer()
{
	
	char dbgStr[100] = {0};
	int i;
	long useTick ;
	long now;
	
	
	
	switch(transfer_state)
	{
		case TRANSFER_IDLE:
			if((uart_send_length_important > 0) || (uart_send_size > 0))
			{
				digitalWrite(EspReqTransferPin, LOW);
				if(digitalRead(McuTfrReadyPin) == LOW) // STM32 READY SIGNAL
				{
					transfer_state = TRANSFER_FRAGMENT;
				}
				else
					transfer_state = TRANSFER_READY;
			}
			
			break;
			
		case TRANSFER_GET_FILE:
			//if(Serial.baudRate() != 4500000)
			if(Serial.baudRate() != 1958400)
			{
				Serial.flush();
				Serial.begin(1958400);
				// Serial.begin(4500000);
			}
			
			 
			readBytes = gFileFifo.pop((char *)blockBuf, FILE_BLOCK_SIZE);
			if(readBytes > 0)
			{
				if(rcv_end_flag && (readBytes < FILE_BLOCK_SIZE))
				{
					file_fragment |= (1 << 31);	//the last fragment
				}
				else
				{
					file_fragment &= ~(1 << 31);
				}

				package_file_fragment(blockBuf, readBytes, file_fragment);
			
				digitalWrite(EspReqTransferPin, LOW);
				
				transfer_state = TRANSFER_READY;

				file_fragment++;

				
			}
			else if(rcv_end_flag)
			{
				memset(blockBuf, 0, sizeof(blockBuf));
				readBytes = 0;
				file_fragment |= (1 << 31);	//the last fragment

				package_file_fragment(blockBuf, readBytes, file_fragment);
			
				digitalWrite(EspReqTransferPin, LOW);
				
				transfer_state = TRANSFER_READY;

		#if 0		
				/*have finished*/
				digitalWrite(EspReqTransferPin, HIGH);
				rcv_end_flag = false;

				transfer_file_flag = false;
				
				transfer_state = TRANSFER_IDLE;
		#endif
			}

			
			
			break;

		case TRANSFER_READY:
						
			if(digitalRead(McuTfrReadyPin) == LOW) // STM32 READY SIGNAL
			{
				transfer_state = TRANSFER_FRAGMENT;
			}
				
			break;
			
		case TRANSFER_FRAGMENT:
				
		//	Serial.write(uart_send_package, sizeof(uart_send_package));
			if(uart_send_length_important > 0)
			{
				uart_send_length_important = (uart_send_length_important >= sizeof(uart_send_package_important) ? sizeof(uart_send_package_important) : uart_send_length_important);
				Serial.write(uart_send_package_important, uart_send_length_important);
				uart_send_length_important = 0;
				memset(uart_send_package_important, 0, sizeof(uart_send_package_important));
			}
			else
			{
				Serial.write(uart_send_package, uart_send_size);
				uart_send_size = 0;
				memset(uart_send_package, 0, sizeof(uart_send_package));
			}
			

			
			digitalWrite(EspReqTransferPin, HIGH);

			if(!transfer_file_flag)
			{
				transfer_state = TRANSFER_IDLE;
			}
			else
			{
				if(rcv_end_flag && (readBytes < FILE_BLOCK_SIZE))
				{							
					
					if(Serial.baudRate() != 115200)
					{
						Serial.flush();
						 Serial.begin(115200);
					}
					transfer_file_flag = false;
					rcv_end_flag = false;
					transfer_state = TRANSFER_IDLE;

					
				}
				else
				{
					transfer_state = TRANSFER_GET_FILE;
				}
			}
				
			break;
			
		default:
			break;



	}
	if(transfer_file_flag)
	{
	//	sprintf((char *)dbgStr, "left:%d,transfer_frags:%d\n", gFileFifo.left() , transfer_frags);
	//	net_print((const uint8_t *) dbgStr);
		if((gFileFifo.left() >= TCP_FRAG_LEN) && (transfer_frags >= TCP_FRAG_LEN))
		{
			net_print((const uint8_t *) "ok\n", strlen((const char *)"ok\n"));
			transfer_frags -= TCP_FRAG_LEN;
		}
	}
		
			

	
}


/*******************************************************************
    receive data from stm32 handler

********************************************************************/
#define UART_RX_BUFFER_SIZE    1024

#define ESP_PROTOC_HEAD	(uint8_t)0xa5
#define ESP_PROTOC_TAIL		(uint8_t)0xfc

#define ESP_TYPE_NET			(uint8_t)0x0
#define ESP_TYPE_PRINTER		(uint8_t)0x1
#define ESP_TYPE_TRANSFER		(uint8_t)0x2
#define ESP_TYPE_EXCEPTION		(uint8_t)0x3
#define ESP_TYPE_CLOUD			(uint8_t)0x4
#define ESP_TYPE_UNBIND		(uint8_t)0x5
#define ESP_TYPE_WID			(uint8_t)0x6
#define ESP_TYPE_SCAN_WIFI		(uint8_t)0x7
#define ESP_TYPE_MANUAL_IP		(uint8_t)0x8
#define ESP_TYPE_WIFI_CTRL		(uint8_t)0x9


uint8_t esp_msg_buf[UART_RX_BUFFER_SIZE] = {0}; //麓忙麓垄麓媒麓娄脌铆碌脛脢媒戮脻
uint16_t esp_msg_index = 0; //脨麓脰赂脮毛

typedef struct
{
	uint8_t head; //0xa5
	uint8_t type; //0x0:脡猫脰脙脥酶脗莽虏脦脢媒,0x1:麓貌脫隆禄煤脨脜脧垄,0x2:脥赂麓芦脨脜脧垄,0x3:脪矛鲁拢脨脜脧垄
	uint16_t dataLen; //脢媒戮脻鲁陇露脠
	uint8_t *data; //脫脨脨搂脢媒戮脻
	uint8_t tail; // 0xfc
} ESP_PROTOC_FRAME;


/*路碌禄脴脢媒脳茅脰脨脛鲁脳脰路没鲁枚脧脰脳卯脭莽碌脛脣梅脪媒潞脜拢卢麓脫0驴陋脢录,脠么虏禄麓忙脭脷脭貌路碌禄脴-1*/
static int32_t charAtArray(const uint8_t *_array, uint32_t _arrayLen, uint8_t _char)
{
	uint32_t i;
	for(i = 0; i < _arrayLen; i++)
	{
		if(*(_array + i) == _char)
		{
			return i;
		}
	}
	
	return -1;
}

static int cut_msg_head(uint8_t *msg, uint16_t msgLen, uint16_t cutLen)
{
	int i;
	
	if(msgLen < cutLen)
	{
		return 0;
	}
	else if(msgLen == cutLen)
	{
		memset(msg, 0, msgLen);
		return 0;
	}
	for(i = 0; i < (msgLen - cutLen); i++)
	{
		msg[i] = msg[cutLen + i];
	}
	memset(&msg[msgLen - cutLen], 0, cutLen);
	
	return msgLen - cutLen;
	
}




static void net_msg_handle(uint8_t * msg, uint16_t msgLen)
{
	uint8_t cfg_mode;
	uint8_t cfg_wifi_len;
	uint8_t *cfg_wifi;
	uint8_t cfg_key_len;
	uint8_t *cfg_key;

	char valid[1] = {0x0a};

	if(msgLen <= 0)
		return;

	//0x01:AP
	//0x02:Client
	//0x03:AP+Client(?Y2??隆矛3?)
	if((msg[0] != 0x01) && (msg[0] != 0x02)) 
		return;
	cfg_mode = msg[0];

	if(msg[1] > 32)
		return;
	cfg_wifi_len = msg[1];
	cfg_wifi = &msg[2];
	
	if(msg[2 +cfg_wifi_len ] > 64)
		return;
	cfg_key_len = msg[2 +cfg_wifi_len];
	cfg_key = &msg[3 +cfg_wifi_len];

	
	
	if((cfg_mode == 0x01) && ((currentState == OperatingState::Client) 
		|| (cfg_wifi_len != strlen((const char *)softApName))
		|| (strncmp((const char *)cfg_wifi, (const char *)softApName, cfg_wifi_len) != 0)
		|| (cfg_key_len != strlen((const char *)softApKey))
		|| (strncmp((const char *)cfg_key,  (const char *)softApKey, cfg_key_len) != 0)))
	{
		if((cfg_key_len > 0) && (cfg_key_len < 8))
		{
			return;
		}
		
		memset(softApName, 0, sizeof(softApName));
		memset(softApKey, 0, sizeof(softApKey));
		memset(wifi_mode, 0, sizeof(wifi_mode));
		
		strncpy((char *)softApName, (const char *)cfg_wifi, cfg_wifi_len);
		strncpy((char *)softApKey, (const char *)cfg_key, cfg_key_len);

	
		strcpy((char *)wifi_mode, "wifi_mode_ap");
		
		
		EEPROM.put(BAK_ADDRESS_WIFI_SSID, softApName);
		EEPROM.put(BAK_ADDRESS_WIFI_KEY, softApKey);
		
		EEPROM.put(BAK_ADDRESS_WIFI_MODE, wifi_mode);

		EEPROM.put(BAK_ADDRESS_WIFI_VALID, valid);
		
		EEPROM.commit();
		delay(300);
		ESP.restart();
	}
	else if((cfg_mode == 0x02) && ((currentState == OperatingState::AccessPoint)
		|| (cfg_wifi_len != strlen((const char *)ssid))
		|| (strncmp((const char *)cfg_wifi, (const char *)ssid, cfg_wifi_len) != 0)
		|| (cfg_key_len != strlen((const char *)pass))
		|| (strncmp((const char *)cfg_key,  (const char *)pass, cfg_key_len) != 0)))
	{
		memset(ssid, 0, sizeof(ssid));
		memset(pass, 0, sizeof(pass));
		memset(wifi_mode, 0, sizeof(wifi_mode));
		strncpy((char *)ssid, (const char *)cfg_wifi, cfg_wifi_len);
		strncpy((char *)pass, (const char *)cfg_key, cfg_key_len);
		
		strcpy((char *)wifi_mode, "wifi_mode_sta");	
		
		EEPROM.put(BAK_ADDRESS_WIFI_SSID, ssid);
		EEPROM.put(BAK_ADDRESS_WIFI_KEY, pass);

		EEPROM.put(BAK_ADDRESS_WIFI_MODE, wifi_mode);

		EEPROM.put(BAK_ADDRESS_WIFI_VALID, valid);

		//Disable manual ip mode		
		EEPROM.put(BAK_ADDRESS_MANUAL_IP_FLAG, 0xff);
		manual_valid = 0xff;

		EEPROM.commit();
		
		delay(300);
		ESP.restart();
	}
	
}

static void cloud_msg_handle(uint8_t * msg, uint16_t msgLen)
{
//Todo
}


static void scan_wifi_msg_handle()
{
	uint8_t valid_nums = 0;
	uint32_t byte_offset = 1;
	uint8_t node_lenth;
	int8_t signal_rssi;
	
	if(currentState == OperatingState::AccessPoint)
	{
		WiFi.mode(WIFI_STA);
		WiFi.disconnect();
		delay(100);
	}
	
	int n = WiFi.scanNetworks();
//	Serial.println("scan done");
	if (n == 0)
	{
		//Serial.println("no networks found");
		return;
	}
	else
	{
		int index = 0;
		//Serial.print(n);
		//Serial.println(" networks found");
		memset(uart_send_package, 0, sizeof(uart_send_package));
		uart_send_package[UART_PROTCL_HEAD_OFFSET] = UART_PROTCL_HEAD;
		uart_send_package[UART_PROTCL_TYPE_OFFSET] = UART_PROTCL_TYPE_HOT_PORT;
		for (int i = 0; i < n; ++i)
		{
			if(valid_nums > 15)
				break;
			signal_rssi = (int8_t)WiFi.RSSI(i);
			// Print SSID and RSSI for each network found
			/*Serial.print(i + 1);
			Serial.print(": ");
			Serial.print(WiFi.SSID(i));
			Serial.print(" (");
			Serial.print(WiFi.RSSI(i));
			Serial.print(")");
			Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
			delay(10);*/
			node_lenth = (uint8_t)WiFi.SSID(i).length();
			if(node_lenth > 32)
			{					
				continue;
			}	
			if(signal_rssi < -78)
			{
				continue;
			}

			uart_send_package[UART_PROTCL_DATA_OFFSET + byte_offset] = node_lenth;
			WiFi.SSID(i).toCharArray(&uart_send_package[UART_PROTCL_DATA_OFFSET + byte_offset + 1], node_lenth + 1, 0);
			uart_send_package[UART_PROTCL_DATA_OFFSET + byte_offset + node_lenth + 1] = WiFi.RSSI(i);

			valid_nums++;
			byte_offset += node_lenth + 2;
			
		}
		
		uart_send_package[UART_PROTCL_DATA_OFFSET] = valid_nums;
		uart_send_package[UART_PROTCL_DATA_OFFSET + byte_offset] = UART_PROTCL_TAIL;
		uart_send_package[UART_PROTCL_DATALEN_OFFSET] = byte_offset & 0xff;
		uart_send_package[UART_PROTCL_DATALEN_OFFSET + 1] = byte_offset >> 8;

		uart_send_size = byte_offset + 5;

		/*if(transfer_state == TRANSFER_IDLE)
		{
			transfer_state = TRANSFER_READY;
			digitalWrite(EspReqTransferPin, LOW);
		}*/
		
	}
	//Serial.println("");
}


static void manual_ip_msg_handle(uint8_t * msg, uint16_t msgLen)
{
	
	if(msgLen < 16)
		return;
		
	ip_static = (msg[3] << 24) + (msg[2] << 16) + (msg[1] << 8) + msg[0];
	subnet_static = (msg[7] << 24) + (msg[6] << 16) + (msg[5] << 8) + msg[4];
	gateway_staic = (msg[11] << 24) + (msg[10] << 16) + (msg[9] << 8) + msg[8];
	dns_static = (msg[15] << 24) + (msg[14] << 16) + (msg[13] << 8) + msg[12];

	manual_valid = 0xa;
	
	WiFi.config(ip_static, gateway_staic, subnet_static, dns_static, (uint32_t)0x00000000);

	EEPROM.put(BAK_ADDRESS_MANUAL_IP, ip_static);
	EEPROM.put(BAK_ADDRESS_MANUAL_MASK, subnet_static);
	EEPROM.put(BAK_ADDRESS_MANUAL_GATEWAY, gateway_staic);
	EEPROM.put(BAK_ADDRESS_MANUAL_DNS, dns_static);
	EEPROM.put(BAK_ADDRESS_MANUAL_IP_FLAG, manual_valid);

	EEPROM.commit();

	
	
}

static void wifi_ctrl_msg_handle(uint8_t * msg, uint16_t msgLen)
{
	if(msgLen != 1)
		return;

	uint8_t ctrl_code = msg[0];

	/*connect the wifi network*/
	if(ctrl_code == 0x1)
	{
		if(!WiFi.isConnected())
		{
			WiFi.begin(ssid, pass);
		}
	}
	/*disconnect the wifi network*/
	else if(ctrl_code == 0x2)
	{
		if(WiFi.isConnected())
		{
			WiFi.disconnect();
		}
	}
	/*disconnect the wifi network and forget the password*/
	else if(ctrl_code == 0x3)
	{
		if(WiFi.isConnected())
		{
			WiFi.disconnect();
		}
		memset(ssid, 0, sizeof(ssid));
		memset(pass, 0, sizeof(pass));
	
				
		EEPROM.put(BAK_ADDRESS_WIFI_SSID, ssid);
		EEPROM.put(BAK_ADDRESS_WIFI_KEY, pass);

		EEPROM.put(BAK_ADDRESS_WIFI_VALID, 0Xff);

		//Disable manual ip mode		
		EEPROM.put(BAK_ADDRESS_MANUAL_IP_FLAG, 0xff);
		manual_valid = 0xff;

		EEPROM.commit();
	}
}

static void except_msg_handle(uint8_t * msg, uint16_t msgLen)
{
	uint8_t except_code = msg[0];
	if(except_code == 0x1) // transfer error
	{
		upload_error = true;
	}
	else if(except_code == 0x2) // transfer sucessfully
	{
		upload_success = true;
	}
}

static void wid_msg_handle(uint8_t * msg, uint16_t msgLen)
{
//Todo
}

static void transfer_msg_handle(uint8_t * msg, uint16_t msgLen)
{
	int j = 0;
	char cmd_line[100] = {0};
	
	init_queue(&cmd_queue);
	cmd_index = 0;
	memset(cmd_fifo, 0, sizeof(cmd_fifo));
	
	while(j < msgLen)
	{
		if((msg[j] == '\r') || (msg[j] == '\n'))
		{
			if((cmd_index) > 1)
			{
				cmd_fifo[cmd_index] = '\n';
				cmd_index++;

				
				push_queue(&cmd_queue, cmd_fifo, cmd_index);
			}
			memset(cmd_fifo, 0, sizeof(cmd_fifo));
			cmd_index = 0;
	//	net_print((const uint8_t*)"push:", strlen((const char *)"push:"));
	//	net_print((const uint8_t*)cmd_fifo, strlen((const char *)cmd_fifo));
		}
		else if(msg[j] == '\0')
			break;
		else
		{
			if(cmd_index >= sizeof(cmd_fifo))
			{
				memset(cmd_fifo, 0, sizeof(cmd_fifo));
				cmd_index = 0;
			}
			cmd_fifo[cmd_index] = msg[j];
			cmd_index++;
		}

		j++;

		do_transfer();
		yield();
	
	}
	while(pop_queue(&cmd_queue, cmd_line, sizeof(cmd_line)) >= 0)		
	{
		if(monitor_rx_buf.length() + strlen(cmd_line) < 500)
		{
			monitor_rx_buf.concat((const char *)cmd_line);
		}
		else
		{
			//net_print((const uint8_t *)"rx overflow", strlen("rx overflow"));
		}
		/*
		if((cmd_line[0] == 'o') && (cmd_line[1] == 'k'))
		{
			cut_msg_head((uint8_t *)cmd_line, strlen((const char*)cmd_line), 2);
			//if(strlen(cmd_line) < 4)
				continue;
		}*/

		/*handle the cmd*/
		paser_cmd((uint8_t *)cmd_line);
		do_transfer();
		yield();
		
		if((cmd_line[0] == 'T') && (cmd_line[1] == ':'))
		{		
			String tempVal((const char *)cmd_line);
			int index = tempVal.indexOf("B:", 0);
			if(index != -1)			
			{
				memset(dbgStr, 0, sizeof(dbgStr));
				sprintf((char *)dbgStr, "T:%d /%d B:%d /%d T0:%d /%d T1:%d /%d @:0 B@:0\r\n", 
					(int)gPrinterInf.curSprayerTemp[0], (int)gPrinterInf.desireSprayerTemp[0], (int)gPrinterInf.curBedTemp, (int)gPrinterInf.desireBedTemp,
					(int)gPrinterInf.curSprayerTemp[0], (int)gPrinterInf.desireSprayerTemp[0], (int)gPrinterInf.curSprayerTemp[1], (int)gPrinterInf.desireSprayerTemp[1]);
				net_print((const uint8_t*)dbgStr, strlen((const char *)dbgStr));
					
			}
			continue;
		}
		else if((cmd_line[0] == 'M') && (cmd_line[1] == '9') && (cmd_line[2] == '9') 
			&& ((cmd_line[3] == '7') ||  (cmd_line[3] == '2') ||  (cmd_line[3] == '4')))
		{
			continue;
		}
		else if((cmd_line[0] == 'M') && (cmd_line[1] == '2') && (cmd_line[2] == '7'))
		{
			continue;
		}
		else
		{
			net_print((const uint8_t*)cmd_line, strlen((const char *)cmd_line));
		}
		
		
	}
	
	
	
}

void esp_data_parser(char *cmdRxBuf, int len)
{
	int32_t head_pos;
	int32_t tail_pos;
	uint16_t cpyLen;
	int16_t leftLen = len; //脢拢脫脿鲁陇露脠
	uint8_t loop_again = 0;
	int i;

	ESP_PROTOC_FRAME esp_frame;

	
	//net_print((const uint8_t *)"rcv:");

	//net_print((const uint8_t *)"\n");
	
	
	while((leftLen > 0) || (loop_again == 1))
	//while(leftLen > 0)
	{
		loop_again = 0;
		
		/* 1. 虏茅脮脪脰隆脥路*/
		if(esp_msg_index != 0)
		{
			head_pos = 0;
			cpyLen = (leftLen < (sizeof(esp_msg_buf) - esp_msg_index)) ? leftLen : sizeof(esp_msg_buf) - esp_msg_index;
			
			memcpy(&esp_msg_buf[esp_msg_index], cmdRxBuf + len - leftLen, cpyLen);			

			esp_msg_index += cpyLen;

			leftLen = leftLen - cpyLen;
			tail_pos = charAtArray(esp_msg_buf, esp_msg_index, ESP_PROTOC_TAIL);
			
		//	net_print((const uint8_t *)esp_msg_buf, esp_msg_index);	
			if(tail_pos == -1)
			{
				//脙禄脫脨脰隆脦虏
				if(esp_msg_index >= sizeof(esp_msg_buf))
				{
					memset(esp_msg_buf, 0, sizeof(esp_msg_buf));
					esp_msg_index = 0;
				}
			
				return;
			}
		}
		else
		{
			head_pos = charAtArray((uint8_t const *)&cmdRxBuf[len - leftLen], leftLen, ESP_PROTOC_HEAD);
		//	net_print((const uint8_t *)"esp_data_parser1\n");
			if(head_pos == -1)
			{
				//脙禄脫脨脰隆脥路
				return;
			}
			else
			{
				//脧脠禄潞麓忙碌陆buf	
				memset(esp_msg_buf, 0, sizeof(esp_msg_buf));
				memcpy(esp_msg_buf, &cmdRxBuf[len - leftLen + head_pos], leftLen - head_pos);

				esp_msg_index = leftLen - head_pos;


				leftLen = 0;

				head_pos = 0;
				
				tail_pos = charAtArray(esp_msg_buf, esp_msg_index, ESP_PROTOC_TAIL);
				//net_print((const uint8_t *)"esp_data_parser2\n", strlen((const char *)"esp_data_parser2\n"));
				if(tail_pos == -1)
				{
					//脮脪碌陆脰隆脥路拢卢脙禄脫脨脰隆脦虏		
					return;
				}
				
			}
		}
		//net_print((const uint8_t *)"esp_data_parser3\n");
		/*3. 脮脪碌陆脥锚脮没碌脛脪禄脰隆	, 脜脨露脧脢媒戮脻鲁陇露脠*/
		esp_frame.type = esp_msg_buf[1];
	
		if((esp_frame.type != ESP_TYPE_NET) && (esp_frame.type != ESP_TYPE_PRINTER)
			 && (esp_frame.type != ESP_TYPE_CLOUD) && (esp_frame.type != ESP_TYPE_UNBIND)
			 && (esp_frame.type != ESP_TYPE_TRANSFER) && (esp_frame.type != ESP_TYPE_EXCEPTION) 
			 && (esp_frame.type != ESP_TYPE_WID) && (esp_frame.type != ESP_TYPE_SCAN_WIFI)
			 && (esp_frame.type != ESP_TYPE_MANUAL_IP) && (esp_frame.type != ESP_TYPE_WIFI_CTRL))
		{
			//脢媒戮脻脌脿脨脥虏禄脮媒脠路拢卢露陋脝煤
			memset(esp_msg_buf, 0, sizeof(esp_msg_buf));
			esp_msg_index = 0;
			//net_print((const uint8_t *)"type err\n", strlen("type err\n"));
			return;
		}
		//net_print((const uint8_t *)"esp_data_parser4\n");
		esp_frame.dataLen = esp_msg_buf[2] + (esp_msg_buf[3] << 8);

		/*脢媒戮脻鲁陇露脠虏禄脮媒脠路*/
		if(4 + esp_frame.dataLen > sizeof(esp_msg_buf))
		{
			//脢媒戮脻鲁陇露脠虏禄脮媒脠路拢卢露陋脝煤
			memset(esp_msg_buf, 0, sizeof(esp_msg_buf));
			esp_msg_index = 0;
			//net_print((const uint8_t *)"len err\n", strlen("len err\n"));
			return;
		}

		if(esp_msg_buf[4 + esp_frame.dataLen] != ESP_PROTOC_TAIL)
		{
			//脰隆脦虏虏禄脮媒脠路拢卢露陋脝煤
			memset(esp_msg_buf, 0, sizeof(esp_msg_buf));
			//net_print((const uint8_t *)"tail err\n", strlen("tail err\n"));
			esp_msg_index = 0;
			return;
		}
	
		/*4. 掳麓脮脮脌脿脨脥路脰卤冒麓娄脌铆脢媒戮脻*/		
		esp_frame.data = &esp_msg_buf[4];

		
		
	
		switch(esp_frame.type)
		{
			case ESP_TYPE_NET:
				net_msg_handle(esp_frame.data, esp_frame.dataLen);
				break;

			case ESP_TYPE_PRINTER:
				//gcode_msg_handle(esp_frame.data, esp_frame.dataLen);
				break;

			case ESP_TYPE_TRANSFER:
			//	net_print((const uint8_t *)"ESP_TYPE_TRANSFER", strlen((const char *)"ESP_TYPE_TRANSFER"));
				if(verification_flag)
					transfer_msg_handle(esp_frame.data, esp_frame.dataLen);
				break;

			case ESP_TYPE_CLOUD:
				if(verification_flag)
					cloud_msg_handle(esp_frame.data, esp_frame.dataLen);
				break;

			case ESP_TYPE_EXCEPTION:
				
				except_msg_handle(esp_frame.data, esp_frame.dataLen);
				break;

			case ESP_TYPE_UNBIND:
				if(cloud_link_state == 3)
				{
					unbind_exec = 1;
				}
				break;
			case ESP_TYPE_WID:
				wid_msg_handle(esp_frame.data, esp_frame.dataLen);
				break;

			case ESP_TYPE_SCAN_WIFI:
				
				scan_wifi_msg_handle();
				break;

			case ESP_TYPE_MANUAL_IP:				
				manual_ip_msg_handle(esp_frame.data, esp_frame.dataLen);
				break;

			case ESP_TYPE_WIFI_CTRL:				
				wifi_ctrl_msg_handle(esp_frame.data, esp_frame.dataLen);
				break;
			
			default:
				break;				
		}
		/*5. 掳脩脪脩麓娄脌铆碌脛脢媒戮脻陆脴碌么*/
		esp_msg_index = cut_msg_head(esp_msg_buf, esp_msg_index, esp_frame.dataLen  + 5);
		if(esp_msg_index > 0)
		{
			if(charAtArray(esp_msg_buf, esp_msg_index,  ESP_PROTOC_HEAD) == -1)
			{
				memset(esp_msg_buf, 0, sizeof(esp_msg_buf));
				esp_msg_index = 0;
				return;
			}
			
			if((charAtArray(esp_msg_buf, esp_msg_index,  ESP_PROTOC_HEAD) != -1) && (charAtArray(esp_msg_buf, esp_msg_index, ESP_PROTOC_TAIL) != -1))
			{
				loop_again = 1;
			}
		}
		yield();
	
	}
}



// Try to connect using the saved SSID and password, returning true if successful
bool TryToConnect()
{

	char eeprom_valid[1] = {0};
	uint8_t failcount = 0;
	
	EEPROM.get(BAK_ADDRESS_WIFI_VALID, eeprom_valid);
	if(eeprom_valid[0] == 0x0a)
	{
		EEPROM.get(BAK_ADDRESS_WIFI_MODE, wifi_mode);		
		EEPROM.get(BAK_ADDRESS_WEB_HOST, webhostname);
		//wifi_station_set_hostname(webhostname);     // must do thia before calling WiFi.begin()
	
	}
	else
	{
		memset(wifi_mode, 0, sizeof(wifi_mode));
		strcpy(wifi_mode, "wifi_mode_ap");
		//Serial.println("not valid");
		NET_INF_UPLOAD_CYCLE = 1000;
	}
	
	

	if(strcmp(wifi_mode, "wifi_mode_ap") != 0)
	{	
		if(eeprom_valid[0] == 0x0a)
		{
			EEPROM.get(BAK_ADDRESS_WIFI_SSID, ssid);
			EEPROM.get(BAK_ADDRESS_WIFI_KEY, pass);
		}
		else
		{
			memset(ssid, 0, sizeof(ssid));
			strcpy(ssid, "mks1");
			memset(pass, 0, sizeof(pass));
			strcpy(pass, "makerbase");
		}
		
		/*Serial.print("config station:");
		Serial.println(ssid);
		Serial.print("key:");
		Serial.println(pass);*/

		currentState = OperatingState::Client;
		package_net_para();
		
		Serial.write(uart_send_package, uart_send_size);		
		
		transfer_state = TRANSFER_READY;

		delay(1000);
		
			
		WiFi.mode(WIFI_STA);
		WiFi.disconnect();
		
		delay(1000);

		EEPROM.get(BAK_ADDRESS_MANUAL_IP_FLAG, manual_valid);
		if(manual_valid == 0xa)
		{
			uint32_t manual_ip, manual_subnet, manual_gateway, manual_dns;
			
			EEPROM.get(BAK_ADDRESS_MANUAL_IP, ip_static);
			EEPROM.get(BAK_ADDRESS_MANUAL_MASK, subnet_static);
			EEPROM.get(BAK_ADDRESS_MANUAL_GATEWAY, gateway_staic);
			EEPROM.get(BAK_ADDRESS_MANUAL_DNS, dns_static);

			WiFi.config(ip_static, gateway_staic, subnet_static, dns_static, (uint32_t)0x00000000);
		}

		WiFi.begin(ssid, pass);

		

		while (WiFi.status() != WL_CONNECTED)
		{
			if(get_printer_reply() > 0)
			{
				esp_data_parser((char *)uart_rcv_package, uart_rcv_index);
			}
			uart_rcv_index = 0;
			package_net_para();
		
			Serial.write(uart_send_package, uart_send_size);
			
			delay(500);
			
			failcount++;
			
			if (failcount > MAX_WIFI_FAIL)  // 1 min
			{
			 // WiFi.mode(WIFI_STA);
			//  WiFi.disconnect();
			  delay(100);
			  return false;
			}
			do_transfer();
			
	 	}
		//Serial.println("WIFI OK");
  	}
#if 1
	else
	{
		if(eeprom_valid[0] == 0x0a)
		{
			EEPROM.get(BAK_ADDRESS_WIFI_SSID, softApName);
			EEPROM.get(BAK_ADDRESS_WIFI_KEY, softApKey);
		}
		else
		{
			String macStr= WiFi.macAddress();
			macStr.replace(":", "");
			
			strcat(softApName, macStr.substring(8).c_str());
			memset(pass, 0, sizeof(pass));
		}
		currentState = OperatingState::AccessPoint;
		
	//	Serial.println("config ap");
		WiFi.mode(WIFI_AP);
		WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
		if(strlen(softApKey) != 0)
			WiFi.softAP(softApName, softApKey);
		else
			WiFi.softAP(softApName);
	//	dns.setErrorReplyCode(DNSReplyCode::NoError);
	//	dns.start(53, "*", apIP);
	//	Serial.println("not valid");

		
	}
	#endif
  	return true;
}

uint8_t refreshApWeb()
{
	#if 0
	 uint8_t num_ssids = WiFi.scanNetworks();

	
	wifiConfigHtml = F("<html><body><h1>Select your WiFi network:</h1><br /><form method=\"POST\">");
	for (uint8_t i = 0; i < num_ssids; i++) {
	 wifiConfigHtml += "<input type=\"radio\" id=\"" + WiFi.SSID(i) + "\"name=\"ssid\" value=\"" + WiFi.SSID(i) + "\" /><label for=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</label><br />";
	}
	wifiConfigHtml += F("<label for=\"password\">WiFi Password:</label><input type=" PASSWORD_INPUT_TYPE " id=\"password\" name=\"password\" /><br />");
	wifiConfigHtml += F("<p><label for=\"webhostname\">Duet host name: </label><input type=\"text\" id=\"webhostname\" name=\"webhostname\" value=\"mkswifi\" /><br />");
	wifiConfigHtml += F("<i>(This would allow you to access your printer by name instead of IP address. I.e. http://mkswifi/)</i></p>");
	wifiConfigHtml += F("<input type=\"submit\" value=\"Save and reboot\" /></form></body></html>");
	#endif
	#if 0
	wifiConfigHtml = F("<html><head><meta http-equiv=\"Content-Type\" content=\"text/html;\"><title>MKS WIFI脜盲脰脙</title><style>body{background: #b5ff6a;}.config{margin: 150px auto;width: 600px;height: 600px;overflow: hidden;</style></head>");
	//wifiConfigHtml += F("<body><div class=\"config\"><br /><form method=\"POST\"><caption><h1>MKS WIFI脜盲脰脙</h1></caption> <div>赂陆陆眉WIFI脕脨卤铆:</div>/>");
	wifiConfigHtml += F("<body><div class=\"config\"><br /><form method=\"POST\" action='config' ><caption><h1>MKS WIFI</h1></caption> ");
	/*
	for (uint8_t i = 0; i < num_ssids; i++) {		
		wifiConfigHtml += "<input type=\"radio\" id=\"" + WiFi.SSID(i) + "\"name=\"ssid\" value=\"" + WiFi.SSID(i) + "\" /><label for=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</label><br />";
	}*/
	wifiConfigHtml += F("<br /><h2>赂眉脨脗鹿脤录镁</h2><form method='POST' action='update_sketch' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></td></tr></table></form><br /><br/>");
	wifiConfigHtml += F("<h2>脥酶脗莽脜盲脰脙</h2><input type=\"radio\" id=\"wifi_mode_sta\" name=\"wifi_mode\" value=\"wifi_mode_sta\" /><label for=\"wifi_mode_sta\">STA(脡猫脰脙脕卢陆脫碌陆脧脗脙忙wifi)</label><br /><input type=\"radio\" id=\"wifi_mode_ap\" name=\"wifi_mode\" value=\"wifi_mode_ap\" /><label for=\"wifi_mode_ap\">AP(脡猫脰脙脛拢驴茅脦陋脧脗脙忙脠脠碌茫)</label><br />");
	

	wifiConfigHtml += F("<br /><br /><table border='0'><tr><td><label for=\"password\">WIFI拢潞</label><input type=\"text\" id=\"hidden_ssid\" name=\"hidden_ssid\" /></td></tr><tr><td><label for=\"password\">脙脺脗毛拢潞</label>");wifiConfigHtml += F("<input type=\" PASSWORD_INPUT_TYPE \" id=\"password\" name=\"password\" /></td></tr><tr><td colspan=2 align=\"right\"> <input type=\"submit\" value=\"脡猫脰脙虏垄脰脴脝么\"></td></tr></table></form></div></div></body></html>");
#endif
	wifiConfigHtml = F("<html><head><meta http-equiv='Content-Type' content='text/html;'><title>MKS WIFI</title><style>body{background: #b5ff6a;}.config{margin: 150px auto;width: 600px;height: 600px;overflow: hidden;</style></head>");
	wifiConfigHtml += F("<body><div class='config'></caption><br /><h2>Update</h2>");
	wifiConfigHtml += F("<form method='POST' action='update_sketch' enctype='multipart/form-data'><table border='0'><tr><td>wifi firmware:</td><td><input type='file' name='update' ></td><td><input type='submit' value='update'></td></tr></form>");
	wifiConfigHtml += F("<form method='POST' action='update_spiffs' enctype='multipart/form-data'><tr><td>web view:</td><td><input type='file' name='update' ></td><td><input type='submit' value='update'></td></tr></table></form>");
	wifiConfigHtml += F("<br /><br /><h2>WIFI Configuration</h2><form method='GET' action='update_cfg'><caption><input type='radio' id='wifi_mode_sta' name='wifi_mode' value='wifi_mode_sta' /><label for='wifi_mode_sta'>STA</label><br />");
	wifiConfigHtml += F("<input type='radio' id='wifi_mode_ap' name='wifi_mode' value='wifi_mode_ap' /><label for='wifi_mode_ap'>AP</label><br /><br /><table border='0'><tr><td>");
	wifiConfigHtml += F("WIFI: </td><td><input type='text' id='hidden_ssid' name='hidden_ssid' /></td></tr><tr><td>KEY: </td><td><input type=' PASSWORD_INPUT_TYPE ' id='password' name='password' />");
	wifiConfigHtml += F("</td></tr><tr><td colspan=2 align='right'> <input type='submit' value='config and reboot'></td></tr></table></form></div></body></html>");
	return 0;
}

char hidden_ssid[32] = {0};

void onWifiConfig()
{
	uint8_t num_ssids =	refreshApWeb();

	server.on("/", HTTP_GET, []() {
		server.send(200, FPSTR(STR_MIME_TEXT_HTML), wifiConfigHtml);
	});
	server.on("/update_sketch", HTTP_GET, []() {
		server.send(200, FPSTR(STR_MIME_TEXT_HTML), wifiConfigHtml);
	});
	server.on("/update_spiffs", HTTP_GET, []() {
		server.send(200, FPSTR(STR_MIME_TEXT_HTML), wifiConfigHtml);
	});

 
 	server.on("/update_cfg", HTTP_GET, []() {
		//Serial.printf("on http post\n");

		if (server.args() <= 0) 
		{
			server.send(500, FPSTR(STR_MIME_TEXT_PLAIN), F("Got no data, go back and retry"));
			return;
		}
		for (uint8_t e = 0; e < server.args(); e++) {
			String argument = server.arg(e);
			urldecode(argument);
			if (server.argName(e) == "password") argument.toCharArray(pass, 64);//pass = server.arg(e);
			else if (server.argName(e) == "ssid") argument.toCharArray(ssid, 32);//ssid = server.arg(e);
			else if (server.argName(e) == "hidden_ssid") argument.toCharArray(hidden_ssid, 32);//ssid = server.arg(e);
			else if (server.argName(e) == "wifi_mode") argument.toCharArray(wifi_mode, 15);//ssid = server.arg(e);
			//else if (server.argName(e) == "webhostname") argument.toCharArray(webhostname, 64);
		}
		
		/*if(hidden_ssid[0] != 0)
		{
			memset(ssid, 0, sizeof(ssid));
			memcpy(ssid, hidden_ssid, sizeof(hidden_ssid));
		}*/
		if(strlen((const char *)hidden_ssid) <= 0)
		{
			server.send(200, FPSTR(STR_MIME_TEXT_HTML), F("<p>wifi parameters error!</p>"));
			return;
		}
		if((strcmp(wifi_mode, "wifi_mode_ap") == 0) && (strlen(pass) > 0) && ((strlen(pass) < 8) ))
		{
			server.send(500, FPSTR(STR_MIME_TEXT_PLAIN), F("wifi password length is not correct, go back and retry"));
			return;	
		}
		else
		{
			memset(ssid, 0, sizeof(ssid));
			memcpy(ssid, hidden_ssid, sizeof(hidden_ssid));
		}
		
		//Serial.printf("on http post:ready eeprom\n");
	
		char valid[1] = {0x0a};
		
		EEPROM.put(BAK_ADDRESS_WIFI_SSID, ssid);
		EEPROM.put(BAK_ADDRESS_WIFI_KEY, pass);
		EEPROM.put(BAK_ADDRESS_WEB_HOST, webhostname);

		EEPROM.put(BAK_ADDRESS_WIFI_MODE, wifi_mode);

		EEPROM.put(BAK_ADDRESS_WIFI_VALID, valid);

		EEPROM.put(BAK_ADDRESS_MANUAL_IP_FLAG, 0xff);
		manual_valid = 0xff;

		EEPROM.commit();
	

		//Serial.printf("on http post:ready send to client\n");
		server.send(200, FPSTR(STR_MIME_TEXT_HTML), F("<p>Configure successfully!<br />Please use the new ip to connect again.</p>"));
	
		
		//Serial.printf("on http post:after commit\n");
		delay(300);
		ESP.restart();
	});
}

void StartAccessPoint()
{
	
		
	
	delay(5000);
	IPAddress apIP(192, 168, 4, 1);
	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
	String macStr= WiFi.macAddress();
	macStr.replace(":", "");
	strcat(softApName, macStr.substring(8).c_str());
	WiFi.softAP(softApName);
//	dns.setErrorReplyCode(DNSReplyCode::NoError);
//	dns.start(53, "*", apIP);

	onWifiConfig();
	
  
  server.begin();
}

static void extract_file_item(File dataFile, String fileStr)
{
//Todo
}
static void extract_file_item_cloud(File dataFile, String fileStr)
{
//Todo
}
#if 1

//篓娄????隆颅???篓娄???2鈧??隆庐篓娄??篓篓隆矛????篓娄篓C?篓篓隆陋隆毛?1a?隆矛???隆颅???篓娄???隆陋-?隆盲?篓娄?篓C?隆庐???-GET??隆掳????鈧∶????
void fsHandler()
{
	String path = server.uri();

	if(!verification_flag)
	{
		return;
	}
	
	#if 0
	if (path.endsWith("/"))
	{
		path += F("reprap.htm");            // default to reprap.htm as the index page
	}
	
	#endif
	bool addedGz = false;
	File dataFile = SPIFFS.open(path, "r");

	if (!dataFile && !path.endsWith(".gz") && path.length() <= 29)
	{
		// Requested file not found and wasn't a zipped file, so see if we have a zipped version
		path += F(".gz");
		addedGz = true;
		dataFile = SPIFFS.open(path, "r");
	}
	if (!dataFile)
	{
		server.send(404, FPSTR(STR_MIME_APPLICATION_JSON), "{\"err\": \"404: " + server.uri() + " NOT FOUND\"}");
		return;
	}
	// No need to add the file size or encoding headers here because streamFile() does that automatically
	String dataType = FPSTR(STR_MIME_TEXT_PLAIN);
	if (path.endsWith(".html") || path.endsWith(".htm")) dataType = FPSTR(STR_MIME_TEXT_HTML);
	else if (path.endsWith(".css") || path.endsWith(".css.gz")) dataType = F("text/css");
	else if (path.endsWith(".js") || path.endsWith(".js.gz")) dataType = F("application/javascript");
	else if (!addedGz && path.endsWith(".gz")) dataType = F("application/x-gzip");
	else if ( path.endsWith(".png")) dataType = F("application/x-png");
	else if ( path.endsWith(".ico")) dataType = F("image/x-icon");


	/*篓娄????隆矛???html篓娄????|??? */
	server.streamFile(dataFile, dataType);


	dataFile.close();

}
#endif
#if 0
// Handle a rr_ request from the client
void handleGcode() 
{
	uint32_t now;
 uint32_t timeout;

  uint32_t postLength = server.getPostLength();
  String uri = server.uri();


  if(uri != NULL)
  {
   	//Serial.println("handle1");
  	//gcode
  	if(server.hasArg((const char *)"gcode"))
  	{
		if((!transfer_file_flag) && (transfer_state == TRANSFER_IDLE))
		{
			String gcodeStr = server.arg((const char *)"gcode");
			package_gcode(gcodeStr);
			transfer_state = TRANSFER_READY;
			digitalWrite(EspReqTransferPin, LOW);
			
			server.send(200, FPSTR(STR_MIME_APPLICATION_JSON), FPSTR(STR_JSON_ERR_0));	
  		}
		else
		{
			server.send(500, FPSTR(STR_MIME_APPLICATION_JSON), FPSTR(STR_JSON_ERR_500_IS_BUSY));	
		}
	

	
	  }
  
		 
  }

}
#endif


void handleUpload()
{


  uint32_t now;
  uint8_t readBuf[1024];

  uint32_t postLength = server.getPostLength();
  String uri = server.uri();
  

  if(uri != NULL)
  {
  	if((transfer_file_flag) || (transfer_state != TRANSFER_IDLE) || (gPrinterInf.print_state != PRINTER_IDLE))
	{	
		server.send(409, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("409 Conflict"));	
		return;
	}
		
  	if(server.hasArg((const char *) "X-Filename"))
  	{
  		if((transfer_file_flag) || (transfer_state != TRANSFER_IDLE))
  		{
  			server.send(500, FPSTR(STR_MIME_APPLICATION_JSON), FPSTR(STR_JSON_ERR_500_IS_BUSY));		
			return;
  		}

		file_fragment = 0;
		rcv_end_flag = false;
		transfer_file_flag = true;
		gFileFifo.reset();
		upload_error = false;
		upload_success = false;

		String FileName = server.arg((const char *) "X-Filename");
		//package_gcode(FileName, true);
		//transfer_state = TRANSFER_READY;
		//digitalWrite(EspReqTransferPin, LOW);
		//String fileNameAfterDecode = urlDecode(FileName);
		//package_gcode(fileNameAfterDecode, true);
		//transfer_state = TRANSFER_READY;
		//digitalWrite(EspReqTransferPin, LOW);
		if(package_file_first((char *)FileName.c_str(), (int)postLength) == 0)
		{
			/*transfer_state = TRANSFER_READY;
			digitalWrite(EspReqTransferPin, LOW);*/
		}
		else
		{
			transfer_file_flag = false;
		}
		/*wait m3 reply for first frame*/
		int wait_tick = 0;
		while(1)
		{
			do_transfer();
			
			delay(100);

			wait_tick++;

			if(wait_tick > 20) // 2s
			{
				if(digitalRead(McuTfrReadyPin) == HIGH) // STM32 READY SIGNAL
				{
					upload_error = true;		
				//	Serial.println("upload_error");
				}
				else
				{
			//		Serial.println("upload_sucess");
				}
				break;
			}
			
			int len_get = get_printer_reply();
			if(len_get > 0)
			{
				esp_data_parser((char *)uart_rcv_package, len_get);
			
				uart_rcv_index = 0;
			}

			if(upload_error)
			{
				break;
			}
		}
		
		if(!upload_error)
		{
			
			 now = millis();
			do
			{
				do_transfer();

				int len = get_printer_reply();
			
				if(len > 0)
				{
				//	Serial.println("rcv");
					esp_data_parser((char *)uart_rcv_package, len);
				
					uart_rcv_index = 0;
				}

				if(upload_error || upload_success)
				{
					break;
				}

				if (postLength != 0)
				{				  
					uint32_t len = gFileFifo.left();



					if (len > postLength)
					{
						 len = postLength;
					}
					if(len > sizeof(readBuf))
					{
						 len = sizeof(readBuf);
					}	
					if(len > 0)
					{

						size_t len2 = server.readPostdata(server.client(), readBuf, len);
				
						if (len2 > 0)
						{
							postLength -= len2;

							gFileFifo.push((char *)readBuf, len2);

							
							now = millis();
						}
					}
					
				}
				else
				{
					rcv_end_flag = true;
					break;
				}
				yield();
			
				
			}while (millis() - now < 10000);
		}
		
		if(upload_success || rcv_end_flag )
		{
			server.send(200, FPSTR(STR_MIME_APPLICATION_JSON), FPSTR(STR_JSON_ERR_0));	
		}
		//if((millis() - now >= 10000) || upload_error)
		else
		{
			
			//Serial.print("len:");
			//Serial.println(gFileFifo.left() );
			
			//Serial.print("postLength:");
			//Serial.println(postLength );
			if(Serial.baudRate() != 115200)
			{
				Serial.flush();
				Serial.begin(115200);
				// Serial.begin(4500000);
			}
			
			//Serial.println("timeout" );
			
			
			transfer_file_flag = false;
			rcv_end_flag = false;
			transfer_state = TRANSFER_IDLE;
			server.send(500, FPSTR(STR_MIME_APPLICATION_JSON), FPSTR(STR_JSON_ERR_500_NO_DATA_RECEIVED));	
		}
		
  	}
	else
	{
		
		server.send(500, FPSTR(STR_MIME_APPLICATION_JSON), FPSTR(STR_JSON_ERR_500_NO_FILENAME_PROVIDED));	
		return;
	}
	

		 
  }

  

}

#if 0
void handleConfig()
{
	uint32_t postLength = server.getPostLength();
	String uri = server.uri();
	char cfg_ssid[32];
	char cfg_pass[64];
	char cfg_wifi_mode[] = "wifi_mode_sta";
	
	  if(uri != NULL)
	  {
	   	
	  	for (uint8_t e = 0; e < server.args(); e++) {
			String argument = server.arg(e);
			urldecode(argument);
			if (server.argName(e) == "key") argument.toCharArray(cfg_pass, 64);//pass = server.arg(e);
			else if (server.argName(e) == "ssid") argument.toCharArray(cfg_ssid, 32);//ssid = server.arg(e);
			else if (server.argName(e) == "mode") argument.toCharArray(cfg_wifi_mode, 15);//ssid = server.arg(e);
			//else if (server.argName(e) == "webhostname") argument.toCharArray(webhostname, 64);
		}
	  	
		//Serial.printf("mode: %s\nssid: %s\nkey: %s\n", wifi_mode, ssid, pass);				

		char valid[1] = {0x0a};
		
		EEPROM.put(BAK_ADDRESS_WIFI_SSID, cfg_ssid);
		EEPROM.put(BAK_ADDRESS_WIFI_KEY, cfg_pass);
		
		EEPROM.put(BAK_ADDRESS_WIFI_MODE, cfg_wifi_mode);

		EEPROM.put(BAK_ADDRESS_WIFI_VALID, valid);
		
		server.send(200, FPSTR(STR_MIME_TEXT_HTML), F("<h1>All set!</h1><br /><p>(Reboot.)</p>"));
	
		EEPROM.commit();
		delay(300);
		ESP.restart();
		
	
		
	 }	  
			 
}

void handleApiPrinter() 
{
	StaticJsonBuffer<600> jsonBuffer;
	StaticJsonBuffer<600> jsonFlagsBuffer;
	JsonObject& state = jsonFlagsBuffer.createObject();
	JsonObject& flags = jsonFlagsBuffer.createObject();
	JsonObject& temp = jsonFlagsBuffer.createObject();
	JsonObject& bed = jsonFlagsBuffer.createObject();
	JsonObject& tool0 = jsonFlagsBuffer.createObject();
	JsonObject& tool1 = jsonFlagsBuffer.createObject();

	if(gPrinterInf.print_state == PRINTER_NOT_CONNECT)
	{
		flags["operational"] = false;
		flags["paused"] = false;
		flags["printing"] = false;
		flags["error"] = true;
		flags["ready"] = false;
		flags["sdReady"] = false;

		state["text"] = "Not connected";		
	}	
	else if(gPrinterInf.print_state == PRINTER_IDLE)
	{
		flags["operational"] = true;
		flags["paused"] = false;
		flags["printing"] = false;
		flags["error"] = false;
		flags["ready"] = true;
		flags["sdReady"] = false;

		
		state["text"] = "Operational";		
	}
	else if(gPrinterInf.print_state == PRINTER_PRINTING)
	{
		flags["operational"] = true;
		flags["paused"] = false;
		flags["printing"] = true;
		flags["error"] = false;
		flags["ready"] = true;
		flags["sdReady"] = false;

		
		state["text"] = "Printing";		
	}
	else if(gPrinterInf.print_state == PRINTER_PAUSE)
	{
		flags["operational"] = true;
		flags["paused"] = true;
		flags["printing"] = false;
		flags["error"] = false;
		flags["ready"] = true;
		flags["sdReady"] = false;

		
		state["text"] = "Pause";		
	}
	state["flags"] = flags;


	bed["actual"] = gPrinterInf.curBedTemp;
	bed["target"] = gPrinterInf.desireBedTemp;
	bed["offset"] = 0;

	tool0["actual"] = gPrinterInf.curSprayerTemp[0];
	tool0["target"] = gPrinterInf.desireSprayerTemp[0];
	tool0["offset"] = 0;

	tool1["actual"] = gPrinterInf.curSprayerTemp[1];
	tool1["target"] = gPrinterInf.desireSprayerTemp[1];
	tool1["offset"] = 0;

	temp["bed"] = bed;
	temp["tool0"] = tool0;
	temp["tool1"] = tool1;	

	
	JsonObject& root = jsonBuffer.createObject();
	root["state"] = state;
	root["temperature"] = temp; 

	
	
 	memset(jsBuffer, 0, sizeof(jsBuffer));
	
	root.printTo(jsBuffer, sizeof(jsBuffer));
	
 	server.send(200, FPSTR(STR_MIME_APPLICATION_JSON), FPSTR(jsBuffer));
		 
	
}

void handleApiPrinterCtrl()
{
	
	uint8_t readBuf[200] = {0};
	String gcodeStr = "";
	
	StaticJsonBuffer<200> jsonBuffer;
	uint32_t postLength = server.getPostLength();

	if((postLength > sizeof(readBuf)) || (postLength <= 0))
	{
		server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
		return;
	}

	if((!transfer_file_flag) && (transfer_state == TRANSFER_IDLE))
	{
		String gcodeStr = server.arg((const char *)"gcode");
		package_gcode(gcodeStr);
		transfer_state = TRANSFER_READY;
		digitalWrite(EspReqTransferPin, LOW);
		
		server.readPostdata(server.client(), readBuf, postLength);
			
		JsonObject& root = jsonBuffer.parseObject((char *)readBuf);
		if (!root.success())
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}
		
		const char* cmd    = root["command"];
		
		if(strcmp(cmd, "jog") == 0)
		{
			gcodeStr.concat("G91\nG1");
			
			if (root.containsKey("x"))
			{
				float x  = root["x"];
				gcodeStr.concat(" X");
				gcodeStr.concat(x);
			}
			if (root.containsKey("y"))
			{
				float y  = root["y"];
				gcodeStr.concat(" Y");
				gcodeStr.concat(y);
			
			}
			if (root.containsKey("z"))
			{
				float z  = root["z"];
				gcodeStr.concat(" Z");
				gcodeStr.concat(z);
			
			}
			gcodeStr.concat("\nG90");
			
		}
		else if(strcmp(cmd, "home") == 0)
		{
			if (root.containsKey("axes"))
			{
				JsonArray& nestedArray = root["axes"];
				if(nestedArray.size() == 3)
					gcodeStr.concat("G28");
				else
				{
					for(int i = 0; i < nestedArray.size(); i++)
					{
						const char* axes = nestedArray[i];
						gcodeStr.concat("G28 ");
						if(strcmp(axes, "x") == 0)
						{
							gcodeStr.concat("X0");
						}
						else if(strcmp(axes, "y") == 0)
						{
							gcodeStr.concat("Y0");
						}
						else if(strcmp(axes, "z") == 0)
						{
							gcodeStr.concat("Z0");
						}
					}
				}
				
			}
		}
		else
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}
		
		gcodeStr.concat('\n');
		package_gcode(gcodeStr);
		transfer_state = TRANSFER_READY;
		digitalWrite(EspReqTransferPin, LOW);

		server.send(204, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("No Content"));	

	}
	else
	{
		server.send(409, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("409 Conflict"));	
	}

	

}

void handleApiToolRelative()
{
	uint8_t readBuf[200] = {0};
	String gcodeStr = "";
	
	StaticJsonBuffer<200> jsonBuffer;
	uint32_t postLength = server.getPostLength();

	if((postLength > sizeof(readBuf)) || (postLength <= 0))
	{
		server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
		return;
	}
	if((!transfer_file_flag) && (transfer_state == TRANSFER_IDLE))
	{
	
		server.readPostdata(server.client(), readBuf, postLength);
			
		JsonObject& root = jsonBuffer.parseObject((char *)readBuf);
		if (!root.success())
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}
		
		const char* cmd    = root["command"];
		
		if(strcmp(cmd, "target") == 0)
		{		
			if (root.containsKey("targets"))
			{
				JsonObject& tg = root["targets"];
				if (!tg.success())
				{
					server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
				 	return;
				}
				if(tg.containsKey("tool0"))
				{
					float temp = tg["tool0"];
					gcodeStr.concat("M104 T0 S");
					gcodeStr.concat(temp);
					gcodeStr.concat('\n');
				}
				if(tg.containsKey("tool1"))
				{
					float temp = tg["tool1"];
					gcodeStr.concat("M104 T1 S");
					gcodeStr.concat(temp);
					gcodeStr.concat('\n');
				}
				
				
			}
			else
			{
				server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
				return;
			}
		}
		else if(strcmp(cmd, "select") == 0)
		{
			if (root.containsKey("tool"))
			{
				const char *_tool = root["tool"];
				if(strcmp(_tool, "tool0") == 0)
				{
					gcodeStr.concat("T0");
				}
				else if(strcmp(_tool, "tool1") == 0)
				{
					gcodeStr.concat("T1");
				}
				else
				{
					server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
					return;
				}
			}
		}
		else if(strcmp(cmd, "extrude") == 0)
		{
			if (root.containsKey("amount"))
			{
				float amount = root["amount"];
				gcodeStr.concat("G91\nG1 E");
				gcodeStr.concat(amount);
				gcodeStr.concat(" F300\nG90\n");
			}
			else
			{
				server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
				return;
			}
			
		}
		else  if(strcmp(cmd, "flowrate") == 0)
		{
			if (root.containsKey("factor"))
			{
				int fac = root["factor"];
				gcodeStr.concat("M221 S");
				gcodeStr.concat(fac);
			}
			else
			{
				server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
				return;
			}
		}
		else
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}
			
		gcodeStr.concat('\n');
		package_gcode(gcodeStr);
		transfer_state = TRANSFER_READY;
		digitalWrite(EspReqTransferPin, LOW);

		server.send(204, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("No Content"));	
	}
	else
	{
		server.send(409, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("409 Conflict"));	
	}
		
}

void handleApiBedRelative()
{
	uint8_t readBuf[200] = {0};
	String gcodeStr = "";
	
	StaticJsonBuffer<200> jsonBuffer;
	uint32_t postLength = server.getPostLength();

	if((postLength > sizeof(readBuf)) || (postLength <= 0))
	{
		server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
		return;
	}

	if((!transfer_file_flag) && (transfer_state == TRANSFER_IDLE))
	{
		server.readPostdata(server.client(), readBuf, postLength);
			
		JsonObject& root = jsonBuffer.parseObject((char *)readBuf);
		if (!root.success())
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}
		
		const char* cmd    = root["command"];
		
		if(strcmp(cmd, "target") == 0)
		{		
			if (root.containsKey("targets"))
			{
				float temp = root["targets"];
				gcodeStr.concat("M140 S");
				gcodeStr.concat(temp);			
			}
			else
			{
				server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
				return;
			}
		}
		else
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}
		gcodeStr.concat('\n');
		package_gcode(gcodeStr);
		transfer_state = TRANSFER_READY;
		digitalWrite(EspReqTransferPin, LOW);

		server.send(204, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("No Content"));	
	}
	else
	{
		server.send(409, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("409 Conflict"));	
	}
}

void handleApiSendCmd()
{
	uint8_t readBuf[200] = {0};
	String gcodeStr = "";
	
	StaticJsonBuffer<200> jsonBuffer;
	uint32_t postLength = server.getPostLength();

	if((postLength > sizeof(readBuf)) || (postLength <= 0))
	{
		server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
		return;
	}

	if((!transfer_file_flag) && (transfer_state == TRANSFER_IDLE))
	{
		server.readPostdata(server.client(), readBuf, postLength);
			
		JsonObject& root = jsonBuffer.parseObject((char *)readBuf);
		if (!root.success())
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}
		
		const char* cmd    = root["command"];
		
		gcodeStr.concat(cmd);
		gcodeStr.concat("\n");
		package_gcode(gcodeStr);
		transfer_state = TRANSFER_READY;
		digitalWrite(EspReqTransferPin, LOW);
		
		server.send(204, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("No Content"));	
	}
	else
	{
		server.send(409, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("409 Conflict"));	
	}
}

void handleApiChooseFileToPrint()
{
	uint8_t readBuf[200] = {0};
	String gcodeStr = "";
	
	StaticJsonBuffer<200> jsonBuffer;
	uint32_t postLength = server.getPostLength();

	String uri = server.uri();

	if(uri == NULL)
	{
		server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
		return;
	}


	if((postLength > sizeof(readBuf)) || (postLength <= 0))
	{
		server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
		return;
	}

	if((!transfer_file_flag) && (transfer_state == TRANSFER_IDLE))
	{
		server.readPostdata(server.client(), readBuf, postLength);
			
		JsonObject& root = jsonBuffer.parseObject((char *)readBuf);
		if (!root.success())
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}
		
		const char* cmd    = root["command"];

		if(strcmp(cmd, "select") == 0)
		{		
			
			gcodeStr.concat("M23 ");
			String vol = uri.substring(strlen("/api/files/"), uri.indexOf("/", strlen("/api/files/") + 1) + 1);

		/*	if(vol.startsWith("sd"))
			{
				gcodeStr.concat("1:");
			}
			else if(vol.startsWith("udisk"))
			{
				gcodeStr.concat("0:");
			}
			*/
			if((!vol.startsWith("sd")) && (!vol.startsWith("udisk")))
			{
				server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
				return;
			}
			String file1 = uri.substring(strlen("/api/files/") + vol.length());
			
			gcodeStr.concat(file1);
				
		
			if (root.containsKey("print"))
			{
				bool print = root["print"];
				if(print)
				{
					gcodeStr.concat("\nM24");	

					gPrinterInf.print_file_inf.file_name = "";
					gPrinterInf.print_file_inf.file_size = 0;
					gPrinterInf.print_file_inf.print_rate = 0;
					gPrinterInf.print_file_inf.print_hours = 0;
					gPrinterInf.print_file_inf.print_mins = 0;
					gPrinterInf.print_file_inf.print_seconds = 0;

					printFinishFlag = false;

					gPrinterInf.print_state = PRINTER_PRINTING;
					
				}
			}
		}
		else
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}		

		gcodeStr.concat("\n");
		package_gcode(gcodeStr);
		transfer_state = TRANSFER_READY;
		digitalWrite(EspReqTransferPin, LOW);

		server.send(200, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("OK"));
	}
	else
	{
		server.send(409, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("409 Conflict"));	
	}

}


	




void handleApiFileList()
{
	String uri = server.uri();
	String path;
	String gcodeStr;
	File dataFile;

	char *head_end;

	if(uri == NULL)
	{
		server.send(404, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Not Found"));	
		return;
	}

#if 1
	/*if(uri.startsWith("/api/files/local"))
	{
		path = uri.substring(strlen("/api/files/local"), uri.indexOf("/", strlen("/api/files/local") + 1));
		
	}
	else */if(uri.startsWith("/api/files/sdcard"))
	{
		path = uri.substring(strlen("/api/files/sdcard"));
		//gcodeStr = "M998 1\nM20 1:" + path;
		gcodeStr = "M20 1:" + path;
	}
/*	else if(uri.startsWith("/api/files/udisk"))
	{
		path = uri.substring(strlen("/api/files/udisk"), uri.indexOf("/", strlen("/api/files/udisk") + 1));
		gcodeStr = "M998 0\nM20 0:" + path;
	}*/
	else
	{
		server.send(404, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Not Found"));	
		return;
	}
	
	getting_file_flag = true;
	
	gcodeStr.concat("\n");
	package_gcode(gcodeStr);
	transfer_state = TRANSFER_READY;
	digitalWrite(EspReqTransferPin, LOW);

	

	uint8_t save_in_file = 0;
	
	int now = millis();

	int item_num = 0;
	
	do
	{
		
		
		do_transfer();

		int len = get_printer_reply();
		
		if(len > 0)
		{
			esp_data_parser((char *)uart_rcv_package, len);
		
			uart_rcv_index = 0;
		}


		if((save_in_file) || (gPrinterInf.sd_file_list.length() > LIST_MIN_LEN_SAVE_FILE))
		{
			if((save_in_file == 0))
			{
				dataFile = SPIFFS.open(F("/list_tree.txt"), "w+");
				head_end = "{\"files\":[";
				dataFile.write((const uint8_t *)head_end, strlen((const char *)head_end));

				save_in_file = 1;
			}

			if(gPrinterInf.sd_file_list.length() > 2)
			{
				String file1;
				if(gPrinterInf.sd_file_list.indexOf('\n') == -1)
				{
					file1 = gPrinterInf.sd_file_list;
				}
				else
				{
					file1 = gPrinterInf.sd_file_list.substring(0,gPrinterInf.sd_file_list.indexOf('\n'));
				}

		
				extract_file_item(dataFile, file1);

				if(dataFile.position() > LIST_MAX_LEN_SAVE_FILE)
				{
					break;
				}

				dataFile.write(',');
				

				gPrinterInf.sd_file_list = gPrinterInf.sd_file_list.substring(file1.length() + 1);
			}
			
		}

		if(!getting_file_flag)
		{
			if(save_in_file)
			{
				if(gPrinterInf.sd_file_list.length() > 2)
					continue;
				dataFile.seek(-1, SeekCur); // ','
			}
			if(gPrinterInf.sd_file_list.length() > LIST_MIN_LEN_SAVE_FILE)
			{
				continue;
			}
				
			break;
		}

		yield();

		
	} while(millis() - now < 8000); //8s timeout

	if(millis() - now >= 8000)
	{
		server.send(404, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Not Found"));	
		return;
	}
#endif

	if(save_in_file)
	{
		head_end = "],\"free\":\"10M\"}";
		dataFile.write((const uint8_t *)head_end, strlen((const char *)head_end));

		dataFile.seek(0, SeekSet);
		server.streamFile(dataFile, FPSTR(STR_MIME_APPLICATION_JSON));
		
		dataFile.close();

		gPrinterInf.sd_file_list.remove(0, gPrinterInf.sd_file_list.length());
	}
	else
	{
		String list = gPrinterInf.sd_file_list;
		while(list.indexOf('\n') != -1)
		{
			item_num++;
			list = list.substring(list.indexOf('\n') + 1);
		}
		const size_t bufferSize = JSON_ARRAY_SIZE(item_num) + (item_num + 1) * JSON_OBJECT_SIZE(2) + gPrinterInf.sd_file_list.length() + strlen("name") * item_num + strlen("typefile") * item_num + 3 * (item_num + 2) + strlen("files") + strlen("free10M") - item_num;
/*
		Serial.print("item_num:");
		Serial.println(item_num);
		Serial.print("strLen:");
		Serial.println(gPrinterInf.sd_file_list.length());
		Serial.print("bufferSize:");
		Serial.println(bufferSize);
	*/
		DynamicJsonBuffer rootJsonBuffer(bufferSize);
		//StaticJsonBuffer<1024> rootJsonBuffer;


		JsonObject& root = rootJsonBuffer.createObject();
		JsonArray& files = root.createNestedArray("files");
		root["free"] = "10M";
		
		if(gPrinterInf.sd_file_list.length() > LIST_MIN_LEN_SAVE_FILE)
		{
			gPrinterInf.sd_file_list.remove(LIST_MIN_LEN_SAVE_FILE, gPrinterInf.sd_file_list.length() - LIST_MIN_LEN_SAVE_FILE);
		}

		

		while(gPrinterInf.sd_file_list.length() > 2)
		{
			if(gPrinterInf.sd_file_list.indexOf('\n') == -1)
			{
				file1 = gPrinterInf.sd_file_list;
			}
			else
			{
				file1 = gPrinterInf.sd_file_list.substring(0,gPrinterInf.sd_file_list.indexOf('\n'));
			}

			JsonObject& item = rootJsonBuffer.createObject();
			item["name"] = file1;
			

			if(file1.endsWith(".DIR"))
			{
				item["type"] = "dir";
			}
			else
			{
				item["type"] = "file";
			}
			files.add(item);

			gPrinterInf.sd_file_list = gPrinterInf.sd_file_list.substring(file1.length() + 1);



			yield();
		//	optimistic_yield(10000);
		}

		memset(jsBuffer, 0, sizeof(jsBuffer));
		
		root.printTo(jsBuffer, sizeof(jsBuffer));

		server.send(200, FPSTR(STR_MIME_APPLICATION_JSON), FPSTR(jsBuffer));
	}
	

	
 //	
}

void handleApiPrint()
{
	uint8_t readBuf[200] = {0};
	String gcodeStr = "";
	
	StaticJsonBuffer<200> jsonBuffer;
	uint32_t postLength = server.getPostLength();

	
	if((postLength > sizeof(readBuf)) || (postLength <= 0))
	{
		server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
		return;
	}

	if((!transfer_file_flag) && (transfer_state == TRANSFER_IDLE))
	{
		server.readPostdata(server.client(), readBuf, postLength);
			
		JsonObject& root = jsonBuffer.parseObject((char *)readBuf);
		if (!root.success())
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}

		if (!root.containsKey("command"))
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}
		
		const char* cmd    = root["command"];
		
		
		if(strcmp(cmd, "cancel") == 0)
		{
			gcodeStr.concat("M26");
		}
		else if(strcmp(cmd, "pause") == 0)
		{		
			if (!root.containsKey("action"))
			{
				server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
				return;
			}
			const char* action    = root["action"];
			if(strcmp(action, "pause") == 0)
			{
				gcodeStr.concat("M25");
				gPrinterInf.print_state = PRINTER_PAUSE;
			}
			else if(strcmp(action, "resume") == 0)
			{
				gcodeStr.concat("M24");
				gPrinterInf.print_state = PRINTER_PRINTING;
			}
			else
			{
				server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
				return;
			}
				
		}
		else
		{
			server.send(400, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("Bad Request"));	
			return;
		}
		
		gcodeStr.concat("\n");
		package_gcode(gcodeStr);
		transfer_state = TRANSFER_READY;
		digitalWrite(EspReqTransferPin, LOW);
		
		server.send(204, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("No Content"));	
	}
	else
	{
		server.send(409, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("409 Conflict"));	
	}
}

void handleApiPrintInf()
{

	if((gPrinterInf.print_state == PRINTER_PRINTING) || (gPrinterInf.print_state == PRINTER_PAUSE))
	{
		StaticJsonBuffer<600> jsonBuffer;
	
		
		JsonObject& job = jsonBuffer.createObject();
		JsonObject& file = jsonBuffer.createObject();
		JsonObject& progress = jsonBuffer.createObject();
		
		file["name"] = gPrinterInf.print_file_inf.file_name.c_str();
		file["size"] = gPrinterInf.print_file_inf.file_size;

		progress["printTime"] = gPrinterInf.print_file_inf.print_hours * 3600 + gPrinterInf.print_file_inf.print_mins * 60 + gPrinterInf.print_file_inf.print_seconds;
		progress["filepos"] = gPrinterInf.print_file_inf.file_size * gPrinterInf.print_file_inf.print_rate;
		progress["completion"] = (float)gPrinterInf.print_file_inf.print_rate / 100.0;

		job["file"] = file;

		JsonObject& root = jsonBuffer.createObject();
		root["job"] = job;
		root["progress"] = progress;
		
		memset(jsBuffer, 0, sizeof(jsBuffer));
	
		root.printTo(jsBuffer, sizeof(jsBuffer));

		server.send(200, FPSTR(STR_MIME_APPLICATION_JSON), FPSTR(jsBuffer));

		if(gPrinterInf.print_file_inf.print_rate >= 100)
		{
			printFinishFlag = true;
		}
		
	}
	else
	{
	//	server.send(409, FPSTR(STR_MIME_TEXT_PLAIN), FPSTR("409 Conflict"));	
	}
	
}

void handleApiLogs()
{

	StaticJsonBuffer<1024> jsonBuffer;
	
	JsonObject& root = jsonBuffer.createObject();
	JsonObject& logs = jsonBuffer.createObject();
	JsonArray& tx = logs.createNestedArray("tx");
	JsonArray& rx = logs.createNestedArray("rx");
	

	int ind = monitor_tx_buf.indexOf('\n');
	while(1)
	{
		if(ind != -1)
		{
			tx.add(monitor_tx_buf.substring(0, ind));
			monitor_tx_buf.remove(0, ind + 1);
			 ind = monitor_tx_buf.indexOf('\n');
		}
		else 
			break;
	}

	ind = monitor_rx_buf.indexOf('\n');
	while(1)
	{
		if(ind != -1)
		{
			rx.add(monitor_rx_buf.substring(0, ind));
			monitor_rx_buf.remove(0, ind + 1);
			 ind = monitor_rx_buf.indexOf('\n');
		}
		else 
			break;
	}


	root["logs"] = logs;

	memset(jsBuffer, 0, sizeof(jsBuffer));
	
	root.printTo(jsBuffer, sizeof(jsBuffer));
	
 	server.send(200, FPSTR(STR_MIME_APPLICATION_JSON), FPSTR(jsBuffer));	

}



#endif
void handleRrUpload() {
}

#if 0
void rm_str_char(char *str, char del_char)
{
	char *p, *q;
	
	if(str == 0)
		return;
	
	for(p = str, q = str; *p != '\0'; p++)
	{
		if(*p != del_char)
		{
			*q++=*p;
		}
	}
	*q=*p;
}
#endif


void cloud_down_file()
{
//Todo
}


void cloud_get_file_list()
{
//Todo
}

void cloud_handler()
{
//Todo
}

void urldecode(String &input) { // LAL ^_^
  input.replace("%0A", String('\n'));
  input.replace("%20", " ");
  input.replace("+", " ");
  input.replace("%21", "!");
  input.replace("%22", "\"");
  input.replace("%23", "#");
  input.replace("%24", "$");
  input.replace("%25", "%");
  input.replace("%26", "&");
  input.replace("%27", "\'");
  input.replace("%28", "(");
  input.replace("%29", ")");
  input.replace("%30", "*");
  input.replace("%31", "+");
  input.replace("%2C", ",");
  input.replace("%2E", ".");
  input.replace("%2F", "/");
  input.replace("%2C", ",");
  input.replace("%3A", ":");
  input.replace("%3A", ";");
  input.replace("%3C", "<");
  input.replace("%3D", "=");
  input.replace("%3E", ">");
  input.replace("%3F", "?");
  input.replace("%40", "@");
  input.replace("%5B", "[");
  input.replace("%5C", "\\");
  input.replace("%5D", "]");
  input.replace("%5E", "^");
  input.replace("%5F", "-");
  input.replace("%60", "`");
  input.replace("%7B", "{");
  input.replace("%7D", "}");
}

String urlDecode(const String& text)
{
	String decoded = "";
	char temp[] = "0x00";
	unsigned int len = text.length();
	unsigned int i = 0;
	while (i < len)
	{
		char decodedChar;
		char encodedChar = text.charAt(i++);
		if ((encodedChar == '%') && (i + 1 < len))
		{
			temp[2] = text.charAt(i++);
			temp[3] = text.charAt(i++);

			decodedChar = strtol(temp, NULL, 16);
		}
		else {
			if (encodedChar == '+')
			{
				decodedChar = ' ';
			}
			else {
				decodedChar = encodedChar;  // normal ascii char
			}
		}
		decoded += decodedChar;
	}
	return decoded;
}


void urlencode(String &input) { // LAL ^_^
  input.replace(String('\n'), "%0A");
  input.replace(" " , "+");
  input.replace("!" , "%21");
  input.replace("\"", "%22" );
  input.replace("#" , "%23");
  input.replace("$" , "%24");
  //input.replace("%" , "%25");
  input.replace("&" , "%26");
  input.replace("\'", "%27" );
  input.replace("(" , "%28");
  input.replace(")" , "%29");
  input.replace("*" , "%30");
  input.replace("+" , "%31");
  input.replace("," , "%2C");
  //input.replace("." , "%2E");
  input.replace("/" , "%2F");
  input.replace(":" , "%3A");
  input.replace(";" , "%3A");
  input.replace("<" , "%3C");
  input.replace("=" , "%3D");
  input.replace(">" , "%3E");
  input.replace("?" , "%3F");
  input.replace("@" , "%40");
  input.replace("[" , "%5B");
  input.replace("\\", "%5C" );
  input.replace("]" , "%5D");
  input.replace("^" , "%5E");
  input.replace("-" , "%5F");
  input.replace("`" , "%60");
  input.replace("{" , "%7B");
  input.replace("}" , "%7D");
}

String fileUrlEncode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
    
}

String fileUrlEncode(char *array)
{
    String encodedString="";
	String str(array);
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
    
}

