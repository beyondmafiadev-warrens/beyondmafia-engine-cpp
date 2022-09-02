#include <memory>
#include <unordered_map>
#include <vector>
#include <list>
#include "Client.h"
namespace chat{
    class participant
    {
    public:
        virtual ~participant() {};
        virtual void onMessage(std::array<char, MAX_IP_PACK_SIZE>& msg) = 0;
        virtual void sendWebSocketMessage(std::array<char, MAX_IP_PACK_SIZE>& msg) = 0;
        virtual void deleteSocket() = 0;
    };

    class chatRoom {
    public:
        void removePlayer(uint64_t playerID);
        void enter(std::shared_ptr<participant> participant, uint64_t uuid);
        void leave(std::shared_ptr<participant> participant);
        void leave(uint64_t uuid);
        void join(std::shared_ptr<participant> participant);
        void broadcast(std::pair<std::array<char, MAX_IP_PACK_SIZE>, std::set<uint64_t>> msg, std::shared_ptr<participant> participant);
        void emitMessage(std::pair<std::array<char, MAX_IP_PACK_SIZE>, std::set<uint64_t>> msg);
        void emitStateless(std::pair<std::array<char, MAX_IP_PACK_SIZE>, std::set<uint64_t>> msg);
        void addClient(std::shared_ptr<cli::client> userClient, uint64_t uuid);
        bool hasPlayer(uint64_t uuid);

    private:
        std::unordered_map<std::shared_ptr<participant>, uint64_t> participants_;
        std::unordered_map<uint64_t, std::shared_ptr<cli::client> > clientMap;
        std::vector<std::pair<std::array<char, MAX_IP_PACK_SIZE>, std::set<uint64_t> > >  playerMessages;
    };
}