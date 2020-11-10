/*
  ESP8266WebServer.cpp - Dead simple web-server.
  Supports only one simultaneous client, knows how to handle GET and POST.

  Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  Modified 8 May 2015 by Hristo Gochkov (proper post and file upload handling)
*/


#include <Arduino.h>
#include <libb64/cencode.h>
#include "WiFiServer.h"
#include "WiFiClient.h"
#include "RepRapWebServer.h"
#include "FS.h"
#include "RequestHandlersImpl.h"
#undef DEBUG
#define DEBUG_OUTPUT Serial


RepRapWebServer::RepRapWebServer(IPAddress addr, int port)
: _server(addr, port)
, _currentMethod(HTTP_ANY)
, _currentVersion(0)
, _currentStatus(HC_NONE)
, _statusChange(0)
, _currentHandler(0)
, _firstHandler(0)
, _lastHandler(0)
, _currentArgCount(0)
, _currentArgs(0)
, _headerKeysCount(0)
, _currentHeaders(0)
, _contentLength(0)
, _postLength(0)
, _servingPrinter(false)
, _chunked(false)
{
}

RepRapWebServer::RepRapWebServer(int port)
: _server(port)
, _currentMethod(HTTP_ANY)
, _currentVersion(0)
, _currentStatus(HC_NONE)
, _statusChange(0)
, _currentHandler(0)
, _firstHandler(0)
, _lastHandler(0)
, _currentArgCount(0)
, _currentArgs(0)
, _headerKeysCount(0)
, _currentHeaders(0)
, _contentLength(0)
, _postLength(0)
, _servingPrinter(false)
, _chunked(false)
{
}

RepRapWebServer::~RepRapWebServer() {
  if (_currentHeaders)
    delete[]_currentHeaders;
  _headerKeysCount = 0;
  RequestHandler* handler = _firstHandler;
  while (handler) {
    RequestHandler* next = handler->next();
    delete handler;
    handler = next;
  }
  close();
}

void RepRapWebServer::begin() {
  _currentStatus = HC_NONE;
  _server.begin();
  if(!_headerKeysCount)
    collectHeaders(0, 0);
}





void RepRapWebServer::on(const char* uri, RepRapWebServer::THandlerFunction handler) {
  on(uri, HTTP_ANY, handler);
}

void RepRapWebServer::on(const char* uri, HTTPMethod method, RepRapWebServer::THandlerFunction fn) {
  on(uri, method, fn, _fileUploadHandler);
}

void RepRapWebServer::on(const char* uri, HTTPMethod method, RepRapWebServer::THandlerFunction fn, RepRapWebServer::THandlerFunction ufn) {
  _addRequestHandler(new FunctionRequestHandler(fn, ufn, uri, method));
}

void RepRapWebServer::onPrefix(const char* prefix, HTTPMethod method, RepRapWebServer::THandlerFunction fn) {
  onPrefix(prefix, method, fn, _fileUploadHandler);
}

void RepRapWebServer::onPrefix(const char* prefix, HTTPMethod method, RepRapWebServer::THandlerFunction fn, RepRapWebServer::THandlerFunction ufn) {
  _addRequestHandler(new PrefixRequestHandler(fn, ufn, prefix, method));
}


void RepRapWebServer::addHandler(RequestHandler* handler) {
    _addRequestHandler(handler);
}

void RepRapWebServer::_addRequestHandler(RequestHandler* handler) {
    if (!_lastHandler) {
      _firstHandler = handler;
      _lastHandler = handler;
    }
    else {
      _lastHandler->next(handler);
      _lastHandler = handler;
    }
}

void RepRapWebServer::serveStatic(const char* uri, FS& fs, const char* path, const char* cache_header) {
    _addRequestHandler(new StaticRequestHandler(fs, path, uri, cache_header));
}

void RepRapWebServer::handleClient() {
	
  if (_currentStatus == HC_NONE) {
    WiFiClient client = _server.available();
  if (!client) {
    return;
  }

#ifdef DEBUG
  DEBUG_OUTPUT.println("New client");
#endif

    _currentClient = client;
    _currentStatus = HC_WAIT_READ;
    _statusChange = millis();
  }

  if (!_currentClient.connected()) {
    _currentClient = WiFiClient();
    _currentStatus = HC_NONE;
    return;
  }

  // Wait for data from client to become available
  if (_currentStatus == HC_WAIT_READ) {
    if (!_currentClient.available()) {
      if (millis() - _statusChange > HTTP_MAX_DATA_WAIT) {
        _currentClient = WiFiClient();
        _currentStatus = HC_NONE;
      }
      yield();
      return;
    }

  size_t postLength;
  if (!_parseRequest(_currentClient, postLength)) {
      _currentClient = WiFiClient();
      _currentStatus = HC_NONE;
    return;
  }
    _currentClient.setTimeout(HTTP_MAX_SEND_WAIT);
  
  _postLength = postLength;
  _contentLength = CONTENT_LENGTH_NOT_SET;
  _handleRequest();
    if (!_currentClient.connected()) {
      _currentClient = WiFiClient();
      _currentStatus = HC_NONE;
      return;
    } else {
      _currentStatus = HC_WAIT_CLOSE;
      _statusChange = millis();
      return;
    }
  }

  if (_currentStatus == HC_WAIT_CLOSE) {
    if (millis() - _statusChange > HTTP_MAX_CLOSE_WAIT) {
      _currentClient = WiFiClient();
      _currentStatus = HC_NONE;
    } else {
      yield();
      return;
    }
  }
}

void RepRapWebServer::close() {
  _server.close();
}

void RepRapWebServer::stop() {
  close();
}
void RepRapWebServer::sendHeader(const String& name, const String& value, bool first) {
  String headerLine = name;
  headerLine += ": ";
  headerLine += value;
  headerLine += "\r\n";

  if (first) {
    _responseHeaders = headerLine + _responseHeaders;
  }
  else {
    _responseHeaders += headerLine;
  }
}


void RepRapWebServer::_prepareHeader(String& response, int code, const char* content_type, size_t contentLength)
{
    response = "HTTP/1."+String(_currentVersion)+" ";
    response += String(code);
    response += " ";
    response += _responseCodeToString(code);
    response += "\r\nCache-Control: no-cache, no-store, must-revalidate\r\nPragma: no-cache\r\nExpires: 0\r\n";

    if (!content_type)
    {
        content_type = "text/html";
    }
    sendHeader("Content-Type", content_type, true);

    if (_contentLength == CONTENT_LENGTH_NOT_SET) {
        sendHeader("Content-Length", String(contentLength));
    } else if (_contentLength != CONTENT_LENGTH_UNKNOWN && _contentLength != CONTENT_LENGTH_NOT_SET)
    {
        sendHeader("Content-Length", String(_contentLength));
    } else if(_contentLength == CONTENT_LENGTH_UNKNOWN && _currentVersion){ //HTTP/1.1 or above client
      //let's do chunked
      _chunked = true;
      sendHeader("Accept-Ranges","none");
      sendHeader("Transfer-Encoding","chunked");
    }
    sendHeader("Connection", "close");

    response += _responseHeaders;
    response += "\r\n";
    _responseHeaders = String();
}

void RepRapWebServer::send(int code, size_t contentLength, const __FlashStringHelper *contentType, const uint8_t *data, size_t dataLength, bool isLast)
{
    String header;
    String contentTypeStr(contentType);
    _prepareHeader(header, code, contentTypeStr.c_str(), contentLength);
    sendContent(header);
    if (dataLength != 0)
    {
      sendContent(data, dataLength);
    }
}

void RepRapWebServer::sendMore(const uint8_t *data, size_t dataLength, bool isLast)
{
    sendContent(data, dataLength);    
}

void RepRapWebServer::send(int code, const char* content_type, const String& content)
{
    String header;
    _prepareHeader(header, code, content_type, content.length());
    _currentClient.write(header.c_str(), header.length());
    if(content.length())
      sendContent(content);
}

void RepRapWebServer::send_P(int code, PGM_P content_type, PGM_P content)
{
    size_t contentLength = 0;

    if (content != NULL)
    {
        contentLength = strlen_P(content);
    }

    String header;
    char type[64];
    memccpy_P((void*)type, (PGM_VOID_P)content_type, 0, sizeof(type));
    _prepareHeader(header, code, (const char* )type, contentLength);
    _currentClient.write(header.c_str(), header.length());
    sendContent_P(content);
}

void RepRapWebServer::send_P(int code, PGM_P content_type, PGM_P content, size_t contentLength) {
    String header;
    char type[64];
    memccpy_P((void*)type, (PGM_VOID_P)content_type, 0, sizeof(type));
    _prepareHeader(header, code, (const char* )type, contentLength);
    sendContent(header);
    sendContent_P(content, contentLength);
}

void RepRapWebServer::send(int code, char* content_type, const String& content)
{
  send(code, (const char*)content_type, content);
}

void RepRapWebServer::send(int code, const String& content_type, const String& content)
{
  send(code, (const char*)content_type.c_str(), content);
}

void RepRapWebServer::sendContent(const uint8_t *content, size_t dataLength)
{
  const char * footer = "\r\n";
  size_t len = dataLength;
  if(_chunked) {
    char * chunkSize = (char *)malloc(11);
    if(chunkSize){
      sprintf(chunkSize, "%x%s", len, footer);
      _currentClient.write((const uint8_t *)chunkSize, strlen(chunkSize), false);
      free(chunkSize);
    }
  }
  _currentClient.write(content, len);
  if(_chunked){
    _currentClient.write(footer, 2);
  }
}

void RepRapWebServer::sendContent(const String& content)
{
  sendContent((const uint8_t*)content.c_str(), content.length());
}

void RepRapWebServer::sendContent_P(PGM_P content)
{
   sendContent_P(content, strlen_P(content));
}

void RepRapWebServer::sendContent_P(PGM_P content, size_t size)
{
   const char * footer = "\r\n";
  if(_chunked) {
    char * chunkSize = (char *)malloc(11);
    if(chunkSize){
      sprintf(chunkSize, "%x%s", size, footer);
      _currentClient.write((const uint8_t *)chunkSize, strlen(chunkSize), false);
      free(chunkSize);
    }
  }
  _currentClient.write_P(content, size, false);
  if(_chunked){
    _currentClient.write(footer, 2);
  }
}

String RepRapWebServer::arg(const char* name) {
  for (int i = 0; i < _currentArgCount; ++i) {
    if (_currentArgs[i].key == name)
      return _currentArgs[i].value;
  }
  return String();
}

String RepRapWebServer::arg(int i) {
  if (i < _currentArgCount)
    return _currentArgs[i].value;
  return String();
}

String RepRapWebServer::argName(int i) {
  if (i < _currentArgCount)
    return _currentArgs[i].key;
  return String();
}

int RepRapWebServer::args() {
  return _currentArgCount;
}

bool RepRapWebServer::hasArg(const char* name) {
  for (int i = 0; i < _currentArgCount; ++i) {
    if (_currentArgs[i].key == name)
      return true;
  }
  return false;
}

String RepRapWebServer::header(const char* name) {
  for (int i = 0; i < _headerKeysCount; ++i) {
    if (_currentHeaders[i].key.equalsIgnoreCase(name))
      return _currentHeaders[i].value;
  }
  return String();
}

void RepRapWebServer::collectHeaders(const char* headerKeys[], const size_t headerKeysCount) {
  _headerKeysCount = headerKeysCount;
  if (_currentHeaders)
     delete[]_currentHeaders;
  _currentHeaders = new RequestArgument[_headerKeysCount];
  for (int i = 0; i < _headerKeysCount; i++){
    _currentHeaders[i].key = headerKeys[i];
  }
}

String RepRapWebServer::header(int i) {
  if (i < _headerKeysCount)
    return _currentHeaders[i].value;
  return String();
}

String RepRapWebServer::headerName(int i) {
  if (i < _headerKeysCount)
    return _currentHeaders[i].key;
  return String();
}

int RepRapWebServer::headers() {
  return _headerKeysCount;
}

bool RepRapWebServer::hasHeader(const char* name) {
  for (int i = 0; i < _headerKeysCount; ++i) {
    if ((_currentHeaders[i].key.equalsIgnoreCase(name)) &&  (_currentHeaders[i].value.length() > 0))
      return true;
  }
  return false;
}

String RepRapWebServer::hostHeader() {
  return _hostHeader;
}

void RepRapWebServer::onFileUpload(THandlerFunction fn) {
  _fileUploadHandler = fn;
}

void RepRapWebServer::onNotFound(THandlerFunction fn) {
  _notFoundHandler = fn;
}

void RepRapWebServer::_handleRequest() {
  bool handled = false;
  if (!_currentHandler){
#ifdef DEBUG
    DEBUG_OUTPUT.println("request handler not found");
#endif
  }
  else {
    handled = _currentHandler->handle(*this, _currentMethod, _currentUri);
#ifdef DEBUG
    if (!handled) {
      DEBUG_OUTPUT.println("request handler failed to handle request");
    }
#endif
  }

  if (!handled) {
    if(_notFoundHandler) {
      _notFoundHandler();
    }
    else {
      send(404, "text/plain", String("Not found: ") + _currentUri);
    }
  }

  _currentUri = String();
}

const char* RepRapWebServer::_responseCodeToString(int code) {
  switch (code) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-Authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 307: return "Temporary Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Time-out";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Request Entity Too Large";
    case 414: return "Request-URI Too Large";
    case 415: return "Unsupported Media Type";
    case 416: return "Requested range not satisfiable";
    case 417: return "Expectation Failed";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Time-out";
    case 505: return "HTTP Version not supported";
    default:  return "";
  }
}
