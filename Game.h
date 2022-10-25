#pragma once
#include <list>
#include <set>
#include <random>
#include <boost/thread.hpp>
#include <unordered_map>
#include "RoleConfig.h"
#include "Chatroom.h"
#include "Database.h"
namespace game{

	class Role {
	public:
		Role(uint64_t roleConfig);
		uint64_t getRoleConfig();
		std::list<uint64_t> getInteractions();
		std::list<uint64_t>  getItems();
		void addInteraction(uint64_t interaction);
		void clearInteractions();
		bool isAlive();
		bool hasNightAction();
		bool hasDayAction();
		void addItem(uint64_t item);
		void clearItems();
		void setAlive(bool alive);
		void eraseItem(uint64_t itemConfig);
		void eraseInteraction(uint64_t interactionConfig);
	private:
		bool alive;
		std::string roleString;
		std::list<uint64_t > items;
		std::list<uint64_t > interactions;
		uint64_t roleConfig_;
	};

	class Game {
	public:
		Game(chat::chatRoom& chatroom, std::shared_ptr<db::database> database, int port, boost::asio::steady_timer& gameTimer);
		int addPlayer(uint64_t uuid);
		bool isEmpty();
		int addSpectator(uint64_t uuid);
		void createGame(uint64_t settings, uint64_t setupId);
		void startGame();
		void unvote(uint64_t roleAction, uint64_t uuid);
		void removePlayer(uint64_t playerid);
		void vote(uint64_t roleAction, uint64_t uuid, uint64_t target);
		std::string printGameDetails();
		void emitStatusMessageStateless(uint64_t playerid);
		std::set<uint64_t> getPlayerMeeting(uint64_t playerid);
		bool isStarted();
		bool hasEnded();
		void addKicks(uint64_t playerid);
		void kickAllPlayers();
	private:
	  std::recursive_mutex * mutex_;
		void initalizeGame();
		void startTimer();
		void queueKicks();
		void kickPlayers();
		void start();
		bool freezeVotes = false;
		bool kicksRequested = false;
		bool killUserAlt(uint64_t playerID);
		bool killUserVillage(uint64_t playerID);
		bool meetingCheck(uint64_t roleConfig, int amountVotes);
		void sendUpdateGameState();
		void handleMeeting(uint64_t roleConfig);
		bool checkVotes();
		bool canVote(uint64_t roleAction, uint64_t target);
		//Mafia 
		void mafiaMeeting();
		void handleMafiaMeeting(uint64_t playerID);
		void emitMafiaMessage(uint64_t playerID);
		//Village
		void villageMeeting();
		void handleVillageMeeting(uint64_t targetUuid);
		void emitVillageMessage(uint64_t playerId);
		void emitStatusMessage(uint64_t playerid);
		//Cop 
		void copMeeting();
		void handleCopMeeting(uint64_t targetUuid);
		void emitCopMessage(uint64_t playerId, bool mafiaSided);
		//Doctor
		void doctorMeeting();
		void handleDoctorMeeting(uint64_t targetUuid);
		//Lawyer Meeting 
		void lawyerMeeting();
		void handleLawyerMeeting(uint64_t targetUuid);
		//Framer meeting 
		void framerMeeting();
		void handleFramerMeeting(uint64_t targetUuid);
		//Bulletproof
		void emitBulletproofMessage(uint64_t targetUuid);
		//Roleblocker 
		void handleRoleblockerMeeting(uint64_t targetUuid);
		void roleBlockerMeeting(uint64_t ALIGNMENT);
		//Stalker Meeting
		void stalkerMeeting();
		void handleStalkerMeeting(uint64_t targetUuid);
		void emitStalkerMessage(uint64_t targetUuid);
		void parseGlobalMeetings();
		void parseRoleConfig(uint64_t roleConfig);
		void endGame();
		int gameOver();
		std::array<char, MAX_IP_PACK_SIZE> getMessage(std::string writeMessage);
		void switchCycle();
		void wait(int seconds);
		bool authenticateRole(uint64_t uuid, uint64_t roleAction);
		std::vector<uint64_t>& getGraveyard();
		int maxPlayers_;
		int settings_;
		time_t cycleTime;
		bool started;
		bool ended;
		bool cycle;
		chat::chatRoom& chatroom_;
		//INT = playerid
		std::set<uint64_t> alivePlayers;
		struct roleComparison {
			bool operator()(const uint64_t& a, const uint64_t& b) const {
				if (a & ROLEBLOCKER) {
					return true;
				}
				return a > b;
			}
		};
		std::set < uint64_t, roleComparison> roleQueue;
		std::set<uint64_t> kickedList;
		std::vector<Role> availableRoles;
		//INT= ROLE_OPCODE
		std::set<uint64_t> allRoles;
		//INT = PLAYERID
		std::set<uint64_t> deadPlayers;
		std::unordered_map<uint64_t, Role> playerMapping;
		int state;
		///PLAYERID -> isVoting flag 
		boost::shared_mutex votingQueue;
		// ROLE_OPCODE -> list of playerid ;
		std::unordered_map < uint64_t, std::set<uint64_t >> meetingList;
		// ROLE_OPCODE -> PLAYERID -> LIST OF VOTES 
		std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::list<uint64_t>>> meetingVotes;
		std::vector<std::thread*> gameThread;
		std::shared_ptr<db::database> database_;
		int port_;
		boost::asio::steady_timer &gameTimer_;
		uint64_t setupId; 
	  std::set<uint64_t> globalPlayers;
	};
}
