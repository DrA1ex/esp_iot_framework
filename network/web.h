#pragma once

#include "ESPAsyncWebServer.h"
#include "FS.h"

#include "../debug.h"

class WebServer {
    uint16_t _port;
    AsyncWebServer _server;

    FS *_fs = nullptr;

public:
    explicit WebServer(uint16_t port = 80);

    void begin(FS *fs);

    inline void on(const char *uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest) {
        _server.on(uri, method, std::move(onRequest));
    }

    inline void add_handler(AsyncWebHandler *handler) { _server.addHandler(handler); }

    inline uint16_t port() const { return _port; }
};

class WebAuthHandler : public AsyncWebHandler {
private:
    const char *_user;
    const char *_password;
    bool _allow_local;

public:
    WebAuthHandler(const char *user, const char *password, bool allow_local = true);

    inline void set_allow_local(bool value) { _allow_local = value; }

    virtual bool canHandle(AsyncWebServerRequest *request) const override final;
    virtual void handleRequest(AsyncWebServerRequest *request) override final;
};