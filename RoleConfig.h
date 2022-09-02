#pragma once
enum ROLES : uint64_t {
	//0000
	VILLAGER = 0x0,
	//0001
	MAFIA = 0X1,
	//0010 
	DOCTOR = 0x2,
	//0100
	COP = 0x4,
	//1000 
	BULLETPROOF = 0x8,
	// 0001 0000
	ROLEBLOCKER = 0x10,
	//0010 0000
	LAWYER = 0x20
};

enum CYCLE : uint64_t {
	NIGHT = 0x0, 
	DAY = 0x1,
};

enum ITEM : uint64_t {
	//0000 
	SINGLE_USE = 0x0,
	//0001
	MULTI_USE = 0x1,
	//0010
	SAVE = 0x2,
	//0100
	MILLER = 0x4,
	//1000
	VEST = 0x8,
	//1 0000
	ROLEBLOCK = 0x10,

	LAW = 0x20, 


};