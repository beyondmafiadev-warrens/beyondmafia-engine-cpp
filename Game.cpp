#include "Game.h"

namespace game {
	bool Role::isAlive() {
		return this->alive;
	}
	void Role::setAlive(bool alive) {
		this->alive = alive;
	}

	bool Role::hasNightAction() {
		switch (this->roleConfig_) {
		case(MAFIA):
			return true;
		case(COP):
			return true;
		case(COP + MAFIA):
			return true;
		case(DOCTOR):
			return true;
		case(ROLEBLOCKER):
			return true;
		case(ROLEBLOCKER + MAFIA):
			return true;
		}
		return false;
	}

	bool Role::hasDayAction() {
		switch (this->roleConfig_) {
		case(VILLAGER):
			return true;
		}
		return false;
	}

	Game::Game(chat::chatRoom& chatroom, std::shared_ptr<db::database> database, int port) :
		chatroom_(chatroom), database_(database), port_(port) {
		this->state = 0;
		this->cycle = false;
		this->started = false;
	}

	void Game::removePlayer(uint64_t playerid) {
		auto it = alivePlayers.find(playerid);
		if (it != alivePlayers.end()) {
			alivePlayers.erase(it);
			std::set<uint64_t> recipients;
			std::array<char, MAX_IP_PACK_SIZE> writeString;
			std::string writeMessage = "{\"cmd\": 6,  \"playerid\":" + std::to_string(playerid) + '}';
			memset(writeString.data(), '\0', writeString.size());
			std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
			chatroom_.emitStateless(std::make_pair(writeString, recipients));
		}
	}

	void Game::createGame(uint64_t settings, boost::json::array roles) {
		this->settings_ = settings;
		for (int i = 0; i < roles.size(); i++) {
			boost::json::value val = roles[i];
			if (!val.is_int64()) {
				throw new std::invalid_argument("JSON Array contains invalid roles");
			}
			else {
				Role role = Role::Role(val.as_int64());
				Game::parseRoleConfig(role.getRoleConfig());
				availableRoles.push_back(role);
			}
		}
		database_->insertGameData(port_, roles.size(), settings);
		for (int i = 0; i < roles.size(); i++) {
			boost::json::value val = roles[i];
			database_->insertRole(port_, val.as_int64());
		}
	}

	void Game::unvote(uint64_t roleAction, uint64_t uuid) {
		meetingVotes[roleAction][uuid].push_front(0);
		std::set<uint64_t> sendPlayers = Game::getPlayerMeeting(uuid);
		std::string writeMessage = "{\"cmd\": 2, \"roleAction\":" + std::to_string(roleAction) + ",\"playerid\":" + std::to_string(uuid) + ",\"target\":-1}";
		std::array<char, MAX_IP_PACK_SIZE> writeString;
		memset(writeString.data(), '\0', writeString.size());
		std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
		chatroom_.emitMessage(std::make_pair(writeString, sendPlayers));
	}
	bool Game::canVote(uint64_t roleAction, uint64_t target) {
		switch (roleAction) {
		case(MAFIA):
			if (alivePlayers.find(target) != alivePlayers.end()
				&& std::find(meetingList[MAFIA].begin(), meetingList[MAFIA].end(), target) == meetingList[MAFIA].end()) {
				return true;
			}
			break;
		case(VILLAGER):
			if (alivePlayers.find(target) != alivePlayers.end()) {
				return true;
			}
			break;
		case(COP):
			if (alivePlayers.find(target) != alivePlayers.end() && std::find(meetingList[COP].begin(), meetingList[COP].end(), target) == meetingList[COP].end()) {
				return true;
			}
		case(DOCTOR):
			if (alivePlayers.find(target) != alivePlayers.end() && std::find(meetingList[DOCTOR].begin(), meetingList[DOCTOR].end(), target) == meetingList[DOCTOR].end()) {
				return true;
			}
		case(ROLEBLOCKER):
			if (alivePlayers.find(target) != alivePlayers.end() && std::find(meetingList[ROLEBLOCKER].begin(), meetingList[ROLEBLOCKER].end(), target) == meetingList[ROLEBLOCKER].end()) {
				return true;
			}
		case(ROLEBLOCKER + MAFIA):
			if (alivePlayers.find(target) != alivePlayers.end() && std::find(meetingList[MAFIA].begin(), meetingList[MAFIA].end(), target) == meetingList[MAFIA].end()) {
				return true;
			}
		}

		return false;
	}
	void Game::vote(uint64_t roleAction, uint64_t uuid, uint64_t target) {
		if (std::find(allRoles.begin(), allRoles.end(), roleAction) != allRoles.end() &&
			Game::authenticateRole(uuid, roleAction)
			&&
			playerMapping.at(uuid).isAlive() &&
			Game::canVote(roleAction, target) || (target == 0 || target == -2 || playerMapping.find(target) != playerMapping.end())) {
			if (target == 0) {
				Game::unvote(roleAction, uuid);
				return;
			}
			std::set<uint64_t> sendPlayers = Game::getPlayerMeeting(uuid);
			std::string writeMessage = "{\"cmd\": 2, \"roleAction\":"+ std::to_string(roleAction) + ",\"playerid\":" + std::to_string(uuid) + ",\"target\":" + std::to_string(target) + '}';
			std::array<char, MAX_IP_PACK_SIZE> writeString;
			memset(writeString.data(), '\0', writeString.size());
			std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
			chatroom_.emitMessage(std::make_pair(writeString, sendPlayers));
			meetingVotes[roleAction][uuid].push_front(target);
		}
	}

	void Game::startGame() {
		std::thread* t = new std::thread(&Game::start, this);
		gameThread.push_back(t);
	}
	void Game::endGame() {
		gameThread[0]->join();
	}
	bool Game::isStarted() {
		return this->started;
	}

	void Game::start() {

		while (true) {
			if (this->ended) {
				Game::endGame();
			}
			if (this->started) {
				//Game initalization
				if (playerMapping.empty()) {
					unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
					std::default_random_engine e(seed);
					std::shuffle(availableRoles.begin(), availableRoles.end(), e);
					for (auto i = alivePlayers.begin(); i != alivePlayers.end(); i++) {
						std::set<uint64_t> players;
						uint64_t roleConfig = availableRoles.back().getRoleConfig();
						if (meetingList.find(roleConfig) != meetingList.end()) {
							meetingList.at(roleConfig).insert(*i);
						}
						else {
							meetingList.insert(std::make_pair(roleConfig, players));
							meetingList.at(roleConfig).insert(*i);
						}
						if (meetingList.find(VILLAGER) != meetingList.end()) {
							meetingList.at(VILLAGER).insert(*i);
						}
						else {
							std::set<uint64_t> village;
							meetingList.insert(std::make_pair(VILLAGER, village));
							meetingList.at(VILLAGER).insert(*i);
						}
						if (roleConfig & MAFIA && meetingList.find(MAFIA) != meetingList.end()) {
							meetingList.at(MAFIA).insert(*i);
						}
						else if (roleConfig & MAFIA && meetingList.find(MAFIA) == meetingList.end()) {
							std::set<uint64_t> mafia;
							meetingList.insert(std::make_pair(MAFIA, mafia));
							meetingList.at(MAFIA).insert(*i);
						}
						if (meetingVotes.find(roleConfig) != meetingVotes.end()) {
							std::list<uint64_t>list;
							meetingVotes[roleConfig].insert(std::make_pair(*i, list));
						}
						else {
							std::unordered_map< uint64_t, std::list<uint64_t>> playerVotes;
							std::list<uint64_t>list;
							meetingVotes.insert(std::make_pair(roleConfig, playerVotes));
							meetingVotes[roleConfig].insert(std::make_pair(*i, list));
						}
						if (meetingVotes.find(VILLAGER) != meetingVotes.end()) {
							std::list<uint64_t>list;
							meetingVotes[VILLAGER].insert(std::make_pair(*i, list));
						}
						else {
							std::unordered_map< uint64_t, std::list<uint64_t>> playerVotes;
							std::list<uint64_t>list;
							meetingVotes.insert(std::make_pair(VILLAGER, playerVotes));
							meetingVotes[VILLAGER].insert(std::make_pair(*i, list));
						}
						if (roleConfig & MAFIA && meetingVotes.find(MAFIA) == meetingVotes.end()) {
							std::list<uint64_t>list;
							meetingVotes[MAFIA].insert(std::make_pair(*i, list));
						}
						else if (roleConfig & MAFIA && meetingVotes.find(MAFIA) != meetingVotes.end()) {
							std::unordered_map< uint64_t, std::list<uint64_t>> playerVotes;
							std::list<uint64_t>list;
							meetingVotes.insert(std::make_pair(MAFIA, playerVotes));
							meetingVotes[MAFIA].insert(std::make_pair(*i, list));
						}
						playerMapping.insert(std::make_pair(*i, availableRoles.back()));
						availableRoles.back().setAlive(true);
						std::string writeMessage = "{\"cmd\": 4,  \"role\":" + std::to_string(availableRoles.back().getRoleConfig()) + '}';
						// std::string writeMessage = "{\"cmd\": 1,  \"msg\": \"You have been assigned the role " + availableRoles.back().getRoleString() + '!';
						std::set<uint64_t> recipients;
						std::array<char, MAX_IP_PACK_SIZE> writeString;
						memset(writeString.data(), '\0', writeString.size());
						std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
						recipients.insert(*i);
						chatroom_.emitMessage(std::make_pair(writeString, recipients));
						availableRoles.pop_back();
					}
					this->state = 1;
					Game::sendUpdateGameState();
				}
				if (Game::checkVotes(60)) {
					Game::parseGlobalMeetings();
					Game::switchCycle();
					if (Game::gameOver()) {
						break;
					}
					this->state += 1;
					//send update game state command
					Game::sendUpdateGameState();
				}
			}
		}
	}

	bool Game::gameOver() {
		if (meetingList[MAFIA].size() == 0) {
			std::string writeMessage = "{\"cmd\": -4,\"role\":1}";
			std::set<uint64_t> recipients;
			std::array<char, MAX_IP_PACK_SIZE> writeString;
			memset(writeString.data(), '\0', writeString.size());
			std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
			chatroom_.emitMessage(std::make_pair(writeString, recipients));
			return true;
		}
		else if (meetingList[MAFIA].size() >= meetingList[VILLAGER].size() - meetingList[MAFIA].size()) {
			std::string writeMessage = "{\"cmd\": -4,\"role\":1}";
			std::set<uint64_t> recipients;
			std::array<char, MAX_IP_PACK_SIZE> writeString;
			memset(writeString.data(), '\0', writeString.size());
			std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
			chatroom_.emitMessage(std::make_pair(writeString, recipients));
			return true;
		}
		return false;
	}

	void Game::sendUpdateGameState() {
		std::string writeMessage = "{\"cmd\": 7, \"state\":" + std::to_string(this->state) + '}';
		std::set<uint64_t> recipients;
		std::array<char, MAX_IP_PACK_SIZE> writeString;
		memset(writeString.data(), '\0', writeString.size());
		std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
		chatroom_.emitMessage(std::make_pair(writeString, recipients));

		for (auto i = playerMapping.begin(); i != playerMapping.end(); i++) {
			if (!this->cycle && i->second.isAlive() && i->second.hasNightAction()) {
				uint64_t roleConfig = i->second.getRoleConfig();
				if (MAFIA & roleConfig) {
					this->roleQueue.insert(MAFIA);
				}
				this->roleQueue.insert(i->second.getRoleConfig());
			}
			if (this->cycle && i->second.isAlive() && i->second.hasDayAction()) {
				this->roleQueue.insert(i->second.getRoleConfig());
			}
			Game::emitStatusMessage(i->first);
		}
		if (this->cycle) {
			//Insert village meeting
			this->roleQueue.insert(VILLAGER);
		}
	}

	bool Game::checkVotes(int cycleTime) {
		bool voteCheck = true;
		for (auto i = roleQueue.begin(); i != roleQueue.end(); i++) {
			auto playerVotes = meetingVotes.at(*i);
			for (auto j = playerVotes.begin(); j != playerVotes.end(); j++) {
				if (j->second.empty() || playerMapping.at(j->first).isAlive() && j->second.front() == 0) {
					voteCheck = false;
					break;
				}
			}
		}
		if (voteCheck) {
			return true;
		}
		if (difftime(time(NULL), this->cycleTime) >= cycleTime) {
			if (static_cast<double>(kickedList.size()) > static_cast<double>(alivePlayers.size() / 2.0)) {				//Wait 5 seconds
				//Check 
				//Then return true

				return true;
			}
			else {
				return false;
			}
		}
		return false;
	}
	void Game::parseRoleConfig(uint64_t roleConfig) {
		if ((roleConfig & MAFIA)) {
			allRoles.insert(MAFIA);
		}
		if (roleConfig == VILLAGER) {
			allRoles.insert(VILLAGER);
		}
		allRoles.insert(roleConfig);
		return;
	}

	void Game::switchCycle() {
		this->cycle = !this->cycle;
		this->cycleTime = time(NULL);

		for (auto i = meetingVotes.begin(); i != meetingVotes.end(); i++) {
			for (auto j = i->second.begin(); j != i->second.end(); j++) {
				j->second.clear();
			}
		}

		for (auto i = playerMapping.begin(); i != playerMapping.end(); i++) {
			Role playerRole = i->second;
			std::list<uint64_t> interactions = playerRole.getInteractions();
			if (playerRole.isAlive() && !interactions.empty()) {
				//Use custom comparator for find 
				std::list<uint64_t>::iterator docFound = std::find(interactions.begin(), interactions.end(), DOCTOR);
				if (docFound != interactions.end()) {
					handleDoctorMeeting(i->first);
				}
				for (auto j = interactions.begin(); j != interactions.end(); j++) {
					switch (*j) {
					case(COP):
						handleCopMeeting(i->first);
						break;
					case(MAFIA):
						handleMafiaMeeting(i->first);
						break;
					case(VILLAGER):
						handleVillageMeeting(i->first);
						break;
					case(COP + MAFIA):
						handleStalkerMeeting(i->first);
					}
				}
			}

			i->second.clearInteractions();
			i->second.clearItems();
		}
		roleQueue.clear();
	}
	void Game::emitStatusMessage(uint64_t playerid) {
		if (this->started) {
			std::set<uint64_t> recipients;
			recipients.insert(playerid);
			if (playerMapping.at(playerid).isAlive()
				&& this->cycle) {
				std::string writeMessage = "{\"cmd\": 5,\"players\":[";
				if (this->cycle) {
					std::string secondWriteMessage = writeMessage;
					std::set<uint64_t> players = meetingList.at(VILLAGER);
					for (auto j = players.begin(); j != players.end(); j++) {
						secondWriteMessage += std::to_string(*j) + ',';
					}
					secondWriteMessage.pop_back();
					secondWriteMessage += "], \"role\":" + std::to_string(VILLAGER) + '}';
					std::array<char, MAX_IP_PACK_SIZE> secondWriteString;
					memset(secondWriteString.data(), '\0', secondWriteString.size());
					std::copy(secondWriteMessage.begin(), secondWriteMessage.end(), secondWriteString.data());
					chatroom_.emitMessage(std::make_pair(secondWriteString, recipients));
					if (playerMapping.at(playerid).getRoleConfig() == VILLAGER) {
						return;
					}
				}
			}
			if (playerMapping.at(playerid).isAlive() && !this->cycle && playerMapping.at(playerid).getRoleConfig() & MAFIA) {
				std::string writeMessage = "{\"cmd\": 5,\"players\":[";
				std::set<uint64_t> players = meetingList.at(MAFIA);
				for (auto j = players.begin(); j != players.end(); j++) {
					writeMessage += std::to_string(*j) + ',';
				}
				writeMessage.pop_back();
				writeMessage += "], \"role\":" + std::to_string(MAFIA) + '}';
				std::array<char, MAX_IP_PACK_SIZE> secondWriteString;
				memset(secondWriteString.data(), '\0', secondWriteString.size());
				std::copy(writeMessage.begin(), writeMessage.end(), secondWriteString.data());
				chatroom_.emitMessage(std::make_pair(secondWriteString, recipients));
				if (playerMapping.at(playerid).getRoleConfig() == MAFIA) {
					return;
				}
			}
			if (playerMapping.at(playerid).isAlive() && (!this->cycle && playerMapping.at(playerid).hasNightAction()) || (this->cycle && playerMapping.at(playerid).hasDayAction())) {
				std::string writeMessage = "{\"cmd\": 5,\"players\":[";
				std::set<uint64_t> players = meetingList.at(playerMapping.at(playerid).getRoleConfig());
				for (auto j = players.begin(); j != players.end(); j++) {
					writeMessage += std::to_string(*j) + ',';
				}
				writeMessage.pop_back();
				writeMessage += "], \"role\":" + std::to_string(playerMapping.at(playerid).getRoleConfig()) + '}';
				std::array<char, MAX_IP_PACK_SIZE> writeString;
				memset(writeString.data(), '\0', writeString.size());
				std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
				chatroom_.emitMessage(std::make_pair(writeString, recipients));
			}
		}
		if (!playerMapping.at(playerid).isAlive()) {
			for (auto i = roleQueue.begin(); i != roleQueue.end(); i++) {
				std::set<uint64_t> recipients;
				recipients.insert(playerid);
				std::string writeMessage = "{\"cmd\": 5,\"players\":[";
				std::set<uint64_t> players = meetingList.at(*i);
				for (auto j = players.begin(); j != players.end(); j++) {
					writeMessage += std::to_string(*j) + ',';
				}
				writeMessage.pop_back();
				writeMessage += "], \"role\":" + std::to_string(playerMapping.at(playerid).getRoleConfig()) + '}';
				std::array<char, MAX_IP_PACK_SIZE> writeString;
				memset(writeString.data(), '\0', writeString.size());
				std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
				chatroom_.emitMessage(std::make_pair(writeString, recipients));
			}
		}
	}

	void Game::handleMafiaMeeting(uint64_t playerID) {
		if (Game::killUserAlt(playerID)) {
			Game::emitVillageMessage(playerID);
		}
	}

	void Game::handleCopMeeting(uint64_t targetUuid) {
		if (playerMapping.at(targetUuid).getRoleConfig() & MAFIA) {
			Game::emitCopMessage(targetUuid, true);
		}
		else {
			Game::emitCopMessage(targetUuid, false);
		}
	}
	void Game::handleStalkerMeeting(uint64_t targetUuid) {
		Game::emitStalkerMessage(targetUuid);
	}
	
	void Game::handleDoctorMeeting(uint64_t targetUuid) {
		auto target = alivePlayers.find(targetUuid);
		playerMapping.at(targetUuid).addItem(SAVE);
	}
	void Game::handleRoleblockerMeeting(uint64_t targetUuid) {
		uint64_t roleConfig = playerMapping.at(targetUuid).getRoleConfig();
		meetingVotes.at(roleConfig).at(targetUuid).push_front(-2);
		if (roleConfig != MAFIA && roleConfig & MAFIA) {
			meetingVotes.at(MAFIA).at(targetUuid).push_front(-2);
		}
	}


	void Game::emitMafiaMessage(uint64_t playerID) {
		std::string writeMessage = "{\"cmd\": 3,\"role\":" + std::to_string(playerMapping.at(playerID).getRoleConfig()) + ",\"action\":1, \"playerid\": " + std::to_string(playerID) + '}';
		std::set<uint64_t> recipients;
		std::array<char, MAX_IP_PACK_SIZE> writeString;
		memset(writeString.data(), '\0', writeString.size());
		std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
		chatroom_.emitMessage(std::make_pair(writeString, recipients));
	}
	void Game::emitVillageMessage(uint64_t playerID) {
		std::string writeMessage = "{\"cmd\": 3,\"role\":" + std::to_string(playerMapping.at(playerID).getRoleConfig()) + ",\"action\":1, \"playerid\": " + std::to_string(playerID) + '}';
		std::set<uint64_t> recipients;
		std::array<char, MAX_IP_PACK_SIZE> writeString;
		memset(writeString.data(), '\0', writeString.size());
		std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
		chatroom_.emitMessage(std::make_pair(writeString, recipients));
	}

	void Game::emitCopMessage(uint64_t playerID, bool mafiaSided) {
		std::string writeMessage = "{\"cmd\": 3,\"action\":2, \"alignment\":" + std::to_string(mafiaSided) + ",\"playerid\": " + std::to_string(playerID) + '}';
		std::array<char, MAX_IP_PACK_SIZE> writeString;
		memset(writeString.data(), '\0', writeString.size());
		std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
		chatroom_.emitMessage(std::make_pair(writeString, meetingList[COP]));
	}
	void Game::emitBulletproofMessage(uint64_t playerId) {
		std::string writeMessage = "{\"cmd\": 3,\"action\":3}";
		std::set<uint64_t> recipients;
		recipients.insert(playerId);
		std::array<char, MAX_IP_PACK_SIZE> writeString;
		memset(writeString.data(), '\0', writeString.size());
		std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
		chatroom_.emitMessage(std::make_pair(writeString, recipients));
	}

	void Game::emitStalkerMessage(uint64_t targetUuid) {
	  std::string writeMessage = "{\"cmd\": 3,\"playerid\":"  + std::to_string(targetUuid) + ",\"role\":" + std::to_string(playerMapping.at(targetUuid).getRoleConfig()) + ",\"action\":4}";
		std::array<char, MAX_IP_PACK_SIZE> writeString;
		memset(writeString.data(), '\0', writeString.size());
		std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
		chatroom_.emitMessage(std::make_pair(writeString, meetingList[COP + MAFIA]));
	}
	void Game::handleVillageMeeting(uint64_t playerID) {
		if (Game::killUserVillage(playerID)) {
			Game::emitVillageMessage(playerID);
		}
	}

	//Make kill user return bool 
	//Emit message based on return valu
	bool Game::killUserAlt(uint64_t playerID) {
		std::list<uint64_t> items = playerMapping.at(playerID).getItems();
		//Use custom comparator
		std::list<uint64_t>::iterator docFound = std::find(items.begin(), items.end(), SAVE);
		std::list<uint64_t>::iterator vestFound = std::find_if(items.begin(), items.end(), [](const uint64_t& itemConfig) {
			return itemConfig & VEST;
			});
		if (docFound == items.end() && vestFound == items.end()) {
			auto target = alivePlayers.find(playerID);
			if (target != alivePlayers.end()) {
				Role role = playerMapping.at(*target);
				uint64_t config = role.getRoleConfig();
				playerMapping.at(*target).setAlive(false);
				meetingList[config].erase(*target);
				meetingVotes[config].erase(*target);
				if (config != VILLAGER) {
					meetingList[VILLAGER].erase(*target);
					meetingVotes[VILLAGER].erase(*target);
				}
				if (config != MAFIA && config & MAFIA) {
					meetingList[MAFIA].erase(*target);
					meetingVotes[MAFIA].erase(*target);
				}
				deadPlayers.insert(*target);
				alivePlayers.erase(target);
				return true;
			};
		}
		if (vestFound != items.end()) {
			emitBulletproofMessage(playerID);
			playerMapping.at(playerID).eraseItem(VEST);
		}
		return false;
	}
	std::set<uint64_t> Game::getPlayerMeeting(uint64_t playerid) {
		std::set<uint64_t> players = this->deadPlayers;
		players.insert(playerid);
		if (!playerMapping.at(playerid).isAlive()) {
			return players;
		}
		if (this->cycle) {
			players.insert(meetingList[VILLAGER].begin(), meetingList[VILLAGER].end());
			return players;
		}
		if (!this->cycle && playerMapping.at(playerid).getRoleConfig() & MAFIA) {
			players.insert(meetingList[MAFIA].begin(), meetingList[MAFIA].end());
			return players;
		}
		if (!this->cycle && playerMapping.at(playerid).hasNightAction()) {
			players.insert(meetingList[playerMapping.at(playerid).getRoleConfig()].begin(), meetingList[playerMapping.at(playerid).getRoleConfig()].end());
			return players;
		}
		return players;
	}

	bool Game::killUserVillage(uint64_t playerID) {
		std::list<uint64_t> items = playerMapping.at(playerID).getItems();
		auto target = alivePlayers.find(playerID);
		if (target != alivePlayers.end()) {
			Role role = playerMapping.at(*target);
			uint64_t config = role.getRoleConfig();
			playerMapping.at(*target).setAlive(false);
			meetingList[config].erase(*target);
			meetingVotes[config].erase(*target);
			if (config != VILLAGER) {
				meetingList[VILLAGER].erase(*target);
				meetingVotes[VILLAGER].erase(*target);
			}
			if (config != MAFIA && config & MAFIA) {
				meetingList[MAFIA].erase(*target);
				meetingVotes[MAFIA].erase(*target);
			}
			deadPlayers.insert(*target);
			alivePlayers.erase(target);
			return true;
		}
		return false;
	}

	void Game::parseGlobalMeetings() {
		for (auto i = roleQueue.begin(); i != roleQueue.end(); i++) {
			switch (*i) {
			case(MAFIA):
				mafiaMeeting();
				break;
			case(COP):
				copMeeting();
				break;
			case(COP + MAFIA):
				stalkerMeeting();
				break;
			case(VILLAGER):
				villageMeeting();
				break;
			case(DOCTOR):
				doctorMeeting();
				break;
			case(ROLEBLOCKER):
				roleBlockerMeeting(VILLAGER);
				break;
			case(ROLEBLOCKER + MAFIA):
				roleBlockerMeeting(MAFIA);
				break;
			}
		}
	}

	int Game::addPlayer(uint64_t uuid) {
		if (alivePlayers.size() < availableRoles.size()) {
			alivePlayers.insert(uuid);
			std::set<uint64_t> recipients;
			std::array<char, MAX_IP_PACK_SIZE> writeString;
			std::string writeMessage = "{\"cmd\": 8,  \"playerid\":" + std::to_string(uuid) + '}';
			memset(writeString.data(), '\0', writeString.size());
			std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
			chatroom_.emitStateless(std::make_pair(writeString, recipients));
		}
		else {
			return addSpectator(uuid);
		}
		if (alivePlayers.size() == availableRoles.size()) {
			std::string writeMessage = "{\"cmd\": 0,  \"action\":-1}";
			std::list<uint64_t> recipients;
			Game::wait(5);
			if (alivePlayers.size() == availableRoles.size()) {
				this->cycleTime = time(NULL);
				this->started = true;
				this->ended = false;
				Game::startGame();
			}
		}
		return 1;
	}

	void Game::wait(int seconds) {
		time_t currentTime = time(NULL);
		while (!difftime(time(NULL), currentTime) >= seconds) {

		}
		return;
	}

	int Game::addSpectator(uint64_t uuid) {
		return 1;
	}


	std::string Game::printGameDetails() {
		std::string retString = "{\"cmd\":9,\"players\":[";
		for (auto i = alivePlayers.begin(); i != alivePlayers.end(); i++) {
			retString += std::to_string(*i) + ',';
		}
		if (!alivePlayers.empty()) {
			retString.pop_back();
		}
		retString += "],\"state\":" + std::to_string(this->state) + ",\"graveyard\":[";
		if (!deadPlayers.empty()) {
			for (auto i = deadPlayers.begin(); i != deadPlayers.end(); i++) {
				retString += std::to_string(*i) + ',';
			}
			retString.pop_back();
		}
		retString += "],\"roles\":[";
		if (!availableRoles.empty()) {
			for (auto i = availableRoles.begin(); i != availableRoles.end(); i++) {
				retString += std::to_string(i->getRoleConfig()) + ',';
			}
		}
		else {
			for (auto i = playerMapping.begin(); i != playerMapping.end(); i++) {
				retString += std::to_string(i->second.getRoleConfig()) + ',';
			}
		}
		retString.pop_back();
		retString += "],\"size\":" + std::to_string(alivePlayers.size()) + ",\"maxSize\":" + std::to_string(availableRoles.size()) + '}';
		return retString;
	}


	void Game::villageMeeting() {
		Game::handleMeeting(VILLAGER);
	}
	void Game::stalkerMeeting() {
		Game::handleMeeting(COP + MAFIA);
	}

	void Game::mafiaMeeting() {
		Game::handleMeeting(MAFIA);
	}
	void Game::copMeeting() {
		Game::handleMeeting(COP);
	}
	void Game::doctorMeeting() {
		Game::handleMeeting(DOCTOR);
	}
	void Game::roleBlockerMeeting(uint64_t ALIGNMENT) {
		Game::handleMeeting(ROLEBLOCKER + ALIGNMENT);
	}
	

	void Game::handleMeeting(uint64_t roleConfig) {
		std::unordered_map<uint64_t, std::list<uint64_t>> voteMap = meetingVotes.at(roleConfig);
		std::unordered_map<uint64_t, uint64_t> currentVotes;
		for (auto votes = voteMap.begin(); votes != voteMap.end(); votes++) {
			uint64_t voterUuid = votes->second.front();
			if (currentVotes.find(voterUuid) != currentVotes.end()) {
				currentVotes[voterUuid] = currentVotes[voterUuid] + 1;
			}
			else {
				currentVotes.insert(std::make_pair(voterUuid, 1));
			}
		}
		uint64_t tempID;
		int highestVote = 0;
		for (auto i = currentVotes.begin(); i != currentVotes.end(); i++) {
			if (highestVote < i->second && i->first != -2) {
				tempID = i->first;
				highestVote = i->second;
			}
		}
		if (highestVote != 0 && meetingCheck(roleConfig, highestVote)) {
			if (roleConfig & ROLEBLOCKER) {
				handleRoleblockerMeeting(tempID);
			}
			else {
				playerMapping.at(tempID).addInteraction(roleConfig);
			}
		}
	}

	bool Game::meetingCheck(uint64_t roleConfig,int amountVotes) {
		if (
			static_cast<double>(amountVotes) > static_cast<double>(meetingList[roleConfig].size()) / 2.0) {
			return true;
		}
		return false;
	}



	bool Game::authenticateRole(uint64_t uuid, uint64_t roleAction) {
		if (roleAction == 0x0 || (playerMapping.at(uuid).getRoleConfig() & roleAction)) {
			return true;
		}
		
		return false;
	}
	
	void Role::addInteraction(uint64_t interaction) {
		this->interactions.push_back(interaction);
	}
	void Role::clearInteractions() {
		this->interactions.clear();
	}
	void Role::addItem(uint64_t item) {
		this->items.push_back(item);
	}
	uint64_t Role::getRoleConfig() {
		return this->roleConfig_;
	}
	std::list<uint64_t > Role::getItems() {
		return this->items;
	}
	std::list<uint64_t > Role::getInteractions() {
		return this->interactions;
	}
	
	void Role::clearItems() {
		for (auto i = items.begin(); i != items.end(); i++) {
			if (!(*i & MULTI_USE)) {
				items.erase(i);
			}
		}
	}
	void Role::eraseInteraction(uint64_t interaction) {
		this->interactions.remove(interaction);
	}

	void Role::eraseItem(uint64_t itemConfig) {
		std::list<uint64_t>::iterator itemiterator = std::remove_if(items.begin(), items.end(), [itemConfig](const uint64_t& item) {
			return item & itemConfig;
			});
		this->items.erase(itemiterator,items.end());
	}

	Role::Role(uint64_t roleConfig) :
		roleConfig_(roleConfig){
		if (roleConfig & BULLETPROOF) {
			this->items.push_back(VEST + MULTI_USE);
		}
		this->alive = true;
	}
}
