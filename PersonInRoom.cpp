#include "PersonInRoom.h"
namespace gameServer {
    listener::listener(
        std::shared_ptr<boost::asio::io_context> ioc,
        tcp::endpoint endpoint,
        std::shared_ptr<personInRoom> person) : ioc_(ioc)
        , acceptor_(*ioc)
        , client_(person) {
        beast::error_code ec;
        // Open the acceptor


               acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(false), ec);
        if (ec)
        {
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if (ec)
        {
            return;
        }
        client_->insertWebsocket(acceptor_.local_endpoint().port());

    };
    int listener::getPort() {
        return port_;
    }
    void listener::run() {
        do_accept();

    };
    void
        listener::do_accept()
    {
        // The new connection gets its own strand
        if (acceptor_.is_open()) {
            acceptor_.async_accept(
                net::make_strand(*ioc_),
                beast::bind_front_handler(
                    &listener::on_accept,
                    shared_from_this()));
        }
        else {
            do_accept();
        }
    };

    void
        listener::on_accept(beast::error_code ec, tcp::socket socket)
    {
        if (ec)
        {
            return;
        }
        
    
            std::shared_ptr<session>ptr(new session(std::move(socket), client_, *ioc_));
            listener_ = ptr;
            listener_->run();
        do_accept();

        // Accept another connection
     
    };



    void listener::closeAcceptor() {
        if (acceptor_.is_open()) {
            acceptor_.close();
        }

    }

    void listener::clearWebsocket() {
        listener_->close();
        return;
    }
    std::shared_ptr<session> listener::getWebsocket() {
        return listener_;
    }

    session::session(tcp::socket&& socket, std::shared_ptr<personInRoom> client, boost::asio::io_context& ioc)
        : 
        client_(client),
        ioc_(ioc),
        strand_(ioc.get_executor()) {
        ws_ = std::make_unique<websocket::stream < beast::tcp_stream> >(websocket::stream < beast::tcp_stream>(std::move(socket)));
        write_msgs_.clear();
        }

    void
        session::run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(ws_->get_executor(),
            beast::bind_front_handler(
                &session::on_run,
                shared_from_this()));
    };

    void
        session::on_run()
    {
        // Set suggested timeout settings for the websocket


        // Set a decorator to change the Server of the handshake
        ws_->set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res)
            {
                res.set(http::field::server,
                    "BeyondMafia C++ Engine");
            }));

        ws_->async_accept(boost::asio::bind_executor(strand_,
            beast::bind_front_handler(
                &session::on_accept,
                shared_from_this())));
    };

    void
        session::on_accept(beast::error_code ec)
    {
       
        if (ec)
            return;
        // Read a message
        session::do_read();
    };
    void
        session::do_read()
    {
        // Read a message into our buffer
        ws_->async_read(
            buffer_, boost::asio::bind_executor(strand_,
                beast::bind_front_handler(
                    &session::on_read,
                    shared_from_this())));
    };
    void
        session::on_read(
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        if (ec == beast::error::timeout) {
            client_->createWebsocket();
        }
        if (ec)
            return;
   
            // Echo the message
            ws_->text(ws_->got_text());
            std::array<char, MAX_IP_PACK_SIZE> formatted_msg;
            if (buffer_.size() > 0 && buffer_.size() < MAX_IP_PACK_SIZE) {
                memset(formatted_msg.data(), '\0', formatted_msg.size());
                std::string data = beast::buffers_to_string(buffer_.data());
                strcpy(formatted_msg.data(), data.c_str());
                //parse message
                //ensure message length is adequate 
                //parse message prior to on message
                client_->onMessage(formatted_msg);
            }
            buffer_.consume(buffer_.size());
            ws_->async_read(
                buffer_, boost::asio::bind_executor(strand_,
                    beast::bind_front_handler(
                        &session::on_read,
                        shared_from_this())));
    };

    void session::send(std::string message) {
        net::post(
            ws_->get_executor(),
                beast::bind_front_handler(
                    &session::on_send,
                    shared_from_this(),message));
    }

    void session::on_send(std::string message) {
        bool write_in_progress = !write_msgs_.empty();
        if (message.size() > 0) {
            write_msgs_.push_back(message);
        }
        if (!write_in_progress) {
            return;
        }
        ws_->async_write(boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()), boost::asio::bind_executor(strand_,
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this())));
    }

    void
        session::on_write(
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        if (ec == beast::error::timeout) {
            client_->createWebsocket();
        }
        if (ec)
            return;
        boost::ignore_unused(bytes_transferred);
        write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
                ws_->async_write(
                    boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()), boost::asio::bind_executor(strand_,
                        beast::bind_front_handler(
                            &session::on_write,
                            shared_from_this())));
            }

    };

    void
        session::on_close(beast::error_code ec)
    {
        if (ec == websocket::error::closed ||ec == beast::error::timeout) {
            buffer_.clear();
            return;
        }
      
    }
    void session::close() {
        ws_->async_close(beast::websocket::close_code::normal, boost::asio::bind_executor(strand_, beast::bind_front_handler(
            &session::on_close,
            shared_from_this())));
    }

    personInRoom::personInRoom(boost::asio::io_context& io_service,
        boost::asio::io_service::strand& strand, chat::chatRoom& room, uint64_t port, game::Game& game, std::shared_ptr<db::database> database) :
        socket_(io_service), strand_(strand), room_(room), port_(port), io_service_(io_service), game_(game), database_(database)
    {

    };
    tcp::socket& personInRoom::socket() {
        return socket_;
    };

    bool personInRoom::verifyUser(std::string cookieString) {
        return database_->verifyUser(this->userId_, cookieString);
    }

    void personInRoom::start() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data(), read_msg_.size()),
            strand_.wrap(boost::bind(&personInRoom::startHandler, shared_from_this(), _1)));

    };

    void personInRoom::parseWebsocketMessage(boost::json::value& val, std::array<char, MAX_IP_PACK_SIZE>& msg) {
        std::set<uint64_t> recipients;
        if(game_.isStarted()){
            recipients = game_.getPlayerMeeting(this->userId_);
        }
        try {
            std::string formatted;
            int roleAction, playerId;
            int command = val.at("cmd").as_int64();
            boost::json::string message;
            std::array<char, MAX_IP_PACK_SIZE> sendBuffer;
            std::string sendMessage = "";
            if (!this->verified) {
                if (command == -3) {
                    std::string cookie = val.at("auth").as_string().data();
                    if (this->verifyUser(cookie)) {
                        this->addPlayer();
                        this->sendPlayerId();
                        this->sendGameDetails();
                        this->joinRoom();
                        this->verified = true;
                    }
                    else {

                    }
                }
            }
            else {
                switch (command) {
                case(-2):
                    //Stop Typing
                    sendMessage = "{\"cmd\":-2, \"playerId\":" + std::to_string(this->userId_) + '}';
                    memset(sendBuffer.data(), '\0', sendBuffer.size());
                    std::copy(sendMessage.begin(), sendMessage.end(), sendBuffer.data());
                    room_.broadcast(std::make_pair(sendBuffer, recipients), shared_from_this());
                    break;
                case(-1):
                    //Typing
                    sendMessage = "{\"cmd\":-1, \"playerId\":" + std::to_string(this->userId_) + '}';
                    memset(sendBuffer.data(), '\0', sendBuffer.size());
                    std::copy(sendMessage.begin(), sendMessage.end(), sendBuffer.data());
                    room_.broadcast(std::make_pair(sendBuffer, recipients), shared_from_this());
                    break;
                case(0):
                    sendMessage = "{\"cmd\": 0, \"msg\":";
                    sendMessage += boost::json::serialize(val.at("msg").as_string()) + ",\"playerId\":" + std::to_string(this->userId_) + '}';
                    memset(sendBuffer.data(), '\0', sendBuffer.size());
                    std::copy(sendMessage.begin(), sendMessage.end(), sendBuffer.data());
                    room_.broadcast(std::make_pair(sendBuffer, recipients), shared_from_this());
                    break;
                case(1):
                    roleAction = val.at("roleAction").as_int64();
                    playerId = val.at("playerid").as_int64();
                    game_.vote(roleAction, this->userId_, playerId);
                    break;
                case(2):
                    room_.leave(shared_from_this());
                    game_.removePlayer(this->userId_);
                    database_->deleteGamePlayers(this->userId_);
                    database_->deleteWebsocket(this->userId_);
                    break;
                }
            }
        }
        catch (std::out_of_range& e) {
            return;
        }
    }
    void personInRoom::onMessage(std::array<char, MAX_IP_PACK_SIZE>& msg)
    {
        bool write_in_progress = !write_msgs_.empty();
        boost::system::error_code ec;
        boost::json::value val = boost::json::parse(msg.data(), ec);
        if (!ec) {
            write_msgs_.push_back(msg);
            if (!write_in_progress)
            {
                //Parse websocket message
                try {
                    personInRoom::parseWebsocketMessage(val, write_msgs_.front());
                        memset(write_msgs_.front().data(), '\0', write_msgs_.front().size());
                        boost::asio::async_write(socket_,
                            boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
                            strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));
                    }
                
                catch (std::out_of_range& e) {
                    memset(write_msgs_.front().data(), '\0', write_msgs_.front().size());
                    boost::asio::async_write(socket_,
                        boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
                        strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));

                }
            }
        }
    };

    void personInRoom::sendGameDetails() {
        if (listener_ != nullptr && listener_->getWebsocket() != nullptr) {
            auto websocketClient = listener_->getWebsocket().get();
            if (websocketClient) {
                websocketClient->send(game_.printGameDetails());
            }
        }
    }

    void personInRoom::sendPlayerId() {
        std::string sendMessage = "{\"cmd\":-3, \"playerId\":" + std::to_string(this->userId_) + '}';
        if (listener_ != nullptr && listener_->getWebsocket() != nullptr) {
            auto websocketClient = listener_->getWebsocket().get();
            if (websocketClient) {
                websocketClient->send(std::ref(sendMessage));
            }
        }
    }

    void personInRoom::startHandler(const boost::system::error_code& error)
    {
        //Only enter room if msg indicates to do so.
        //room_.enter(shared_from_this());
        //---
        boost::system::error_code ec;
        boost::json::value val = boost::json::parse(read_msg_.data(), ec);
        if (!ec) {
            parseMessage(val);
            boost::asio::async_read(socket_,
                boost::asio::buffer(read_msg_, read_msg_.size()),
                strand_.wrap(boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
        }
    };
    void personInRoom::sendWebSocketMessage(std::array<char, MAX_IP_PACK_SIZE>& msg)
    {
        std::string websocketMessage(msg.data());
        if (listener_ != nullptr && listener_->getWebsocket() != nullptr) {
            auto websocketClient = listener_->getWebsocket().get();
            if (websocketClient) {
                websocketClient->send(std::ref(websocketMessage));
            }
        }
    }
    void personInRoom::addPlayer() {
        if (!room_.hasPlayer(userId_)) {
            room_.enter(shared_from_this(), userId_);
            game_.addPlayer(userId_);
            database_->insertPlayer(port_, userId_);
        }
    }

    std::shared_ptr<cli::client> personInRoom::createClient(int playerId) {
        tcp::resolver resolver = tcp::resolver(io_service_);
        tcp::resolver::basic_resolver::query query = tcp::resolver::basic_resolver::query("127.0.0.1", std::to_string(port_));
        tcp::resolver::iterator iterator = resolver.resolve(query);
        std::shared_ptr<cli::client> cli(new cli::client(io_service_, iterator, playerId));
        return cli;
    }
    void personInRoom::createWebsocket() {
        std::shared_ptr<boost::asio::io_context> io_service(new boost::asio::io_context);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 0);
        std::shared_ptr<listener>listener (new gameServer::listener(io_service, endpoint, shared_from_this()));
        listener.swap(listener_);
        listener_->run();
        t = std::make_shared<boost::thread>( boost::thread( boost::bind(&gameServer::socketThread::run, io_service) ));
    };
    void personInRoom::insertWebsocket(int port) {
        database_->deleteWebsocket(userId_);
        database_->insertWebsocket(port_, port, userId_);
    }
    void personInRoom::joinRoom() {
        room_.join(shared_from_this());
    }
    void personInRoom::deleteSocket() {
        listener_->clearWebsocket();
        listener_->closeAcceptor();
        listener_.reset();
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        socket_.close();
    }

    int personInRoom::parseMessage(boost::json::value& val) {
        std::array<char, MAX_IP_PACK_SIZE> formatted_msg;
        try {
            int command = val.at("cmd").as_int64();
            std::shared_ptr<cli::client> client;
            int playerId, maxPlayers;
            uint64_t settingsConfig;
            boost::json::array roleArray;
            boost::json::string message;
            switch (command) {
                //Join game command. 
            case(-1):
                playerId = val.at("playerid").get_int64();
                room_.leave(playerId);
                game_.removePlayer(playerId);
                database_->deleteGamePlayers(playerId);
                database_->deleteWebsocket(playerId);
                return 0;
                //Create Game API 
            case(0):
                settingsConfig = val.at("settings").get_int64();
                roleArray = val.at("roles").get_array();
                game_.createGame(settingsConfig, roleArray);
                return 0;
            case(1):
                playerId = val.at("playerid").get_int64();
                client = personInRoom::createClient(playerId);
                room_.addClient(client, playerId);
                socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                socket_.close();
                return 0;
                //Join game command;
            case(2):
                userId_ = val.at("playerid").get_int64();
                personInRoom::createWebsocket();
                return 1;

            }
        }
        catch (std::out_of_range& e) {
            return 0;
        }
    };

    void personInRoom::readHandler(const boost::system::error_code& error)
    {
        if (!error)
        {
            //Only broadcast message if msg indicates to do so 
            // after parsing json
            //room_.broadcast(read_msg_, shared_from_this());
            boost::system::error_code ec;
            boost::json::value val = boost::json::parse(read_msg_.data(), ec);
            if (!ec) {
                //parse message
                parseMessage(val);
                boost::asio::async_read(socket_,
                    boost::asio::buffer(read_msg_, read_msg_.size()),
                    strand_.wrap(boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
            }
        }

    };
    void personInRoom::writeHandler(const boost::system::error_code& error)
    {
        if (!error)
        {
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
                //Parse websocket message
                std::array<char, MAX_IP_PACK_SIZE>& msg = write_msgs_.front();
                boost::json::value val = boost::json::parse(msg.data());
                //Parse websocket message
                try {
                    personInRoom::parseWebsocketMessage(val, write_msgs_.front());
                        memset(write_msgs_.front().data(), '\0', write_msgs_.front().size());
                        boost::asio::async_write(socket_,
                            boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
                            strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));
                    }
                catch (std::out_of_range& e) {
                    memset(write_msgs_.front().data(), '\0', write_msgs_.front().size());
                    boost::asio::async_write(socket_,
                        boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
                        strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));
                }
            }
        }
    };
}
