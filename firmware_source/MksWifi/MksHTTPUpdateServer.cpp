#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include "RepRapWebServer.h"

#include "MksHTTPUpdateServer.h"

#if 1
const char* MksHTTPUpdateServer::_serverIndex =
R"(<html><body><form method='POST' action='update_web' enctype='multipart/form-data'>
                  <input type='file' name='update'>
                  <input type='submit' value='Update'>
               </form>
         </body></html>)";
#endif
const char* MksHTTPUpdateServer::_failedResponse = R"(Update failed!)";
const char* MksHTTPUpdateServer::_successResponse = "<META http-equiv=\"refresh\" content=\"15;URL=\">Update successfully! Rebooting...";

extern boolean transfer_file_flag;

MksHTTPUpdateServer::MksHTTPUpdateServer(bool serial_debug)
{
  _serial_output = serial_debug;
  _server = NULL;
  _username = NULL;
  _password = NULL;
  _authenticated = false;
}
extern "C" uint32_t _SPIFFS_start;
extern "C" uint32_t _SPIFFS_end;

char update_wifi_firmware[] = "update_wifi_firmware";

typedef enum
{
	UPDATE_UNKNOW_ERROR,
	UPDATE_FILE_ERROR,
	UPDATE_COMM_ERROR,
	UPDATE_SUCCESS,
} UPDATE_RESULT;

UPDATE_RESULT Update_result = UPDATE_UNKNOW_ERROR;;
	
//void ESP8266HTTPUpdateServer::setup(ESP8266WebServer *server, const char * path, const char * username, const char * password)
void MksHTTPUpdateServer::setup(RepRapWebServer *server,  const char * username, const char * password)
{
	
	
    _server = server;
    _username = (char *)username;
    _password = (char *)password;
#if 1
    // handler for the /update form page
    _server->onPrefix("/update_web", HTTP_GET, [&](){
    //  if(_username != NULL && _password != NULL && !_server->authenticate(_username, _password))
     //   return _server->requestAuthentication();
      _server->send(200, "text/html", _serverIndex);
    });
#endif

    // handler for the /update form POST (once file upload finishes)
    _server->onPrefix("/update_", HTTP_POST, [&](){
     /* if(!_authenticated)
        return _server->requestAuthentication();*/
        if(Update_result == UPDATE_UNKNOW_ERROR)
        {
        	 _server->send(200, "text/html", _failedResponse );
	
        }
	else if(Update_result == UPDATE_FILE_ERROR)
	{
        	 _server->send(200, "text/html", "File error");
		
        }
	else
	{
        	 _server->send(200, "text/html", Update.hasError() ? _failedResponse : _successResponse);
		
        }
	delay(2000);
      	ESP.restart();
     
	
    },[&]()
	    {
	      // handler for the file upload, get's the sketch bytes, and writes
	      // them through the Update object
	      HTTPUpload& upload = _server->upload();
	
		//Serial.printf("_SPIFFS_start: 0x%x\n", _SPIFFS_start);
		// Serial.printf("_SPIFFS_end: 0x%x\n", _SPIFFS_end);
	      if(upload.status == UPLOAD_FILE_START)
		{
			if (_serial_output)
				Serial.setDebugOutput(true);

			/*     _authenticated = (_username == NULL || _password == NULL || _server->authenticate(_username, _password));
			if(!_authenticated){
			if (_serial_output)
			Serial.printf("Unauthenticated Update\n");
			return;
			}*/

			if((!upload.filename.startsWith("MksWifi.bin")) 
				&& (!upload.filename.startsWith("MksWifi_Web.bin"))  
				&& (!upload.filename.startsWith("MksWifi_WebView.bin")))
			{
				Update_result = UPDATE_FILE_ERROR;
				upload.status == UPLOAD_FILE_ABORTED;
			
				return;
			}

			WiFiUDP::stopAll();
			if (_serial_output)
				Serial.printf("Update: %s\n", upload.filename.c_str());
		
			uint32_t maxSketchSpace;	

			String uri = _server->uri();
		//	Serial.printf("Uri: %s\n", uri.c_str());

			bool res = false;

			
			
			/*if(hidden_ssid[0] != 0)
			{
				memset(ssid, 0, sizeof(ssid));
				memcpy(ssid, hidden_ssid, sizeof(hidden_ssid));
			}*/
			
			
			if(uri.startsWith("/update_sketch") )
			{
				
				if(upload.filename.startsWith("MksWifi.bin") || upload.filename.startsWith("MksWifi_Web.bin"))
				{
					maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
				//	Serial.printf("maxSketchSpace: 0x%x\n", maxSketchSpace);
					res = Update.begin(maxSketchSpace);
				}
				else
				{
					Update_result = UPDATE_FILE_ERROR;
					upload.status == UPLOAD_FILE_ABORTED;
					return;
				}
			}
			else if(uri.startsWith("/update_spiffs") ||uri.startsWith("/update_web") )
			{	
				if(upload.filename.startsWith("MksWifi_WebView.bin"))
					res = Update.begin(3 * 1024 * 1024, U_SPIFFS);
				else
				{
					Update_result = UPDATE_FILE_ERROR;
					upload.status == UPLOAD_FILE_ABORTED;
					
					
					return;
				}
			}
			else
			{
				Update_result = UPDATE_COMM_ERROR;
				upload.status == UPLOAD_FILE_ABORTED;
				return;
			}

			transfer_file_flag = true;

		//	Serial.printf("Ready to write\n");

			if(!res)
			{//start with max available size
				if (_serial_output) 
					Update.printError(Serial);
			}
			// _server->send(200, "text/html", "Please wait.  Updating......");
	      } //else if(_authenticated && upload.status == UPLOAD_FILE_WRITE){
	      else if(upload.status == UPLOAD_FILE_WRITE)
		{
			if (_serial_output) 
				Serial.printf(".");
		
			if(Update.write(upload.buf, upload.currentSize) != upload.currentSize)
			{
				if (_serial_output) 
					Update.printError(Serial);

			}
	      } //else if(_authenticated && upload.status == UPLOAD_FILE_END){
	      else if(upload.status == UPLOAD_FILE_END)
		{
			if(Update.end(true))
			{ //true to set the size to the current progress
				Update_result = UPDATE_SUCCESS;
				if (_serial_output) 
					Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
			} 
			else 
			{
				if (_serial_output) 
					Update.printError(Serial);
			}
			if (_serial_output) 
				Serial.setDebugOutput(false);
		}// else if(_authenticated && upload.status == UPLOAD_FILE_ABORTED){
		else if( upload.status == UPLOAD_FILE_ABORTED)
		{
			Update.end();
			if (_serial_output) 
				Serial.println("Update was aborted");
	  	}
	      delay(0);
	    });
}
