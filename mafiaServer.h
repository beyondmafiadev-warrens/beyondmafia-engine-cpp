#pragma once
#include <ctime>
#include <string>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>
#include "PersonInRoom.h"
namespace clientServer {
    using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
    class server
    {
    public:

        server(boost::asio::io_context& io_service,
            boost::asio::io_context::strand& strand,
            const tcp::endpoint& endpoint)
            : io_service_(io_service), strand_(strand), acceptor_(io_service, endpoint)
        {
            /// <summary>
            /// ADD DATABASE INFO HERE OR ENGINE WILL NOT WORK 
            /// COMPILE WITH DETAILS 
            /// </summary>
            std::string DATABASE_USER = "ENTER USER";
            std::string DATABASE_PASSWORD = "ENER PASSWORD";
            std::string DATABASE_IP = "127.0.0.1";
            int DATABASE_PORT = 3306;
            /// <summary>
            /// ADD DATABASE INFO HERE OR ENGINE WILL NOT WORK 
            /// COMPILE WITH DETAILS 
            /// </summary>

            port_ = acceptor_.local_endpoint().port();
            database_ = std::make_shared<db::database>(db::database(DATABASE_IP, DATABASE_PORT,DATABASE_USER,DATABASE_PASSWORD));
            database_->insertGamePort(port_);
            std::shared_ptr<game::Game>game(new game::Game(room_,database_,port_));
            game_ = game;
            run();
        }

    private:

        void run()
        {
            std::shared_ptr<gameServer::personInRoom> new_participant(new gameServer::personInRoom(io_service_, strand_, room_, port_, *game_,database_));
            acceptor_.async_accept(new_participant->socket(), strand_.wrap(boost::bind(&server::onAccept, this, new_participant, _1)));
        }

        void onAccept(std::shared_ptr<gameServer::personInRoom> new_participant, const boost::system::error_code& error)

        {
            if (!error)
            {
                new_participant->start();
            }

            run();
        }
        uint64_t port_;
        boost::asio::io_context& io_service_;
        boost::asio::io_context::strand& strand_;
        tcp::acceptor acceptor_;
        chat::chatRoom room_;
        std::shared_ptr<db::database> database_;
        std::shared_ptr<game::Game> game_;
    };
    class workerThread
    {
    public:
        static void run(std::shared_ptr<boost::asio::io_context> io_service)
        {
            {
                std::lock_guard < std::mutex > lock(m);
                std::cout << "[" << std::this_thread::get_id() << "] Thread starts" << std::endl;
            }

            io_service->run();

            {
                std::lock_guard < std::mutex > lock(m);
                std::cout << "[" << std::this_thread::get_id() << "] Thread ends" << std::endl;
            }

        }
    private:
        static std::mutex m;
    };

    std::mutex workerThread::m;
};
