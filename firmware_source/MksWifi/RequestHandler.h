#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H

#define UNUSED(_x) (void)(_x)

class RequestHandler {
public:
    virtual ~RequestHandler() {}
    virtual bool canHandle(HTTPMethod method, String uri)
    {
      UNUSED(method);
      UNUSED(uri);
      return false; 
    }
    
    virtual bool canUpload(String uri)
    { 
      UNUSED(uri);
      return false;
    }

    virtual bool handle(RepRapWebServer& server, HTTPMethod requestMethod, String requestUri)
    {
      UNUSED(server);
      UNUSED(requestMethod);
      UNUSED(requestUri);
      return false;
    }

    virtual void upload(RepRapWebServer& server, String requestUri, HTTPUpload& upload)
    {
      UNUSED(server);
      UNUSED(requestUri);
      UNUSED(upload);
    }

    RequestHandler* next() { return _next; }
    void next(RequestHandler* r) { _next = r; }

private:
    RequestHandler* _next = nullptr;
};

#endif //REQUESTHANDLER_H
