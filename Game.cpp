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
		case(STALKER):
			return true;
		case(DOCTOR):
			return true;
		case(ROLEBLOCKER):
			return true;
		case(HOOKER):
			return true;
		case(LAWYER):
			return true;
		case(FRAMER):
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

	Game::Game(chat::chatRoom& chatroom, std::shared_ptr<db::database> database, int port, boost::asio::steady_timer& gameTimer) :
		chatroom_(chatroom), database_(database), port_(port), gameTimer_(gameTimer) {
		this->state = 0;
		this->cycle = false;
		this->started = false;
		this->ended = false;
		this->mutex_ = new std::mutex();
	}
	bool Game::isEmpty() {
		return alivePlayers.empty();
	}

  void Game::removePlayer(uint64_t playerid) {
		 std::lock_guard<std::mutex> iterationMutex(*mutex_);
		auto it = alivePlayers.find(playerid);
		if (it != alivePlayers.end()) {
			alivePlayers.erase(it);
			if (this->started) {
				Role role = playerMapping.at(playerid);
				bool isAlive = role.isAlive();
				if (isAlive) {
					uint64_t config = role.getRoleConfig();
					playerMapping.at(playerid).setAlive(false);
					meetingList[config].erase(playerid);
					meetingVotes[config].erase(playerid);
					if (config != VILLAGER) {
						meetingList[VILLAGER].erase(playerid);
						meetingVotes[VILLAGER].erase(playerid);
					}
					if (config != MAFIA && config & MAFIA) {
						meetingList[MAFIA].erase(playerid);
						meetingVotes[MAFIA].erase(playerid);
					}
					deadPlayers.insert(playerid);
					std::set<uint64_t> recipients;
					std::array<char, MAX_IP_PACK_SIZE> writeString;
					std::string writeMessage = "{\"cmd\": -6,\"playerid\":" + std::to_string(playerid) + ",\"role\":" + std::to_string(config) + '}';
					memset(writeString.data(), '\0', writeString.size());
					std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
					chatroom_.emitMessage(std::make_pair(writeString, recipients));
				}
			}
			else{
			  std::set<uint64_t> recipients;
			  std::array<char, MAX_IP_PACK_SIZE> writeString;
			  std::string writeMessage = "{\"cmd\": -7,  \"playerid\":" + std::to_string(playerid) + '}';
			  memset(writeString.data(), '\0', writeString.size());
			  std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
			  chatroom_.emitStateless(std::make_pair(writeString, recipients));
			}
		}
	}
	void Game::kickAllPlayers() {
		for (auto i = playerMapping.begin(); i != playerMapping.end(); i++) {
			database_->deleteGamePlayers(i->first);
			database_->deleteWebsocket(i->first);
		}
	}
	bool Game::hasEnded() {
		return this->ended;
	}
	void Game::createGame(uint64_t settings, uint64_t setupId) {
		boost::json::array roles = boost::json::parse(database_->getRoles(setupId)).as_array();
		this->setupId = setupId;
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
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
		meetingVotes[roleAction][uuid].push_front(0);
		std::set<uint64_t> sendPlayers = Game::getPlayerMeeting(uuid);
		std::string writeMessage = "{\"cmd\": 2, \"roleAction\":" + std::to_string(roleAction) + ",\"playerid\":" + std::to_string(uuid) + ",\"target\":-1}";
		std::array<char, MAX_IP_PACK_SIZE> writeString;
		memset(writeString.data(), '\0', writeString.size());
		std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
		chatroom_.emitMessage(std::make_pair(writeString, sendPlayers));
	}
  
	bool Game::canVote(uint64_t roleAction, uint64_t target) {
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
		if (freezeVotes) {
			return false;
		}
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
		case(STALKER):
			if (alivePlayers.find(target) != alivePlayers.end() && std::find(meetingList[STALKER].begin(), meetingList[STALKER].end(), target) == meetingList[STALKER].end()
				&& std::find(meetingList[MAFIA].begin(), meetingList[MAFIA].end(), target) == meetingList[MAFIA].end()) {
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
		case(HOOKER):
			if (alivePlayers.find(target) != alivePlayers.end() && std::find(meetingList[HOOKER].begin(), meetingList[HOOKER].end(), target) == meetingList[HOOKER].end()
				&& std::find(meetingList[MAFIA].begin(), meetingList[MAFIA].end(), target) == meetingList[MAFIA].end()) {
				return true;
			}
		case(FRAMER):
			if (alivePlayers.find(target) != alivePlayers.end() && std::find(meetingList[MAFIA].begin(), meetingList[MAFIA].end(), target) == meetingList[MAFIA].end()) {
				return true;
			}
		case(LAWYER):
			if (alivePlayers.find(target) != alivePlayers.end() && std::find(meetingList[MAFIA].begin(), meetingList[MAFIA].end(), target) != meetingList[MAFIA].end()) {
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
	void Game::startTimer() {
		if (this->state % 2 == 0) {
			//day
			gameTimer_.expires_after(boost::asio::chrono::minutes(5));
		}
		else {
			//night
			gameTimer_.expires_after(boost::asio::chrono::minutes(2));
		}
		gameTimer_.async_wait(boost::bind(&Game::queueKicks, this));
	}

	void Game::addKicks(uint64_t playerID) {
		Role role = playerMapping.at(playerID);
		uint64_t config = role.getRoleConfig();
		bool check = true;
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
		for (auto i = roleQueue.begin(); i != roleQueue.end(); i++) {
			if (*i & config && (meetingVotes.at(*i).at(playerID).empty() || meetingVotes.at(*i).at(playerID).front() == 0)) {
				check = false;
				break;
			}
		}
		if (check && kickedList.find(playerID) == kickedList.end()) {
			kickedList.insert(playerID);
			std::set<uint64_t> sendPlayers;
			std::string writeMessage = "{\"cmd\": -5, \"count\":" + std::to_string((int)(std::floor(alivePlayers.size() / 2.0)) - kickedList.size()) + "}";
			std::array<char, MAX_IP_PACK_SIZE> writeString;
			memset(writeString.data(), '\0', writeString.size());
			std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
			chatroom_.emitMessage(std::make_pair(writeString, sendPlayers));
		}
	}

	void Game::queueKicks() {
		std::set<uint64_t> sendPlayers;
		std::string writeMessage = "{\"cmd\": -5, \"count\":" + std::to_string((int)(std::floor(alivePlayers.size() / 2.0))) + "}";
		std::array<char, MAX_IP_PACK_SIZE> writeString;
		memset(writeString.data(), '\0', writeString.size());
		std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
		chatroom_.emitMessage(std::make_pair(writeString, sendPlayers));
	}

	void Game::start() {

		while (true) {
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
					database_->updateStartedGameState();
				}
				if (Game::checkVotes() || Game::gameOver() >= 0) {
					freezeVotes = true;
					Game::parseGlobalMeetings();
					Game::switchCycle();
					int gameOverCheck = Game::gameOver();
					if (gameOverCheck >= 0) {
						if (gameOverCheck == 0) {
							std::string writeMessage = "{\"cmd\": -4,\"role\":0}";
							std::set<uint64_t> recipients;
							std::array<char, MAX_IP_PACK_SIZE> writeString;
							memset(writeString.data(), '\0', writeString.size());
							std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
							chatroom_.emitMessage(std::make_pair(writeString, recipients));
							for (auto i = playerMapping.begin(); i != playerMapping.end(); i++) {
								if (!(i->second.getRoleConfig() & MAFIA)) {
									database_->insertGameStats(i->first, std::ceil(120 - 120 * database_->getSetupStats(this->setupId)), true);
								}
								else if(i->second.getRoleConfig() & MAFIA) {
									database_->insertGameStats(i->first, 0, false);
								}
							}
						}
						else if (gameOverCheck == 1) {
							std::string writeMessage = "{\"cmd\": -4,\"role\":1}";
							std::set<uint64_t> recipients;
							std::array<char, MAX_IP_PACK_SIZE> writeString;
							memset(writeString.data(), '\0', writeString.size());
							std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
							chatroom_.emitMessage(std::make_pair(writeString, recipients));
							for (auto i = playerMapping.begin(); i != playerMapping.end(); i++) {
								if (!(i->second.getRoleConfig() & MAFIA)) {
									database_->insertGameStats(i->first, 0, false);
								}
								else if (i->second.getRoleConfig() & MAFIA) {
									database_->insertGameStats(i->first, std::ceil(120 - (120 * (1 - database_->getSetupStats(this->setupId)))), true);
								}
							}
						}
						database_->updateSetupStats(this->setupId, gameOverCheck);
						database_->updateEndGameState();
						this->ended = true;
						break;
					}
					this->state += 1;
					//send update game state command
					Game::sendUpdateGameState();
				}
			}
		}
	}
	int Game::gameOver() {
		if (meetingList[MAFIA].size() == 0) {
			return 0;
		}
		else if (meetingList[MAFIA].size() >= meetingList[VILLAGER].size() - meetingList[MAFIA].size()) {
			return 1;
		}
		return -1;
	}

	void Game::sendUpdateGameState() {
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
		std::string writeMessage = "{\"cmd\": 7, \"state\":" + std::to_string(this->state) + '}';
		database_->updateGameState();
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
		Game::startTimer();
		freezeVotes = false; 
		kicksRequested = false;
	}

	void Game::kickPlayers() {
	   std::lock_guard<std::mutex> iterationMutex(*mutex_);
		for (auto i = roleQueue.begin(); i != roleQueue.end(); i++) {
			auto playerVotes = meetingVotes.at(*i);
			for (auto j = playerVotes.begin(); j != playerVotes.end(); j++) {
				Role role = playerMapping.at(j->first);
				bool isAlive = role.isAlive();
				if (j->second.empty() && isAlive || isAlive && j->second.front() == 0) {
					uint64_t config = role.getRoleConfig();
					playerMapping.at(j->first).setAlive(false);
					meetingList[config].erase(j->first);
					meetingVotes[config].erase(j->first);
					if (config != VILLAGER) {
						meetingList[VILLAGER].erase(j->first);
						meetingVotes[VILLAGER].erase(j->first);
					}
					if (config != MAFIA && config & MAFIA) {
						meetingList[MAFIA].erase(j->first);
						meetingVotes[MAFIA].erase(j->first);
					}
					deadPlayers.insert(j->first);
					alivePlayers.erase(j->first);
					std::set<uint64_t> sendPlayers;
					std::string writeMessage = "{\"cmd\": -6,\"playerid\":" + std::to_string(j->first) + ",\"role\":" + std::to_string(config) + '}';
					std::array<char, MAX_IP_PACK_SIZE> writeString;
					memset(writeString.data(), '\0', writeString.size());
					std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
					chatroom_.emitMessage(std::make_pair(writeString, sendPlayers));
				}
			}
		}
	}

	bool Game::checkVotes() {
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
		bool voteCheck = true;
		for (auto i = roleQueue.begin(); i != roleQueue.end(); i++) {
			auto playerVotes = meetingVotes.at(*i);
			for (auto j = playerVotes.begin(); j != playerVotes.end(); j++) {
				bool isAlive = playerMapping.at(j->first).isAlive();
				if (j->second.empty() && isAlive || isAlive && j->second.front() == 0) {
					voteCheck = false;
					break;
				}
			}
		}
		if (voteCheck) {
			return true;
		}
		if (!kicksRequested && kickedList.size() == (int)(std::floor(alivePlayers.size() / 2.0))) {
			gameTimer_.expires_after(boost::asio::chrono::seconds(5));
			gameTimer_.async_wait(boost::bind(&Game::kickPlayers, this));
			kicksRequested = true;
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

				std::list<uint64_t>::iterator lawFound = std::find(interactions.begin(), interactions.end(), LAWYER);
				std::list<uint64_t>::iterator framerFound = std::find(interactions.begin(), interactions.end(), FRAMER);
				if (docFound != interactions.end()) {
					handleDoctorMeeting(i->first);
				}
				if (lawFound != interactions.end()) {
					handleLawyerMeeting(i->first);
				}
				if (framerFound != interactions.end()) {
					handleFramerMeeting(i->first);
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
					case(STALKER):
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
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
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
		std::list<uint64_t> items = playerMapping.at(targetUuid).getItems();
		bool millerFound = std::find_if(items.begin(), items.end(), [](const uint64_t& itemConfig) {
			return itemConfig & MILLER_VEST;
			}) != items.end();
		if (playerMapping.at(targetUuid).getRoleConfig() & MAFIA) {
			if (!millerFound) {
				Game::emitCopMessage(targetUuid, true);
			}
			else {
				Game::emitCopMessage(targetUuid, false);
			}
		}
		else {
			if (!millerFound) {
				Game::emitCopMessage(targetUuid, false);
			}
			else {
				Game::emitCopMessage(targetUuid, true);
			}
		}
	}
	void Game::handleStalkerMeeting(uint64_t targetUuid) {
		Game::emitStalkerMessage(targetUuid);
	}
	void Game::handleDoctorMeeting(uint64_t targetUuid) {
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
		auto target = alivePlayers.find(targetUuid);
		playerMapping.at(targetUuid).addItem(SAVE);
	}
	void Game::lawyerMeeting()
	{
		handleMeeting(LAWYER);
	}

	void Game::handleLawyerMeeting(uint64_t targetUuid)
	{
		playerMapping.at(targetUuid).addItem(MILLER_VEST);
	}

	void Game::framerMeeting()
	{
		Game::handleMeeting(FRAMER);
	}
	void Game::handleFramerMeeting(uint64_t targetUuid)
	{
		playerMapping.at(targetUuid).addItem(MILLER_VEST);
	}

	void Game::handleRoleblockerMeeting(uint64_t targetUuid) {
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
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
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
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
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
	  std::string writeMessage = "{\"cmd\": 3,\"playerid\":"  + std::to_string(targetUuid) + ",\"role\":" + std::to_string(playerMapping.at(targetUuid).getRoleConfig()) + ",\"action\":4}";
		std::array<char, MAX_IP_PACK_SIZE> writeString;
		memset(writeString.data(), '\0', writeString.size());
		std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
		chatroom_.emitMessage(std::make_pair(writeString, meetingList[STALKER]));
	}
	void Game::handleVillageMeeting(uint64_t playerID) {
		if (Game::killUserVillage(playerID)) {
			Game::emitVillageMessage(playerID);
		}
	}

	//Make kill user return bool 
	//Emit message based on return valu
	bool Game::killUserAlt(uint64_t playerID) {
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
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
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
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
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
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
			case(STALKER):
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
			case(HOOKER):
				roleBlockerMeeting(MAFIA);
				break;
			case(LAWYER):
				lawyerMeeting();
				break;
			case(FRAMER):
				framerMeeting();
				break;
			}
		}
	}
	void Game::initalizeGame() {
		if (alivePlayers.size() == availableRoles.size()) {
			this->cycleTime = time(NULL);
			this->started = true;
			this->ended = false;
			Game::startGame();
		}
		else {
			std::string writeMessage = "{\"cmd\": -8}";
			std::set<uint64_t> recipients;
			std::array<char, MAX_IP_PACK_SIZE> writeString;
			memset(writeString.data(), '\0', writeString.size());
			std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
			chatroom_.emitMessage(std::make_pair(writeString, recipients));
		}
	}
	int Game::addPlayer(uint64_t uuid) {
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
		if (alivePlayers.size() < availableRoles.size()) {
			globalPlayers.insert(uuid);
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
			std::set<uint64_t> recipients;
			std::string writeMessage = "{\"cmd\": -9}";
			std::array<char, MAX_IP_PACK_SIZE> writeString;
			memset(writeString.data(), '\0', writeString.size());
			std::copy(writeMessage.begin(), writeMessage.end(), writeString.data());
			chatroom_.emitMessage(std::make_pair(writeString, recipients));
			gameTimer_.expires_after(boost::asio::chrono::seconds(5));
			gameTimer_.async_wait(boost::bind(&Game::initalizeGame, this));
		}
		return 1;

	}

	void Game::wait(int seconds) {
	        
	}

	int Game::addSpectator(uint64_t uuid) {
		return 1;
	}


	std::string Game::printGameDetails() {
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
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
		retString += "],\"size\":" + std::to_string(alivePlayers.size()) + ",\"maxSize\":" + std::to_string(availableRoles.size()) + ",\"globalPlayers\":[";
		for (auto i = globalPlayers.begin(); i != globalPlayers.end(); i++) {
		  retString += std::to_string(*i) + ',';
		}
		retString.pop_back();
		retString+= "]}";
		return retString;
	}


	void Game::villageMeeting() {
		Game::handleMeeting(VILLAGER);
	}
	void Game::stalkerMeeting() {
		Game::handleMeeting(STALKER);
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
		std::lock_guard<std::mutex> iterationMutex(*mutex_);
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
		if (roleConfig & MILLER || roleConfig & GODFATHER) {
			this->items.push_back(MILLER_VEST + MULTI_USE);
		}
		this->alive = true;
	}
}
