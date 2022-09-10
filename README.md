cd beyondmafia-engine-cpp directory

ensure required libraries are installed:
- mysql
- boost 1.75+
- mysqlcppconnector

change database configuration details in mafiaserver.h file.

use the following template command to compile:

g++ -fpermissive -o engine -Wl,-R ../boost_1_80_0/stage/lib -I ../boost_1_80_0 -I /usr/include/mysql-cppconn-8/ Database.h mafiaServer.h RoleConfig.h protocol.h Game.cpp Client.cpp Chatroom.cpp PersonInRoom.cpp TestSuite/TestSuite.cpp -L ../boost_1_80_0/stage/lib/ -L /usr/lib/ -lmysqlcppconn -lboost_thread -lboost_system -lpthread -lboost_json