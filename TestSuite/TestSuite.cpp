#include "../mafiaServer.h"
#include <boost/thread.hpp>

int main(int argc, char* argv[])
{
    std::shared_ptr<boost::asio::io_context> io_service(new boost::asio::io_service);
    boost::shared_ptr<boost::asio::io_context::work> work(new boost::asio::io_service::work(*io_service));
    boost::shared_ptr<boost::asio::io_context::strand> strand(new boost::asio::io_service::strand(*io_service));
    boost::asio::ip::tcp::endpoint  tcp_endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 0);
    std::cout << "[" << std::this_thread::get_id() << "]" << "server starts" << std::endl;

    std::shared_ptr<clientServer::server> a_server(new clientServer::server(*io_service, *strand, tcp_endpoint));
    boost::thread_group workers;
    for (int i = 0; i < 1; ++i)
    {
        boost::thread* t = new boost::thread{ boost::bind(&clientServer::workerThread::run, io_service) };

#ifdef __linux__
        // bind cpu affinity for worker thread in linux
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(i, &cpuset);
        pthread_setaffinity_np(t->native_handle(), sizeof(cpu_set_t), &cpuset);
#endif
        workers.add_thread(t);
    }
    workers.join_all();
}
