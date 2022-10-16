#pragma once
#include <mysql/jdbc.h>
#include <boost/json.hpp>
namespace db {
	class database {
	public:
		database(std::string DATABASE_IP, int DATABASE_PORT, std::string DATABASE_USER, std::string DATABASE_PASSWORD) {
			database::makeConnnection(DATABASE_IP, DATABASE_PORT, DATABASE_USER, DATABASE_PASSWORD);
		}
		void insertGamePort(uint64_t port) {
			sql::Statement* stmt;
			sql::PreparedStatement* prep_stmt;
			std::lock_guard<std::mutex> guard(*mutex_);
			stmt = (*globalConnnection)->createStatement();
			stmt->execute("LOCK TABLE mafiadata.gametablequeue WRITE");
			prep_stmt = (*globalConnnection)->prepareStatement(
				"INSERT INTO mafiadata.gametablequeue(port) VALUES(?)");
			prep_stmt->setUInt64(1, port);
			prep_stmt->execute();
			stmt->execute("UNLOCK TABLES");
			delete stmt;
			delete prep_stmt;
		}
		void deleteGamePort(uint64_t port) {
			sql::Statement* stmt;
			sql::PreparedStatement* prep_stmt;			
			std::lock_guard<std::mutex> guard(*mutex_);
			stmt = (*globalConnnection)->createStatement();
			stmt->execute("LOCK TABLE gametablequeue WRITE");
			prep_stmt = (*globalConnnection)->prepareStatement(
				"DELETE FROM mafiadata.gametablequeue WHERE port = ?;");
			prep_stmt->setUInt64(1, port);
			prep_stmt->execute();
			stmt->execute("UNLOCK TABLES");
			delete stmt;
			delete prep_stmt;
		}


		void updateSetupStats(uint64_t id, int winState) {
			if (winState == 0) {
				sql::PreparedStatement* prep_stmt;			
				std::lock_guard<std::mutex> guard(*mutex_);
				prep_stmt = (*globalConnnection)->prepareStatement(
					"UPDATE mafiadata.setups SET townWins=townWins+1 WHERE setupId = ?");
				prep_stmt->setInt(1, id);
				prep_stmt->executeQuery();
				delete prep_stmt;
			}
			else if (winState == 1){
				sql::PreparedStatement* prep_stmt;			
				std::lock_guard<std::mutex> guard(*mutex_);
				prep_stmt = (*globalConnnection)->prepareStatement(
					"UPDATE mafiadata.setups SET mafiaWins=mafiaWins+1 WHERE setupId = ?");
					prep_stmt->setInt(1, id);
					prep_stmt->executeQuery();
					delete prep_stmt;
			}

		}
		void updateEndGameState() {
			sql::PreparedStatement* prep_stmt;			
			std::lock_guard<std::mutex> guard(*mutex_);
			prep_stmt = (*globalConnnection)->prepareStatement(
				"UPDATE `mafiadata`.`games` SET gameEnded=true WHERE gameId = ?");
			prep_stmt->setUInt64(1, gameID);
			prep_stmt->executeQuery();
			delete prep_stmt;
		}
		void updateStartedGameState() {
			sql::PreparedStatement* prep_stmt;			
			std::lock_guard<std::mutex> guard(*mutex_);
			prep_stmt = (*globalConnnection)->prepareStatement(
				"UPDATE `mafiadata`.`games` SET startedGame=true WHERE gameId = ?");
			prep_stmt->setUInt64(1, gameID);
			prep_stmt->executeQuery();
			delete prep_stmt;
		}

		void updateGameState() {
			sql::PreparedStatement* prep_stmt;			
			std::lock_guard<std::mutex> guard(*mutex_);
			prep_stmt = (*globalConnnection)->prepareStatement(
				"UPDATE `mafiadata`.`games` SET state=state+1 WHERE gameId = ?");
			prep_stmt->setUInt64(1, gameID);
			prep_stmt->executeQuery();
			delete prep_stmt;
		}

		double getSetupStats(uint64_t id) {
			sql::PreparedStatement* prep_stmt;			
			std::lock_guard<std::mutex> guard(*mutex_);
			sql::ResultSet* res;
			prep_stmt = (*globalConnnection)->prepareStatement(
				"SELECT mafiaWins,townWins FROM mafiadata.setups WHERE setupId = ? LIMIT 1;");
			prep_stmt->setInt(1, id);
			res = prep_stmt->executeQuery();
			res->next();
			int mafiaWins = res->getInt("mafiaWins");
			int townWins = res->getInt("townWins");
			delete res;
			delete prep_stmt;
			if ((mafiaWins + townWins) < 20) {
				return .50;
			}
			else {
				return ((double)townWins) / (mafiaWins + townWins);
			}
		}
		void insertGameStats(uint64_t playerid,int score,bool win) {
			sql::PreparedStatement* prep_stmt;			
			std::lock_guard<std::mutex> guard(*mutex_);
			if (win) {
				prep_stmt = (*globalConnnection)->prepareStatement(
					"UPDATE `mafiadata`.`usertable` SET wins=wins+1,points=points+? WHERE playerid = ?;");
				prep_stmt->setInt(1, score);
				prep_stmt->setInt(2, playerid);
			}
			else {
				prep_stmt = (*globalConnnection)->prepareStatement(
					"UPDATE `mafiadata`.`usertable` SET losses=losses+1 WHERE playerid = ?;");
				prep_stmt->setInt(1, playerid);
			}
			prep_stmt->executeQuery();
			delete prep_stmt;
		}
		std::string getRoles(uint64_t id) {
			sql::PreparedStatement* prep_stmt;			
			std::lock_guard<std::mutex> guard(*mutex_);
			sql::ResultSet* res;
			prep_stmt = (*globalConnnection)->prepareStatement(
				"SELECT setup FROM mafiadata.setups WHERE setupId = ? LIMIT 1;");
			prep_stmt->setInt(1, id);
			res = prep_stmt->executeQuery();
			res->next();
			std::string returnString = res->getString("setup");
			delete res;
			delete prep_stmt;
			return returnString;
		}
		void insertGameData(uint64_t port, int maxPlayers, uint64_t settings) {
			sql::Statement* stmt;
			sql::ResultSet* res;
			sql::PreparedStatement* prep_stmt;		
			std::lock_guard<std::mutex> guard(*mutex_);
			stmt = (*globalConnnection)->createStatement();
			prep_stmt = (*globalConnnection)->prepareStatement(
				"INSERT INTO mafiadata.games(port,maxPlayers,state,rankedGame,lockedGame,startedGame,gameEnded) VALUES(?,?,0,?,?,false,false)");
			prep_stmt->setUInt64(1, port);
			prep_stmt->setInt(2, maxPlayers);
			if (settings == 0) {
				prep_stmt->setBoolean(3, false);
				prep_stmt->setBoolean(4, false);
			}
			prep_stmt->execute();
			res = stmt->executeQuery("SELECT LAST_INSERT_ID() AS id;");
			res->next();
			this->gameID = res->getInt64("id");
			delete res;
			delete stmt;
			delete prep_stmt;
		}
		void insertPlayer(uint64_t port, uint64_t playerid) {
			sql::Statement* stmt;
			sql::PreparedStatement* prep_stmt;		
			std::lock_guard<std::mutex> guard(*mutex_);
			stmt = (*globalConnnection)->createStatement();
			prep_stmt = (*globalConnnection)->prepareStatement(
				"INSERT INTO mafiadata.gameplayers(gameId,uuid) VALUES( (SELECT gameId FROM mafiadata.games WHERE port = ? LIMIT 1) ,?);");
			prep_stmt->setUInt64(1, port);
			prep_stmt->setInt(2, playerid);
			prep_stmt->execute();
			delete stmt;
			delete prep_stmt;
		}
		void insertRole(uint64_t port, uint64_t roleConfig) {
			sql::Statement* stmt;
			sql::PreparedStatement* prep_stmt;		
			std::lock_guard<std::mutex> guard(*mutex_);
			stmt = (*globalConnnection)->createStatement();
			prep_stmt = (*globalConnnection)->prepareStatement(
				"INSERT INTO mafiadata.gameRoles(gameId,roleConfig) VALUES( (SELECT gameId FROM mafiadata.games WHERE port = ? LIMIT 1) ,?);");
			prep_stmt->setUInt64(1, port);
			prep_stmt->setInt(2, roleConfig);
			prep_stmt->execute();
			delete stmt;
			delete prep_stmt;
		}
		void insertWebsocket(uint64_t port, int websocketPort, uint64_t playerid) {
			sql::Statement* stmt;
			sql::PreparedStatement* prep_stmt;					
			std::lock_guard<std::mutex> guard(*mutex_);
			stmt = (*globalConnnection)->createStatement();
			prep_stmt = (*globalConnnection)->prepareStatement(
				"INSERT INTO mafiadata.playersocket(gameId,websocketPort,uuid) VALUES( (SELECT gameId FROM mafiadata.games WHERE port = ? LIMIT 1) ,?,?);");
			prep_stmt->setUInt64(1, port);
			prep_stmt->setInt(2, websocketPort);
			prep_stmt->setUInt64(3,playerid);
			prep_stmt->execute();
			delete stmt;
			delete prep_stmt;
		}
		void deleteWebsocket(uint64_t playerid) {
			sql::Statement* stmt;
			sql::PreparedStatement* prep_stmt;		
			std::lock_guard<std::mutex> guard(*mutex_);
			stmt = (*globalConnnection)->createStatement();
			prep_stmt = (*globalConnnection)->prepareStatement(
				"DELETE FROM mafiadata.playersocket WHERE uuid = ?;");
			prep_stmt->setUInt64(1, playerid);
			prep_stmt->execute();
			delete stmt;
			delete prep_stmt;
		}
		void deleteGamePlayers(uint64_t playerid) {
			sql::Statement* stmt;
			sql::PreparedStatement* prep_stmt;		
			std::lock_guard<std::mutex> guard(*mutex_);
			stmt = (*globalConnnection)->createStatement();
			prep_stmt = (*globalConnnection)->prepareStatement(
				"DELETE FROM mafiadata.gameplayers WHERE uuid = ?;");
			prep_stmt->setUInt64(1, playerid);
			prep_stmt->execute();
			delete stmt;
			delete prep_stmt;
		}
		bool verifyUser(uint64_t playerid, std::string cookieString) {
			sql::Statement* stmt;
			sql::ResultSet* res;
			sql::PreparedStatement* prep_stmt;		
			std::lock_guard<std::mutex> guard(*mutex_);
			stmt = (*globalConnnection)->createStatement();
			prep_stmt = (*globalConnnection)->prepareStatement(
				"SELECT playerid FROM mafiadata.usertable WHERE cookie = ? LIMIT 1;");
			prep_stmt->setString(1, cookieString.c_str());
			res = prep_stmt->executeQuery();
			res->next();
			if (res->getInt64("playerid") == playerid) {
				delete stmt;
				delete prep_stmt;
				delete res;
				return true;
			}
			else {
				delete stmt;
				delete prep_stmt;
				delete res;
				return false;
			}
		}
		void deleteGame() {
			sql::Statement* stmt;
			sql::PreparedStatement* prep_stmt;			
			std::lock_guard<std::mutex> guard(*mutex_);
			stmt = (*globalConnnection)->createStatement();
			prep_stmt = (*globalConnnection)->prepareStatement(
				"DELETE FROM mafiadata.games WHERE gameId = ?;");
			prep_stmt->setUInt64(1, gameID);
			prep_stmt->execute();
			delete stmt;
			delete prep_stmt;
		}
	private:
		std::shared_ptr<sql::Connection*> globalConnnection;
		uint64_t gameID;
		std::mutex * mutex_;
		void  makeConnnection(std::string DATABASE_IP, int DATABASE_PORT, std::string DATABASE_USER, std::string DATABASE_PASSWORD) {
			sql::mysql::MySQL_Driver* driver;
			sql::Connection* con;
			sql::ConnectOptionsMap connection_properties;
			connection_properties["hostName"] = DATABASE_IP;
			connection_properties["userName"] = DATABASE_USER;
			connection_properties["password"] = DATABASE_PASSWORD;
			connection_properties["schema"] = "mafiadata";
			connection_properties["port"] = DATABASE_PORT;
			connection_properties["OPT_RECONNECT"] = true;
			connection_properties["OPT_READ_TIMEOUT"] = 86400;
			connection_properties["OPT_WRITE_TIMEOUT"] = 86400;
			driver = sql::mysql::get_mysql_driver_instance();
			con = driver->connect(connection_properties);
			globalConnnection = std::make_shared<sql::Connection*>(con);
			this->mutex_ = new std::mutex();
		}
	};
}
