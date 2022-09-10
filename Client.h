#pragma once
#include <array>
#include <deque>
#include <set>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include "protocol.h"
namespace cli {
    using tcp = boost::asio::ip::tcp;
    class client
    {
    public:
        client(boost::asio::io_context& io_service,
            tcp::resolver::iterator endpoint_iterator, int userid);

        void write(const std::array<char, MAX_IP_PACK_SIZE>& msg);

        void close();

        void parse(const std::array<char, MAX_IP_PACK_SIZE>& msg);

    private:

        void onConnect(const boost::system::error_code& error);

        void readHandler(const boost::system::error_code& error);

        void writeImpl(std::array<char, MAX_IP_PACK_SIZE> msg);

        void writeHandler(const boost::system::error_code& error);

        void closeImpl();

        boost::asio::io_context& io_service_;
        tcp::socket socket_;
        std::array<char, MAX_IP_PACK_SIZE> read_msg_;
        std::deque<std::array<char, MAX_IP_PACK_SIZE>> write_msgs_;
        int userid_;
    };
}
