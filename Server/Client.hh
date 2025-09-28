#pragma once

#include <Shared/Binary.hh>
#include <Shared/EntityDef.hh>

#include <cstdint>
#include <set>
#include <string>

#ifdef WASM_SERVER
class WebSocket;
#else
#include <App.h>
class Client;
typedef uWS::WebSocket<false, true, Client> WebSocket;
#endif

class GameInstance;

class Client {
public:
    GameInstance *game;
    EntityID camera;
    std::set<EntityID> in_view;
    WebSocket *ws;
    uint8_t verified = 0;
    uint8_t seen_arena = 0;
    Client();
    void init();
    void remove();
    void disconnect(int = CloseReason::kProtocol, std::string const & = "Protocol Error");
    uint8_t alive();
    bool isAdmin;
    void send_packet(uint8_t const *, size_t);
    float mouse_world_x = 0.0f;
    float mouse_world_y = 0.0f;
    //takes in a bool expr
    //if true, packet reading should be terminated
    //optionally, the client canalso be disconnected
    bool check_invalid(bool);
    static void on_message(WebSocket *, std::string_view, uint64_t);
    static void command(Client* client, std::string const& text, float mouse_x, float mouse_y);
    static void on_disconnect(WebSocket *, int, std::string_view);
};

#ifdef WASM_SERVER
class WebSocket {
    int ws_id;
public:
    Client client;
    WebSocket(int);
    Client *getUserData();
    void send(uint8_t const *, size_t);
    void end(int, std::string const &);
};
#endif