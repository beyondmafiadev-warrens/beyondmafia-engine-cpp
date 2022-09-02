#include "Chatroom.h"
namespace chat{
void chatRoom::enter(std::shared_ptr<participant> participant, uint64_t uuid)
{
    participants_[participant] = uuid;
};

void chatRoom::join(std::shared_ptr<participant> participant) {
    for (auto i = playerMessages.begin(); i != playerMessages.end(); i++) {
        std::set<uint64_t> userIds = i->second;
        if (userIds.empty() || std::find(userIds.begin(), userIds.end(), participants_[participant]) != userIds.end()) {
            participant->sendWebSocketMessage(std::ref(i->first));
        }
    }
}

void chatRoom::addClient(std::shared_ptr<cli::client> userClient, uint64_t uuid) {
    clientMap[uuid] = userClient;
}
void chatRoom::removePlayer(uint64_t playerID) {
    for (auto i = participants_.begin(); i != participants_.end(); i++) {
        if (i->second == playerID) {
            chatRoom::leave(i->first);
            break;
        }
    }
}
bool chatRoom::hasPlayer(uint64_t uuid) {
    for (auto participant : participants_) {
        if (participant.second == uuid) {
            return true;
        }
    }
    return false;
}
void chatRoom::leave(std::shared_ptr<participant> participant)
{
    participant->deleteSocket();
    uint64_t uuid = participants_[participant];
    clientMap[uuid].reset();
    clientMap.erase(uuid);
    participants_.erase(participant);
};

void chatRoom::leave(uint64_t uuid) {
    clientMap[uuid].reset();
    clientMap.erase(uuid);
    for (auto participant : participants_) {
        if (participant.second == uuid) {
            participant.first->deleteSocket();
            participants_.erase(participant.first);
            break;
        }
    }
}
//Broadcast all message to clients.
void chatRoom::broadcast(std::pair<std::array<char, MAX_IP_PACK_SIZE>, std::set<uint64_t> > msg, std::shared_ptr<participant> participant) {
    std::array<char, MAX_IP_PACK_SIZE> formatted_msg;

    // boundary correctness is guarded by protocol.hpp
    /*strcpy(formatted_msg.data(), timestamp.c_str());
    strcat(formatted_msg.data(), msg.data());*/

    playerMessages.push_back(msg);
    if (msg.second.empty()) {
        for (auto i = participants_.begin(); i != participants_.end(); i++) {
            i->first->sendWebSocketMessage(std::ref(msg.first));
        }
    }
    else {
        std::set<uint64_t> sentUsers = msg.second;
        for (auto i = participants_.begin(); i != participants_.end(); i++) {
            if (std::find(sentUsers.begin(), sentUsers.end(), i->second) != sentUsers.end()) {
                i->first->sendWebSocketMessage(std::ref(msg.first));

            }
        }
    }
};
void chatRoom::emitStateless(std::pair<std::array<char, MAX_IP_PACK_SIZE>, std::set<uint64_t> > msg) {
    if (msg.second.empty()) {
        for (auto i = participants_.begin(); i != participants_.end(); i++) {
            i->first->sendWebSocketMessage(std::ref(msg.first));
        }
    }
    else {
        std::set<uint64_t> sentUsers = msg.second;
        for (auto i = participants_.begin(); i != participants_.end(); i++) {
            if (std::find(sentUsers.begin(), sentUsers.end(), i->second) != sentUsers.end()) {
                i->first->sendWebSocketMessage(std::ref(msg.first));

            }
        }
    }

}

void chatRoom::emitMessage(std::pair<std::array<char, MAX_IP_PACK_SIZE>, std::set<uint64_t> > msg) {
    playerMessages.push_back(msg);
    if (msg.second.empty()) {
        for (auto i = participants_.begin(); i != participants_.end(); i++) {
            i->first->sendWebSocketMessage(std::ref(msg.first));
        }
    }
    else {
        std::set<uint64_t> sentUsers = msg.second;
        for (auto i = participants_.begin(); i != participants_.end(); i++) {
            if (std::find(sentUsers.begin(), sentUsers.end(), i->second) != sentUsers.end()) {
                i->first->sendWebSocketMessage(std::ref(msg.first));

            }
        }
    }
}
}