#pragma once

#include <event2/event.h>

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <phosg/Filesystem.hh>

#include "PSOEncryption.hh"
#include "PSOProtocol.hh"
#include "ServerState.hh"



class ProxyServer {
public:
  ProxyServer() = delete;
  ProxyServer(const ProxyServer&) = delete;
  ProxyServer(ProxyServer&&) = delete;
  ProxyServer(
      std::shared_ptr<struct event_base> base,
      std::shared_ptr<ServerState> state);
  virtual ~ProxyServer() = default;

  void listen(uint16_t port, GameVersion version,
      const struct sockaddr_storage* default_destination = nullptr);

  void connect_client(struct bufferevent* bev, uint16_t server_port);

  struct LinkedSession {
    ProxyServer* server;
    uint64_t id;

    std::unique_ptr<struct event, void(*)(struct event*)> timeout_event;

    std::shared_ptr<const License> license;

    std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)> client_bev;
    std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)> server_bev;
    uint16_t local_port;
    struct sockaddr_storage next_destination;
    GameVersion version;
    uint32_t sub_version;
    std::string character_name;

    uint32_t guild_card_number;
    parray<uint8_t, 0x20> remote_client_config_data;
    ClientConfig newserv_client_config;
    bool suppress_newserv_commands;
    bool enable_chat_filter;
    int16_t override_section_id;
    int16_t override_lobby_event;
    int16_t override_lobby_number;

    struct LobbyPlayer {
      uint32_t guild_card_number;
      std::string name;
      LobbyPlayer() : guild_card_number(0) { }
    };
    std::vector<LobbyPlayer> lobby_players;
    size_t lobby_client_id;

    std::shared_ptr<PSOEncryption> client_input_crypt;
    std::shared_ptr<PSOEncryption> client_output_crypt;
    std::shared_ptr<PSOEncryption> server_input_crypt;
    std::shared_ptr<PSOEncryption> server_output_crypt;

    struct SavingFile {
      std::string basename;
      std::string output_filename;
      uint32_t remaining_bytes;
      std::unique_ptr<FILE, std::function<void(FILE*)>> f;

      SavingFile(
          const std::string& basename,
          const std::string& output_filename,
          uint32_t remaining_bytes);
    };
    std::unordered_map<std::string, SavingFile> saving_files;

    // TODO: This first constructor should be private
    LinkedSession(
        ProxyServer* server,
        uint64_t id,
        uint16_t local_port,
        GameVersion version);
    LinkedSession(
        ProxyServer* server,
        uint16_t local_port,
        GameVersion version,
        std::shared_ptr<const License> license,
        const ClientConfig& newserv_client_config);
    LinkedSession(
        ProxyServer* server,
        uint64_t id,
        uint16_t local_port,
        GameVersion version,
        const struct sockaddr_storage& next_destination);

    void resume(
        std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)>&& client_bev,
        std::shared_ptr<PSOEncryption> client_input_crypt,
        std::shared_ptr<PSOEncryption> client_output_crypt,
        uint32_t sub_version,
        const std::string& character_name);
    void resume(struct bufferevent* bev);
    void connect();

    static void dispatch_on_client_input(struct bufferevent* bev, void* ctx);
    static void dispatch_on_client_error(struct bufferevent* bev, short events,
        void* ctx);
    static void dispatch_on_server_input(struct bufferevent* bev, void* ctx);
    static void dispatch_on_server_error(struct bufferevent* bev, short events,
        void* ctx);
    static void dispatch_on_timeout(evutil_socket_t fd, short what, void* ctx);
    void on_client_input();
    void on_server_input();
    void on_stream_error(short events, bool is_server_stream);
    void on_timeout();

    void send_to_end(const void* data, size_t size, bool to_server);
    void send_to_end(const std::string& data, bool to_server);

    void disconnect();

    bool is_open() const;
  };

  std::shared_ptr<LinkedSession> get_session();
  std::shared_ptr<LinkedSession> create_licensed_session(
    std::shared_ptr<const License> l,
    uint16_t local_port,
    GameVersion version,
    const ClientConfig& newserv_client_config);
  void delete_session(uint64_t id);

  bool save_files;

private:
  struct ListeningSocket {
    ProxyServer* server;

    uint16_t port;
    scoped_fd fd;
    std::unique_ptr<struct evconnlistener, void(*)(struct evconnlistener*)> listener;
    GameVersion version;
    struct sockaddr_storage default_destination;

    ListeningSocket(
        ProxyServer* server,
        uint16_t port,
        GameVersion version,
        const struct sockaddr_storage* default_destination);

    static void dispatch_on_listen_accept(struct evconnlistener* listener,
        evutil_socket_t fd, struct sockaddr *address, int socklen, void* ctx);
    static void dispatch_on_listen_error(struct evconnlistener* listener, void* ctx);
    void on_listen_accept(int fd);
    void on_listen_error();
  };

  struct UnlinkedSession {
    ProxyServer* server;

    std::unique_ptr<struct bufferevent, void(*)(struct bufferevent*)> bev;
    uint16_t local_port;
    GameVersion version;

    std::shared_ptr<PSOEncryption> crypt_out;
    std::shared_ptr<PSOEncryption> crypt_in;

    UnlinkedSession(ProxyServer* server, struct bufferevent* bev, uint16_t port, GameVersion version);

    void receive_and_process_commands();

    static void dispatch_on_client_input(struct bufferevent* bev, void* ctx);
    static void dispatch_on_client_error(struct bufferevent* bev, short events,
        void* ctx);
    void on_client_input();
    void on_client_error(short events);
  };

  std::shared_ptr<struct event_base> base;
  std::shared_ptr<ServerState> state;
  std::map<int, std::shared_ptr<ListeningSocket>> listeners;
  std::unordered_map<struct bufferevent*, std::shared_ptr<UnlinkedSession>> bev_to_unlinked_session;
  std::unordered_map<uint64_t, std::shared_ptr<LinkedSession>> id_to_session;
  uint64_t next_unlicensed_session_id;

  void on_client_connect(
      struct bufferevent* bev,
      uint16_t listen_port,
      GameVersion version,
      const struct sockaddr_storage* default_destination);
};
