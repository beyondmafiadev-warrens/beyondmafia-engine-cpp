#pragma once
#include <memory>
#include <deque>
#include <boost/beast/core/tcp_stream.hpp>
#include<boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <boost/scoped_ptr.hpp>
#include "Game.h"
#include "Database.h"
#include "DatabaseConfig.h"
namespace gameServer {
    class personInRoom;
    class listener;
    class session;
    namespace net = boost::asio;            // from <boost/asio.hpp>
    namespace beast = boost::beast;         // from <boost/beast.hpp>
    namespace http = beast::http;           // from <boost/beast/http.hpp>
    namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
    using boost::asio::ip::tcp;

    class socketThread
    {
    public:
        static void run(std::shared_ptr<boost::asio::io_context> io_service)
        {
            try {
                io_service->run();
            }

            catch (boost::system::system_error const& e) {

            }
        }
    };

    class session : public std::enable_shared_from_this<session>
    {
        std::unique_ptr<websocket::stream<beast::tcp_stream>> ws_;
        beast::flat_buffer buffer_;
        std::shared_ptr<personInRoom> client_;
        std::deque<std::string> write_msgs_;
        boost::asio::io_context& ioc_;
        boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;
        http::request<http::string_body> upgrade_request_;
        bool close_ = false;

    public:
        // Take ownership of the socket

        session(tcp::socket&& socket, std::shared_ptr<personInRoom> client, boost::asio::io_context& ioc);
        // Get on the correct executor
        void
            run();
        // Start the asynchronous operation
        void
            fail(beast::error_code ec, char const* what)
        {
            std::cerr << what << ": " << ec.message() << "\n";
        }
        void
            on_run();

        void
            on_accept(beast::error_code ec);

        void
            do_read();
        void
            on_read(
                beast::error_code ec,
                std::size_t bytes_transferred);
        void send(std::string message);
        void on_send(std::string message);
        void
            on_write(
                beast::error_code ec,
                std::size_t bytes_transferred);
        void
            on_close(beast::error_code ec);
        void
            on_upgrade(beast::error_code ec, size_t);
        void close();
        void closeWebsocket();

    };
    class listener : public std::enable_shared_from_this<listener>
    {
        std::shared_ptr<boost::asio::io_context> ioc_;
        tcp::acceptor acceptor_;
        std::shared_ptr<personInRoom> client_;
        std::shared_ptr<session> listener_;
    public:
        listener(
            std::shared_ptr<boost::asio::io_context> ioc,
            tcp::endpoint endpoint,
            std::shared_ptr<personInRoom> person);

        // Start accepting incoming connections
        void
            run();
        void closeAcceptor();
        void clearWebsocket();
        int getPort();
        std::shared_ptr<session> getWebsocket();
    private:
        void
            do_accept();

        void
            on_accept(beast::error_code ec, tcp::socket socket);
        bool acceptorClosed_;
        int port_;
    };

    class personInRoom : public chat::participant,
        public std::enable_shared_from_this<personInRoom>
    {
    public:
        personInRoom(boost::asio::io_context& io_service,
            boost::asio::io_service::strand& strand, chat::chatRoom& room, uint64_t port, game::Game& game, std::shared_ptr<db::database> database);

        tcp::socket& socket();
        void start();
        void onMessage(std::array<char, MAX_IP_PACK_SIZE>& msg);
        void sendWebSocketMessage(std::array<char, MAX_IP_PACK_SIZE>& msg);
        void sendGameDetails();
        void sendPlayerId();
        void joinRoom();
        bool verifyUser(std::string cookieString);
        void addPlayer();
        void createWebsocket();
        void insertWebsocket(int port);
        void deleteSocket();
        bool verified = false; 
    private:
        void startHandler(const boost::system::error_code& error);
        void parseWebsocketMessage(boost::json::value& val, std::array<char, MAX_IP_PACK_SIZE>& msg);
        std::shared_ptr<cli::client> createClient(int playerId);
        game::Game& game_;
        int parseMessage(boost::json::value& val);
        void readHandler(const boost::system::error_code& error);
        void writeHandler(const boost::system::error_code& error);
        boost::asio::io_context& io_service_;
        uint64_t port_;
        tcp::socket socket_;
        boost::asio::io_context::strand& strand_;
        boost::shared_mutex voteMutex;
        chat::chatRoom& room_;
        std::array<char, MAX_IP_PACK_SIZE> read_msg_;
        std::deque<std::array<char, MAX_IP_PACK_SIZE> > write_msgs_;
        int userId_;
        std::shared_ptr<listener> listener_;
        std::shared_ptr<db::database> database_;
        std::shared_ptr<boost::thread> t;
    };
};
