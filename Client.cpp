#include "Client.h"
namespace cli {
    client::client(boost::asio::io_context& io_service,
        tcp::resolver::iterator endpoint_iterator, int userid) :
        io_service_(io_service), socket_(io_service), userid_(userid)
    {
        memset(read_msg_.data(), '\0', MAX_IP_PACK_SIZE);
        boost::asio::async_connect(socket_, endpoint_iterator, boost::bind(&client::onConnect, this, _1));
    };
    void client::write(const std::array<char, MAX_IP_PACK_SIZE>& msg)
    {
        io_service_.post(boost::bind(&client::writeImpl, this, msg));
    };

    void client::parse(const std::array<char, MAX_IP_PACK_SIZE>& msg) {
        //parse json
        client::write(msg);
    }

    void client::close()
    {
        io_service_.post(boost::bind(&client::closeImpl, this));
    };
    void client::onConnect(const boost::system::error_code& error)
    {
        if (!error)
        {

            std::string msg = "{\"cmd\": 2,  \"playerid\":" + std::to_string(userid_) + '}';
            std::array<char, MAX_IP_PACK_SIZE> writeString;
            memset(writeString.data(), '\0', writeString.size());
            std::copy(msg.begin(), msg.end(), writeString.data());
            boost::asio::async_write(socket_,
                boost::asio::buffer(writeString, writeString.size()),
                boost::bind(&client::readHandler, this, _1));
        }
    };

    void client::readHandler(const boost::system::error_code& error)
    {
        if (!error)
        {
            client::write(read_msg_);
            boost::asio::async_read(socket_,
                boost::asio::buffer(read_msg_, read_msg_.size()),
                boost::bind(&client::readHandler, this, _1));
        }
    };
    void client::writeImpl(std::array<char, MAX_IP_PACK_SIZE> msg)
    {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress)
        {
            boost::asio::async_write(socket_,
                boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
                boost::bind(&client::writeHandler, this, _1));
        }
    };
    void client::writeHandler(const boost::system::error_code& error)
    {
        if (!error)
        {
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
                boost::asio::async_write(socket_,
                    boost::asio::buffer(write_msgs_.front(), write_msgs_.front().size()),
                    boost::bind(&client::writeHandler, this, _1));
            }
        }
    };
    void client::closeImpl()
    {
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        socket_.close();
    };
}