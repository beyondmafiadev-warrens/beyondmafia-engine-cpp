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
		void insertGameData(uint64_t port, int maxPlayers, uint64_t settings) {
			sql::Statement* stmt;
			sql::PreparedStatement* prep_stmt;
			stmt = (*globalConnnection)->createStatement();
			prep_stmt = (*globalConnnection)->prepareStatement(
				"INSERT INTO mafiadata.games(port,maxPlayers,rankedGame,lockedGame,startedGame,gameEnded) VALUES(?,?,?,?,false,false)");
			prep_stmt->setUInt64(1, port);
			prep_stmt->setInt(2, maxPlayers);
			if (settings == 0) {
				prep_stmt->setBoolean(3, false);
				prep_stmt->setBoolean(4, false);
			}
			prep_stmt->execute();
			delete stmt;
			delete prep_stmt;
		}
		void insertPlayer(uint64_t port, uint64_t playerid) {
			sql::Statement* stmt;
			sql::PreparedStatement* prep_stmt;
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
			stmt = (*globalConnnection)->createStatement();
			prep_stmt = (*globalConnnection)->prepareStatement(
				"SELECT playerid FROM mafiadata.usertable WHERE cookie = ? LIMIT 1;");
			prep_stmt->setString(1, cookieString.c_str());
			res = prep_stmt->executeQuery();
			res->next();
			if (res->getInt64("playerid") == playerid) {
				delete stmt;
				delete prep_stmt;
				return true;
			}
			else {
				delete stmt;
				delete prep_stmt;
				return false;
			}
		}

	private:
		std::shared_ptr<sql::Connection*> globalConnnection;
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
		}
	};
}
