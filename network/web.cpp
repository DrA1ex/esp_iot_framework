#include "web.h"

class WebLogger : public AsyncWebHandler {
    bool canHandle(AsyncWebServerRequest *request) override {
        D_PRINTF("WebServer: %s -> %s %s\n", request->client()->remoteIP().toString().c_str(),
                 request->methodToString(), request->url().c_str());

        return false;
    }
};

WebServer::WebServer(uint16_t port) : _port(port), _server(port) {
#if DEBUG
    _server.addHandler(new WebLogger());
#endif
}

void WebServer::begin(FS *fs) {
    _fs = fs;

    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        request->send(*_fs, "/index.html", "text/html");
    });

    _server.serveStatic("/", *_fs, "/");

    _server.begin();

    D_WRITE("Web server listening on port: ");
    D_PRINT(_port);
}

WebAuthHandler::WebAuthHandler(const char *user, const char *password, bool allow_local) :
        _user(user), _password(password), _allow_local(allow_local) {}

void WebAuthHandler::handleRequest(AsyncWebServerRequest *request) {
    D_PRINTF("Reject request from: %s\n", request->client()->remoteIP().toString().c_str());

    request->redirect("https://google.com");
}

bool WebAuthHandler::canHandle(AsyncWebServerRequest *request) {
    bool is_local = (request->client()->getRemoteAddress() & PP_HTONL(0xffff0000UL)) == PP_HTONL(0xc0a80000UL);
    bool auth_required = _allow_local ? !is_local : true;

    return auth_required && !request->authenticate(_user, _password);
}
