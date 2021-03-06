﻿#include "rosaserver.h"
#include <sys/mman.h>
#include <cerrno>

static Server* server;

static void pryMemory(void* address, size_t numPages) {
	size_t pageSize = sysconf(_SC_PAGE_SIZE);

	uintptr_t page = (uintptr_t)address;
	page -= (page % pageSize);

	if (mprotect((void*)page, pageSize * numPages, PROT_WRITE | PROT_READ) == 0) {
		std::ostringstream stream;

		stream << RS_PREFIX "Successfully pried open page at ";
		stream << std::showbase << std::hex;
		stream << static_cast<uintptr_t>(page);
		stream << "\n";

		Console::log(stream.str());
	} else {
		throw std::runtime_error(strerror(errno));
	}
}

void defineThreadSafeAPIs(sol::state* state) {
	state->open_libraries(sol::lib::base);
	state->open_libraries(sol::lib::package);
	state->open_libraries(sol::lib::coroutine);
	state->open_libraries(sol::lib::string);
	state->open_libraries(sol::lib::os);
	state->open_libraries(sol::lib::math);
	state->open_libraries(sol::lib::table);
	state->open_libraries(sol::lib::debug);
	state->open_libraries(sol::lib::bit32);
	state->open_libraries(sol::lib::io);
	state->open_libraries(sol::lib::ffi);
	state->open_libraries(sol::lib::jit);

	{
		auto meta = state->new_usertype<Vector>("new", sol::no_constructor);
		meta["x"] = &Vector::x;
		meta["y"] = &Vector::y;
		meta["z"] = &Vector::z;

		meta["class"] = sol::property(&Vector::getClass);
		meta["__tostring"] = &Vector::__tostring;
		meta["__add"] = &Vector::__add;
		meta["__sub"] = &Vector::__sub;
		meta["__mul"] = sol::overload(&Vector::__mul, &Vector::__mul_RotMatrix);
		meta["__div"] = &Vector::__div;
		meta["__unm"] = &Vector::__unm;
		meta["add"] = &Vector::add;
		meta["mult"] = &Vector::mult;
		meta["set"] = &Vector::set;
		meta["clone"] = &Vector::clone;
		meta["dist"] = &Vector::dist;
		meta["distSquare"] = &Vector::distSquare;
		meta["getBlockPos"] = &Vector::getBlockPos;
	}

	{
		auto meta = state->new_usertype<RotMatrix>("new", sol::no_constructor);
		meta["x1"] = &RotMatrix::x1;
		meta["y1"] = &RotMatrix::y1;
		meta["z1"] = &RotMatrix::z1;
		meta["x2"] = &RotMatrix::x2;
		meta["y2"] = &RotMatrix::y2;
		meta["z2"] = &RotMatrix::z2;
		meta["x3"] = &RotMatrix::x3;
		meta["y3"] = &RotMatrix::y3;
		meta["z3"] = &RotMatrix::z3;

		meta["class"] = sol::property(&RotMatrix::getClass);
		meta["__tostring"] = &RotMatrix::__tostring;
		meta["__mul"] = &RotMatrix::__mul;
		meta["set"] = &RotMatrix::set;
		meta["clone"] = &RotMatrix::clone;
		meta["getForward"] = &RotMatrix::getForward;
		meta["getUp"] = &RotMatrix::getUp;
		meta["getRight"] = &RotMatrix::getRight;
	}

	{
		auto meta = state->new_usertype<Image>("Image");
		meta["width"] = sol::property(&Image::getWidth);
		meta["height"] = sol::property(&Image::getHeight);
		meta["numChannels"] = sol::property(&Image::getNumChannels);
		meta["free"] = &Image::free;
		meta["loadFromFile"] = &Image::loadFromFile;
		meta["loadBlank"] = &Image::loadBlank;
		meta["getRGB"] = &Image::getRGB;
		meta["getRGBA"] = &Image::getRGBA;
		meta["setPixel"] = sol::overload(&Image::setRGB, &Image::setRGBA);
		meta["getPNG"] = &Image::getPNG;
	}

	{
		auto meta = state->new_usertype<LuaOpusEncoder>("OpusEncoder");
		meta["close"] = &LuaOpusEncoder::close;
		meta["open"] = &LuaOpusEncoder::open;
		meta["rewind"] = &LuaOpusEncoder::rewind;
		meta["encodeFrame"] = &LuaOpusEncoder::encodeFrame;
	}

	{
		auto meta = state->new_usertype<FileWatcher>("FileWatcher");
		meta["addWatch"] = &FileWatcher::addWatch;
		meta["removeWatch"] = &FileWatcher::removeWatch;
		meta["receiveEvent"] = &FileWatcher::receiveEvent;
	}

	{
		auto meta = state->new_usertype<SQLite>(
		    "SQLite", sol::constructors<SQLite(const char*)>());
		meta["close"] = &SQLite::close;
		meta["query"] = &SQLite::query;
	}

	(*state)["print"] = Lua::print;

	(*state)["Vector"] = sol::overload(Lua::Vector_, Lua::Vector_3f);
	(*state)["RotMatrix"] = Lua::RotMatrix_;

	(*state)["os"]["listDirectory"] = Lua::os::listDirectory;
	(*state)["os"]["createDirectory"] = Lua::os::createDirectory;
	(*state)["os"]["realClock"] = Lua::os::realClock;
	(*state)["os"]["exit"] = sol::overload(Lua::os::exit, Lua::os::exitCode);

	{
		auto httpTable = state->create_table();
		(*state)["http"] = httpTable;
		httpTable["getSync"] = Lua::http::getSync;
		httpTable["postSync"] = Lua::http::postSync;
	}

	(*state)["FILE_WATCH_ACCESS"] = IN_ACCESS;
	(*state)["FILE_WATCH_ATTRIB"] = IN_ATTRIB;
	(*state)["FILE_WATCH_CLOSE_WRITE"] = IN_CLOSE_WRITE;
	(*state)["FILE_WATCH_CLOSE_NOWRITE"] = IN_CLOSE_NOWRITE;
	(*state)["FILE_WATCH_CREATE"] = IN_CREATE;
	(*state)["FILE_WATCH_DELETE"] = IN_DELETE;
	(*state)["FILE_WATCH_DELETE_SELF"] = IN_DELETE_SELF;
	(*state)["FILE_WATCH_MODIFY"] = IN_MODIFY;
	(*state)["FILE_WATCH_MOVE_SELF"] = IN_MOVE_SELF;
	(*state)["FILE_WATCH_MOVED_FROM"] = IN_MOVED_FROM;
	(*state)["FILE_WATCH_MOVED_TO"] = IN_MOVED_TO;
	(*state)["FILE_WATCH_OPEN"] = IN_OPEN;
	(*state)["FILE_WATCH_MOVE"] = IN_MOVE;
	(*state)["FILE_WATCH_CLOSE"] = IN_CLOSE;
	(*state)["FILE_WATCH_DONT_FOLLOW"] = IN_DONT_FOLLOW;
	(*state)["FILE_WATCH_EXCL_UNLINK"] = IN_EXCL_UNLINK;
	(*state)["FILE_WATCH_MASK_ADD"] = IN_MASK_ADD;
	(*state)["FILE_WATCH_ONESHOT"] = IN_ONESHOT;
	(*state)["FILE_WATCH_ONLYDIR"] = IN_ONLYDIR;
	(*state)["FILE_WATCH_IGNORED"] = IN_IGNORED;
	(*state)["FILE_WATCH_ISDIR"] = IN_ISDIR;
	(*state)["FILE_WATCH_Q_OVERFLOW"] = IN_Q_OVERFLOW;
	(*state)["FILE_WATCH_UNMOUNT"] = IN_UNMOUNT;
}

void luaInit(bool redo) {
	std::lock_guard<std::mutex> guard(stateResetMutex);

	if (redo) {
		Console::log(LUA_PREFIX "Resetting state...\n");
		delete server;

		for (int i = 0; i < maxNumberOfAccounts; i++) {
			if (accountDataTables[i]) {
				delete accountDataTables[i];
				accountDataTables[i] = nullptr;
			}
		}

		for (int i = 0; i < maxNumberOfPlayers; i++) {
			if (playerDataTables[i]) {
				delete playerDataTables[i];
				playerDataTables[i] = nullptr;
			}
		}

		for (int i = 0; i < maxNumberOfHumans; i++) {
			if (humanDataTables[i]) {
				delete humanDataTables[i];
				humanDataTables[i] = nullptr;
			}
		}

		for (int i = 0; i < maxNumberOfItems; i++) {
			if (itemDataTables[i]) {
				delete itemDataTables[i];
				itemDataTables[i] = nullptr;
			}
		}

		for (int i = 0; i < maxNumberOfVehicles; i++) {
			if (vehicleDataTables[i]) {
				delete vehicleDataTables[i];
				vehicleDataTables[i] = nullptr;
			}
		}

		for (int i = 0; i < maxNumberOfRigidBodies; i++) {
			if (bodyDataTables[i]) {
				delete bodyDataTables[i];
				bodyDataTables[i] = nullptr;
			}
		}

		delete lua;
	} else {
		Console::log(LUA_PREFIX "Initializing state...\n");
	}

	lua = new sol::state();

	Console::log(LUA_PREFIX "Defining...\n");
	defineThreadSafeAPIs(lua);

	{
		auto meta = lua->new_usertype<Server>("new", sol::no_constructor);
		meta["TPS"] = &Server::TPS;

		meta["class"] = sol::property(&Server::getClass);
		meta["port"] = sol::property(&Server::getPort);
		meta["name"] = sol::property(&Server::getName, &Server::setName);
		meta["maxBytesPerSecond"] = sol::property(&Server::getMaxBytesPerSecond,
		                                          &Server::setMaxBytesPerSecond);
		meta["adminPassword"] =
		    sol::property(&Server::getAdminPassword, &Server::setAdminPassword);
		meta["password"] =
		    sol::property(&Server::getPassword, &Server::setPassword);
		meta["maxPlayers"] =
		    sol::property(&Server::getMaxPlayers, &Server::setMaxPlayers);

		meta["worldTraffic"] =
		    sol::property(&Server::getWorldTraffic, &Server::setWorldTraffic);
		meta["worldStartCash"] =
		    sol::property(&Server::getWorldStartCash, &Server::setWorldStartCash);
		meta["worldMinCash"] =
		    sol::property(&Server::getWorldMinCash, &Server::setWorldMinCash);
		meta["worldShowJoinExit"] = sol::property(&Server::getWorldShowJoinExit,
		                                          &Server::setWorldShowJoinExit);
		meta["worldRespawnTeam"] = sol::property(&Server::getWorldRespawnTeam,
		                                         &Server::setWorldRespawnTeam);
		meta["worldCrimeCivCiv"] = sol::property(&Server::getWorldCrimeCivCiv,
		                                         &Server::setWorldCrimeCivCiv);
		meta["worldCrimeCivTeam"] = sol::property(&Server::getWorldCrimeCivTeam,
		                                          &Server::setWorldCrimeCivTeam);
		meta["worldCrimeTeamCiv"] = sol::property(&Server::getWorldCrimeTeamCiv,
		                                          &Server::setWorldCrimeTeamCiv);
		meta["worldCrimeTeamTeam"] = sol::property(&Server::getWorldCrimeTeamTeam,
		                                           &Server::setWorldCrimeTeamTeam);
		meta["worldCrimeTeamTeamInBase"] =
		    sol::property(&Server::getWorldCrimeTeamTeamInBase,
		                  &Server::setWorldCrimeTeamTeamInBase);
		meta["worldCrimeNoSpawn"] = sol::property(&Server::getWorldCrimeNoSpawn,
		                                          &Server::setWorldCrimeNoSpawn);

		meta["roundRoundTime"] =
		    sol::property(&Server::getRoundRoundTime, &Server::setRoundRoundTime);
		meta["roundStartCash"] =
		    sol::property(&Server::getRoundStartCash, &Server::setRoundStartCash);
		meta["roundIsWeekly"] =
		    sol::property(&Server::getRoundIsWeekly, &Server::setRoundIsWeekly);
		meta["roundHasBonusRatio"] = sol::property(&Server::getRoundHasBonusRatio,
		                                           &Server::setRoundHasBonusRatio);
		meta["roundTeamDamage"] =
		    sol::property(&Server::getRoundTeamDamage, &Server::setRoundTeamDamage);

		meta["type"] = sol::property(&Server::getType, &Server::setType);
		meta["levelToLoad"] =
		    sol::property(&Server::getLevelName, &Server::setLevelName);
		meta["loadedLevel"] = sol::property(&Server::getLoadedLevelName);
		meta["isLevelLoaded"] =
		    sol::property(&Server::getIsLevelLoaded, &Server::setIsLevelLoaded);
		meta["gravity"] = sol::property(&Server::getGravity, &Server::setGravity);
		meta["defaultGravity"] = sol::property(&Server::getDefaultGravity);
		meta["state"] = sol::property(&Server::getState, &Server::setState);
		meta["time"] = sol::property(&Server::getTime, &Server::setTime);
		meta["sunTime"] = sol::property(&Server::getSunTime, &Server::setSunTime);
		meta["version"] = sol::property(&Server::getVersion);
		meta["versionMajor"] = sol::property(&Server::getVersionMajor);
		meta["versionMinor"] = sol::property(&Server::getVersionMinor);
		meta["numEvents"] = sol::property(&Server::getNumEvents);

		meta["setConsoleTitle"] = &Server::setConsoleTitle;
		meta["reset"] = &Server::reset;
	}

	server = new Server();
	(*lua)["server"] = server;

	{
		auto meta = lua->new_usertype<Connection>("new", sol::no_constructor);
		meta["port"] = &Connection::port;
		meta["timeoutTime"] = &Connection::timeoutTime;

		meta["class"] = sol::property(&Connection::getClass);
		meta["address"] = sol::property(&Connection::getAddress);
		meta["adminVisible"] = sol::property(&Connection::getAdminVisible,
		                                     &Connection::setAdminVisible);
		meta["spectatingHuman"] = sol::property(&Connection::getSpectatingHuman);

		meta["getEarShot"] = &Connection::getEarShot;
	}

	{
		auto meta = lua->new_usertype<Account>("new", sol::no_constructor);
		meta["subRosaID"] = &Account::subRosaID;
		meta["phoneNumber"] = &Account::phoneNumber;
		meta["money"] = &Account::money;
		meta["corporateRating"] = &Account::corporateRating;
		meta["criminalRating"] = &Account::criminalRating;
		meta["spawnTimer"] = &Account::spawnTimer;
		meta["playTime"] = &Account::playTime;
		meta["banTime"] = &Account::banTime;

		meta["class"] = sol::property(&Account::getClass);
		meta["__tostring"] = &Account::__tostring;
		meta["index"] = sol::property(&Account::getIndex);
		meta["data"] = sol::property(&Account::getDataTable);
		meta["name"] = sol::property(&Account::getName);
		meta["steamID"] = sol::property(&Account::getSteamID);
	}

	{
		auto meta = lua->new_usertype<Voice>("new", sol::no_constructor);
		meta["volumeLevel"] = &Voice::volumeLevel;
		meta["currentFrame"] = &Voice::currentFrame;

		meta["class"] = sol::property(&Voice::getClass);
		meta["isSilenced"] =
		    sol::property(&Voice::getIsSilenced, &Voice::setIsSilenced);

		meta["getFrame"] = &Voice::getFrame;
		meta["setFrame"] = &Voice::setFrame;
	}

	{
		auto meta = lua->new_usertype<Player>("new", sol::no_constructor);
		meta["subRosaID"] = &Player::subRosaID;
		meta["phoneNumber"] = &Player::phoneNumber;
		meta["money"] = &Player::money;
		meta["corporateRating"] = &Player::corporateRating;
		meta["criminalRating"] = &Player::criminalRating;
		meta["team"] = &Player::team;
		meta["teamSwitchTimer"] = &Player::teamSwitchTimer;
		meta["stocks"] = &Player::stocks;
		meta["spawnTimer"] = &Player::spawnTimer;
		meta["menuTab"] = &Player::menuTab;
		meta["numActions"] = &Player::numActions;
		meta["lastNumActions"] = &Player::lastNumActions;
		meta["numMenuButtons"] = &Player::numMenuButtons;
		meta["gender"] = &Player::gender;
		meta["skinColor"] = &Player::skinColor;
		meta["hairColor"] = &Player::hairColor;
		meta["hair"] = &Player::hair;
		meta["eyeColor"] = &Player::eyeColor;
		meta["model"] = &Player::model;
		meta["suitColor"] = &Player::suitColor;
		meta["tieColor"] = &Player::tieColor;
		meta["head"] = &Player::head;
		meta["necklace"] = &Player::necklace;

		meta["class"] = sol::property(&Player::getClass);
		meta["__tostring"] = &Player::__tostring;
		meta["index"] = sol::property(&Player::getIndex);
		meta["isActive"] =
		    sol::property(&Player::getIsActive, &Player::setIsActive);
		meta["data"] = sol::property(&Player::getDataTable);
		meta["name"] = sol::property(&Player::getName, &Player::setName);
		meta["isAdmin"] = sol::property(&Player::getIsAdmin, &Player::setIsAdmin);
		meta["isReady"] = sol::property(&Player::getIsReady, &Player::setIsReady);
		meta["isBot"] = sol::property(&Player::getIsBot, &Player::setIsBot);
		meta["isZombie"] =
		    sol::property(&Player::getIsZombie, &Player::setIsZombie);
		meta["human"] = sol::property(&Player::getHuman, &Player::setHuman);
		meta["connection"] = sol::property(&Player::getConnection);
		meta["account"] = sol::property(&Player::getAccount, &Player::setAccount);
		meta["voice"] = sol::property(&Player::getVoice);
		meta["botDestination"] =
		    sol::property(&Player::getBotDestination, &Player::setBotDestination);

		meta["getAction"] = &Player::getAction;
		meta["getMenuButton"] = &Player::getMenuButton;
		meta["update"] = &Player::update;
		meta["updateFinance"] = &Player::updateFinance;
		meta["remove"] = &Player::remove;
		meta["sendMessage"] = &Player::sendMessage;
	}

	{
		auto meta = lua->new_usertype<Human>("new", sol::no_constructor);
		meta["stamina"] = &Human::stamina;
		meta["maxStamina"] = &Human::maxStamina;
		meta["vehicleSeat"] = &Human::vehicleSeat;
		meta["despawnTime"] = &Human::despawnTime;
		meta["movementState"] = &Human::movementState;
		meta["zoomLevel"] = &Human::zoomLevel;
		meta["damage"] = &Human::damage;
		meta["pos"] = &Human::pos;
		meta["viewYaw"] = &Human::viewYaw;
		meta["viewPitch"] = &Human::viewPitch;
		meta["strafeInput"] = &Human::strafeInput;
		meta["walkInput"] = &Human::walkInput;
		meta["inputFlags"] = &Human::inputFlags;
		meta["lastInputFlags"] = &Human::lastInputFlags;
		meta["health"] = &Human::health;
		meta["bloodLevel"] = &Human::bloodLevel;
		meta["chestHP"] = &Human::chestHP;
		meta["headHP"] = &Human::headHP;
		meta["leftArmHP"] = &Human::leftArmHP;
		meta["rightArmHP"] = &Human::rightArmHP;
		meta["leftLegHP"] = &Human::leftLegHP;
		meta["rightLegHP"] = &Human::rightLegHP;
		meta["progressBar"] = &Human::progressBar;
		meta["inventoryAnimationFlags"] = &Human::inventoryAnimationFlags;
		meta["inventoryAnimationProgress"] = &Human::inventoryAnimationProgress;
		meta["inventoryAnimationDuration"] = &Human::inventoryAnimationDuration;
		meta["inventoryAnimationHand"] = &Human::inventoryAnimationHand;
		meta["inventoryAnimationSlot"] = &Human::inventoryAnimationSlot;
		meta["inventoryAnimationCounterFinished"] =
		    &Human::inventoryAnimationCounterFinished;
		meta["inventoryAnimationCounter"] = &Human::inventoryAnimationCounter;
		meta["gender"] = &Human::gender;
		meta["head"] = &Human::head;
		meta["skinColor"] = &Human::skinColor;
		meta["hairColor"] = &Human::hairColor;
		meta["hair"] = &Human::hair;
		meta["eyeColor"] = &Human::eyeColor;
		meta["model"] = &Human::model;
		meta["suitColor"] = &Human::suitColor;
		meta["tieColor"] = &Human::tieColor;
		meta["necklace"] = &Human::necklace;
		meta["lastUpdatedWantedGroup"] = &Human::lastUpdatedWantedGroup;

		meta["class"] = sol::property(&Human::getClass);
		meta["__tostring"] = &Human::__tostring;
		meta["index"] = sol::property(&Human::getIndex);
		meta["isActive"] = sol::property(&Human::getIsActive, &Human::setIsActive);
		meta["data"] = sol::property(&Human::getDataTable);
		meta["isAlive"] = sol::property(&Human::getIsAlive, &Human::setIsAlive);
		meta["isImmortal"] =
		    sol::property(&Human::getIsImmortal, &Human::setIsImmortal);
		meta["isOnGround"] = sol::property(&Human::getIsOnGround);
		meta["isStanding"] = sol::property(&Human::getIsStanding);
		meta["isBleeding"] =
		    sol::property(&Human::getIsBleeding, &Human::setIsBleeding);
		meta["player"] = sol::property(&Human::getPlayer, &Human::setPlayer);
		meta["account"] = sol::property(&Human::getAccount, &Human::setAccount);
		meta["vehicle"] = sol::property(&Human::getVehicle, &Human::setVehicle);

		meta["remove"] = &Human::remove;
		meta["teleport"] = &Human::teleport;
		meta["speak"] = &Human::speak;
		meta["arm"] = &Human::arm;
		meta["getBone"] = &Human::getBone;
		meta["getRigidBody"] = &Human::getRigidBody;
		meta["getInventorySlot"] = &Human::getInventorySlot;
		meta["setVelocity"] = &Human::setVelocity;
		meta["addVelocity"] = &Human::addVelocity;
		meta["mountItem"] = &Human::mountItem;
		meta["applyDamage"] = &Human::applyDamage;
	}

	{
		auto meta = lua->new_usertype<ItemType>("new", sol::no_constructor);
		meta["price"] = &ItemType::price;
		meta["mass"] = &ItemType::mass;
		meta["fireRate"] = &ItemType::fireRate;
		meta["magazineAmmo"] = &ItemType::magazineAmmo;
		meta["bulletType"] = &ItemType::bulletType;
		meta["bulletVelocity"] = &ItemType::bulletVelocity;
		meta["bulletSpread"] = &ItemType::bulletSpread;
		meta["numHands"] = &ItemType::numHands;
		meta["rightHandPos"] = &ItemType::rightHandPos;
		meta["leftHandPos"] = &ItemType::leftHandPos;
		meta["primaryGripStiffness"] = &ItemType::primaryGripStiffness;
		meta["primaryGripRotation"] = &ItemType::primaryGripRotation;
		meta["secondaryGripStiffness"] = &ItemType::secondaryGripStiffness;
		meta["secondaryGripRotation"] = &ItemType::secondaryGripRotation;
		meta["boundsCenter"] = &ItemType::boundsCenter;
		meta["gunHoldingPos"] = &ItemType::gunHoldingPos;

		meta["class"] = sol::property(&ItemType::getClass);
		meta["__tostring"] = &ItemType::__tostring;
		meta["index"] = sol::property(&ItemType::getIndex);
		meta["name"] = sol::property(&ItemType::getName, &ItemType::setName);
		meta["isGun"] = sol::property(&ItemType::getIsGun, &ItemType::setIsGun);

		meta["getCanMountTo"] = &ItemType::getCanMountTo;
		meta["setCanMountTo"] = &ItemType::setCanMountTo;
	}

	{
		auto meta = lua->new_usertype<Item>("new", sol::no_constructor);
		meta["physicsSettledTimer"] = &Item::physicsSettledTimer;
		meta["despawnTime"] = &Item::despawnTime;
		meta["parentSlot"] = &Item::parentSlot;
		meta["pos"] = &Item::pos;
		meta["vel"] = &Item::vel;
		meta["rot"] = &Item::rot;
		meta["bullets"] = &Item::bullets;
		meta["phoneNumber"] = &Item::phoneNumber;
		meta["displayPhoneNumber"] = &Item::displayPhoneNumber;
		meta["enteredPhoneNumber"] = &Item::enteredPhoneNumber;
		meta["phoneTexture"] = &Item::phoneTexture;
		meta["computerCurrentLine"] = &Item::computerCurrentLine;
		meta["computerTopLine"] = &Item::computerTopLine;
		meta["computerCursor"] = &Item::computerCursor;

		meta["class"] = sol::property(&Item::getClass);
		meta["__tostring"] = &Item::__tostring;
		meta["index"] = sol::property(&Item::getIndex);
		meta["isActive"] = sol::property(&Item::getIsActive, &Item::setIsActive);
		meta["data"] = sol::property(&Item::getDataTable);
		meta["hasPhysics"] =
		    sol::property(&Item::getHasPhysics, &Item::setHasPhysics);
		meta["physicsSettled"] =
		    sol::property(&Item::getPhysicsSettled, &Item::setPhysicsSettled);
		meta["isStatic"] = sol::property(&Item::getIsStatic, &Item::setIsStatic);
		meta["type"] = sol::property(&Item::getType, &Item::setType);
		meta["rigidBody"] = sol::property(&Item::getRigidBody);
		meta["connectedPhone"] =
		    sol::property(&Item::getConnectedPhone, &Item::setConnectedPhone);
		meta["vehicle"] = sol::property(&Item::getVehicle, &Item::setVehicle);
		meta["grenadePrimer"] =
		    sol::property(&Item::getGrenadePrimer, &Item::setGrenadePrimer);
		meta["parentHuman"] = sol::property(&Item::getParentHuman);
		meta["parentItem"] = sol::property(&Item::getParentItem);

		meta["remove"] = &Item::remove;
		meta["mountItem"] = &Item::mountItem;
		meta["unmount"] = &Item::unmount;
		meta["speak"] = &Item::speak;
		meta["explode"] = &Item::explode;
		meta["setMemo"] = &Item::setMemo;
		meta["computerTransmitLine"] = &Item::computerTransmitLine;
		meta["computerIncrementLine"] = &Item::computerIncrementLine;
		meta["computerSetLine"] = &Item::computerSetLine;
		meta["computerSetLineColors"] = &Item::computerSetLineColors;
		meta["computerSetColor"] = &Item::computerSetColor;
	}

	{
		auto meta = lua->new_usertype<VehicleType>("new", sol::no_constructor);
		meta["controllableState"] = &VehicleType::controllableState;
		meta["price"] = &VehicleType::price;
		meta["mass"] = &VehicleType::mass;

		meta["class"] = sol::property(&VehicleType::getClass);
		meta["__tostring"] = &VehicleType::__tostring;
		meta["index"] = sol::property(&VehicleType::getIndex);
		meta["name"] = sol::property(&VehicleType::getName, &VehicleType::setName);
		meta["usesExternalModel"] =
		    sol::property(&VehicleType::getUsesExternalModel);
	}

	{
		auto meta = lua->new_usertype<Vehicle>("new", sol::no_constructor);
		meta["controllableState"] = &Vehicle::controllableState;
		meta["health"] = &Vehicle::health;
		meta["color"] = &Vehicle::color;
		meta["pos"] = &Vehicle::pos;
		meta["pos2"] = &Vehicle::pos2;
		meta["rot"] = &Vehicle::rot;
		meta["vel"] = &Vehicle::vel;
		meta["gearX"] = &Vehicle::gearX;
		meta["steerControl"] = &Vehicle::steerControl;
		meta["gearY"] = &Vehicle::gearY;
		meta["gasControl"] = &Vehicle::gasControl;
		meta["engineRPM"] = &Vehicle::engineRPM;
		meta["bladeBodyID"] = &Vehicle::bladeBodyID;

		meta["class"] = sol::property(&Vehicle::getClass);
		meta["__tostring"] = &Vehicle::__tostring;
		meta["index"] = sol::property(&Vehicle::getIndex);
		meta["isActive"] =
		    sol::property(&Vehicle::getIsActive, &Vehicle::setIsActive);
		meta["type"] = sol::property(&Vehicle::getType, &Vehicle::setType);
		meta["isLocked"] =
		    sol::property(&Vehicle::getIsLocked, &Vehicle::setIsLocked);
		meta["data"] = sol::property(&Vehicle::getDataTable);
		meta["lastDriver"] = sol::property(&Vehicle::getLastDriver);
		meta["rigidBody"] = sol::property(&Vehicle::getRigidBody);

		meta["updateType"] = &Vehicle::updateType;
		meta["updateDestruction"] = &Vehicle::updateDestruction;
		meta["remove"] = &Vehicle::remove;
		meta["getIsWindowBroken"] = &Vehicle::getIsWindowBroken;
		meta["setIsWindowBroken"] = &Vehicle::setIsWindowBroken;
	}

	{
		auto meta = lua->new_usertype<Bullet>("new", sol::no_constructor);
		meta["type"] = &Bullet::type;
		meta["time"] = &Bullet::time;
		meta["lastPos"] = &Bullet::lastPos;
		meta["pos"] = &Bullet::pos;
		meta["vel"] = &Bullet::vel;

		meta["class"] = sol::property(&Bullet::getClass);
		meta["player"] = sol::property(&Bullet::getPlayer);
	}

	{
		auto meta = lua->new_usertype<Bone>("new", sol::no_constructor);
		meta["pos"] = &Bone::pos;
		meta["pos2"] = &Bone::pos2;

		meta["class"] = sol::property(&Bone::getClass);
	}

	{
		auto meta = lua->new_usertype<RigidBody>("new", sol::no_constructor);
		meta["type"] = &RigidBody::type;
		meta["unk0"] = &RigidBody::unk0;
		meta["mass"] = &RigidBody::mass;
		meta["pos"] = &RigidBody::pos;
		meta["vel"] = &RigidBody::vel;
		meta["rot"] = &RigidBody::rot;
		meta["rotVel"] = &RigidBody::rotVel;

		meta["class"] = sol::property(&RigidBody::getClass);
		meta["__tostring"] = &RigidBody::__tostring;
		meta["index"] = sol::property(&RigidBody::getIndex);
		meta["isActive"] =
		    sol::property(&RigidBody::getIsActive, &RigidBody::setIsActive);
		meta["data"] = sol::property(&RigidBody::getDataTable);
		meta["isSettled"] =
		    sol::property(&RigidBody::getIsSettled, &RigidBody::setIsSettled);

		meta["bondTo"] = &RigidBody::bondTo;
		meta["bondRotTo"] = &RigidBody::bondRotTo;
		meta["bondToLevel"] = &RigidBody::bondToLevel;
		meta["collideLevel"] = &RigidBody::collideLevel;
	}

	{
		auto meta = lua->new_usertype<InventorySlot>("new", sol::no_constructor);
		meta["count"] = &InventorySlot::count;

		meta["class"] = sol::property(&InventorySlot::getClass);

		meta["primaryItem"] = sol::property(&InventorySlot::getPrimaryItem);
		meta["secondaryItem"] = sol::property(&InventorySlot::getSecondaryItem);
	}

	{
		auto meta = lua->new_usertype<Bond>("new", sol::no_constructor);
		meta["type"] = &Bond::type;
		meta["despawnTime"] = &Bond::despawnTime;
		meta["globalPos"] = &Bond::globalPos;
		meta["localPos"] = &Bond::localPos;
		meta["otherLocalPos"] = &Bond::otherLocalPos;

		meta["class"] = sol::property(&Bond::getClass);
		meta["__tostring"] = &Bond::__tostring;
		meta["index"] = sol::property(&Bond::getIndex);
		meta["isActive"] = sol::property(&Bond::getIsActive, &Bond::setIsActive);
		meta["body"] = sol::property(&Bond::getBody);
		meta["otherBody"] = sol::property(&Bond::getOtherBody);
	}

	{
		auto meta = lua->new_usertype<Action>("new", sol::no_constructor);
		meta["type"] = &Action::type;
		meta["a"] = &Action::a;
		meta["b"] = &Action::b;
		meta["c"] = &Action::c;
		meta["d"] = &Action::d;

		meta["class"] = sol::property(&Action::getClass);
	}

	{
		auto meta = lua->new_usertype<MenuButton>("new", sol::no_constructor);
		meta["id"] = &MenuButton::id;
		meta["text"] = sol::property(&MenuButton::getText, &MenuButton::setText);

		meta["class"] = sol::property(&MenuButton::getClass);
	}

	{
		auto meta = lua->new_usertype<EarShot>("new", sol::no_constructor);
		meta["distance"] = &EarShot::distance;
		meta["volume"] = &EarShot::volume;

		meta["class"] = sol::property(&EarShot::getClass);
		meta["isActive"] =
		    sol::property(&EarShot::getIsActive, &EarShot::setIsActive);
		meta["player"] = sol::property(&EarShot::getPlayer, &EarShot::setPlayer);
		meta["human"] = sol::property(&EarShot::getHuman, &EarShot::setHuman);
		meta["receivingItem"] =
		    sol::property(&EarShot::getReceivingItem, &EarShot::setReceivingItem);
		meta["transmittingItem"] = sol::property(&EarShot::getTransmittingItem,
		                                         &EarShot::setTransmittingItem);
	}

	{
		auto meta = lua->new_usertype<Worker>(
		    "Worker", sol::constructors<Worker(std::string)>());
		meta["stop"] = &Worker::stop;
		meta["sendMessage"] = &Worker::sendMessage;
		meta["receiveMessage"] = &Worker::receiveMessage;
	}

	{
		auto meta = lua->new_usertype<ChildProcess>(
		    "ChildProcess", sol::constructors<ChildProcess(std::string)>());
		meta["isRunning"] = &ChildProcess::isRunning;
		meta["terminate"] = &ChildProcess::terminate;
		meta["getExitCode"] = &ChildProcess::getExitCode;
		meta["receiveMessage"] = &ChildProcess::receiveMessage;
		meta["sendMessage"] = &ChildProcess::sendMessage;
		meta["setCPULimit"] = &ChildProcess::setCPULimit;
		meta["setMemoryLimit"] = &ChildProcess::setMemoryLimit;
		meta["setFileSizeLimit"] = &ChildProcess::setFileSizeLimit;
		meta["getPriority"] = &ChildProcess::getPriority;
		meta["setPriority"] = &ChildProcess::setPriority;
	}

	{
		auto meta = lua->new_usertype<StreetLane>("new", sol::no_constructor);
		meta["direction"] = &StreetLane::direction;
		meta["posA"] = &StreetLane::posA;
		meta["posB"] = &StreetLane::posB;

		meta["class"] = sol::property(&StreetLane::getClass);
	}

	{
		auto meta = lua->new_usertype<Street>("new", sol::no_constructor);
		meta["trafficCuboidA"] = &Street::trafficCuboidA;
		meta["trafficCuboidB"] = &Street::trafficCuboidB;
		meta["numTraffic"] = &Street::numTraffic;

		meta["class"] = sol::property(&Street::getClass);
		meta["__tostring"] = &Street::__tostring;
		meta["index"] = sol::property(&Street::getIndex);
		meta["name"] = sol::property(&Street::getName);
		meta["intersectionA"] = sol::property(&Street::getIntersectionA);
		meta["intersectionB"] = sol::property(&Street::getIntersectionB);
		meta["numLanes"] = sol::property(&Street::getNumLanes);

		meta["getLane"] = &Street::getLane;
	}

	{
		auto meta =
		    lua->new_usertype<StreetIntersection>("new", sol::no_constructor);
		meta["pos"] = &StreetIntersection::pos;
		meta["lightsState"] = &StreetIntersection::lightsState;
		meta["lightsTimer"] = &StreetIntersection::lightsTimer;
		meta["lightsTimerMax"] = &StreetIntersection::lightsTimerMax;
		meta["lightEast"] = &StreetIntersection::lightEast;
		meta["lightSouth"] = &StreetIntersection::lightSouth;
		meta["lightWest"] = &StreetIntersection::lightWest;
		meta["lightNorth"] = &StreetIntersection::lightNorth;

		meta["class"] = sol::property(&StreetIntersection::getClass);
		meta["__tostring"] = &StreetIntersection::__tostring;
		meta["index"] = sol::property(&StreetIntersection::getIndex);
		meta["streetEast"] = sol::property(&StreetIntersection::getStreetEast);
		meta["streetSouth"] = sol::property(&StreetIntersection::getStreetSouth);
		meta["streetWest"] = sol::property(&StreetIntersection::getStreetWest);
		meta["streetNorth"] = sol::property(&StreetIntersection::getStreetNorth);
	}

	{
		auto meta = lua->new_usertype<ShopCar>("new", sol::no_constructor);
		meta["price"] = &ShopCar::price;
		meta["color"] = &ShopCar::color;

		meta["class"] = sol::property(&ShopCar::getClass);
		meta["type"] = sol::property(&ShopCar::getType, &ShopCar::setType);
	}

	{
		auto meta = lua->new_usertype<Building>("new", sol::no_constructor);
		meta["type"] = &Building::type;
		meta["pos"] = &Building::pos;
		meta["spawnRot"] = &Building::spawnRot;
		meta["interiorCuboidA"] = &Building::interiorCuboidA;
		meta["interiorCuboidB"] = &Building::interiorCuboidB;
		meta["numShopCars"] = &Building::numShopCars;
		meta["shopCarSales"] = &Building::shopCarSales;

		meta["class"] = sol::property(&Building::getClass);
		meta["__tostring"] = &Building::__tostring;
		meta["index"] = sol::property(&Building::getIndex);

		meta["getShopCar"] = &Building::getShopCar;
	}

	{
		auto meta = lua->new_usertype<Hooks::Float>("new", sol::no_constructor);
		meta["value"] = &Hooks::Float::value;
	}

	{
		auto meta = lua->new_usertype<Hooks::Integer>("new", sol::no_constructor);
		meta["value"] = &Hooks::Integer::value;
	}

	{
		auto meta =
		    lua->new_usertype<Hooks::UnsignedInteger>("new", sol::no_constructor);
		meta["value"] = &Hooks::UnsignedInteger::value;
	}

	(*lua)["flagStateForReset"] = Lua::flagStateForReset;

	{
		auto hookTable = lua->create_table();
		(*lua)["hook"] = hookTable;
		hookTable["persistentMode"] = hookMode;
		hookTable["enable"] = Lua::hook::enable;
		hookTable["disable"] = Lua::hook::disable;
		hookTable["clear"] = Lua::hook::clear;
		Lua::hook::clear();
	}

	{
		auto eventTable = lua->create_table();
		(*lua)["event"] = eventTable;
		eventTable["sound"] =
		    sol::overload(Lua::event::sound, Lua::event::soundSimple);
		eventTable["explosion"] = Lua::event::explosion;
		eventTable["bullet"] = Lua::event::bullet;
		eventTable["bulletHit"] = Lua::event::bulletHit;
	}

	{
		auto physicsTable = lua->create_table();
		(*lua)["physics"] = physicsTable;
		physicsTable["lineIntersectLevel"] = Lua::physics::lineIntersectLevel;
		physicsTable["lineIntersectHuman"] = Lua::physics::lineIntersectHuman;
		physicsTable["lineIntersectVehicle"] = Lua::physics::lineIntersectVehicle;
		physicsTable["lineIntersectLevelQuick"] =
		    Lua::physics::lineIntersectLevelQuick;
		physicsTable["lineIntersectHumanQuick"] =
		    Lua::physics::lineIntersectHumanQuick;
		physicsTable["lineIntersectVehicleQuick"] =
		    Lua::physics::lineIntersectVehicleQuick;
		physicsTable["lineIntersectAnyQuick"] = Lua::physics::lineIntersectAnyQuick;
		physicsTable["lineIntersectTriangle"] = Lua::physics::lineIntersectTriangle;
		physicsTable["garbageCollectBullets"] = Lua::physics::garbageCollectBullets;
		physicsTable["createBlock"] = Lua::physics::createBlock;
		physicsTable["deleteBlock"] = Lua::physics::deleteBlock;
	}

	{
		auto chatTable = lua->create_table();
		(*lua)["chat"] = chatTable;
		chatTable["announce"] = Lua::chat::announce;
		chatTable["tellAdmins"] = Lua::chat::tellAdmins;
		chatTable["addRaw"] = Lua::chat::addRaw;
	}

	{
		auto accountsTable = lua->create_table();
		(*lua)["accounts"] = accountsTable;
		accountsTable["save"] = Lua::accounts::save;
		accountsTable["getCount"] = Lua::accounts::getCount;
		accountsTable["getAll"] = Lua::accounts::getAll;
		accountsTable["getByPhone"] = Lua::accounts::getByPhone;

		sol::table _meta = lua->create_table();
		accountsTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::accounts::getCount;
		_meta["__index"] = Lua::accounts::getByIndex;
	}

	{
		auto playersTable = lua->create_table();
		(*lua)["players"] = playersTable;
		playersTable["getCount"] = Lua::players::getCount;
		playersTable["getAll"] = Lua::players::getAll;
		playersTable["getByPhone"] = Lua::players::getByPhone;
		playersTable["getNonBots"] = Lua::players::getNonBots;
		playersTable["createBot"] = Lua::players::createBot;

		sol::table _meta = lua->create_table();
		playersTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::players::getCount;
		_meta["__index"] = Lua::players::getByIndex;
	}

	{
		auto humansTable = lua->create_table();
		(*lua)["humans"] = humansTable;
		humansTable["getCount"] = Lua::humans::getCount;
		humansTable["getAll"] = Lua::humans::getAll;
		humansTable["create"] = Lua::humans::create;

		sol::table _meta = lua->create_table();
		humansTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::humans::getCount;
		_meta["__index"] = Lua::humans::getByIndex;
	}

	{
		auto itemTypesTable = lua->create_table();
		(*lua)["itemTypes"] = itemTypesTable;
		itemTypesTable["getCount"] = Lua::itemTypes::getCount;
		itemTypesTable["getAll"] = Lua::itemTypes::getAll;
		itemTypesTable["getByName"] = Lua::itemTypes::getByName;

		sol::table _meta = lua->create_table();
		itemTypesTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::itemTypes::getCount;
		_meta["__index"] = Lua::itemTypes::getByIndex;
	}

	{
		auto itemsTable = lua->create_table();
		(*lua)["items"] = itemsTable;
		itemsTable["getCount"] = Lua::items::getCount;
		itemsTable["getAll"] = Lua::items::getAll;
		itemsTable["create"] =
		    sol::overload(Lua::items::create, Lua::items::createVel);
		itemsTable["createRope"] = Lua::items::createRope;

		sol::table _meta = lua->create_table();
		itemsTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::items::getCount;
		_meta["__index"] = Lua::items::getByIndex;
	}

	{
		auto vehicleTypesTable = lua->create_table();
		(*lua)["vehicleTypes"] = vehicleTypesTable;
		vehicleTypesTable["getCount"] = Lua::vehicleTypes::getCount;
		vehicleTypesTable["getAll"] = Lua::vehicleTypes::getAll;
		vehicleTypesTable["getByName"] = Lua::vehicleTypes::getByName;

		sol::table _meta = lua->create_table();
		vehicleTypesTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::vehicleTypes::getCount;
		_meta["__index"] = Lua::vehicleTypes::getByIndex;
	}

	{
		auto vehiclesTable = lua->create_table();
		(*lua)["vehicles"] = vehiclesTable;
		vehiclesTable["getCount"] = Lua::vehicles::getCount;
		vehiclesTable["getAll"] = Lua::vehicles::getAll;
		vehiclesTable["create"] =
		    sol::overload(Lua::vehicles::create, Lua::vehicles::createVel);

		sol::table _meta = lua->create_table();
		vehiclesTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::vehicles::getCount;
		_meta["__index"] = Lua::vehicles::getByIndex;
	}

	{
		auto bulletsTable = lua->create_table();
		(*lua)["bullets"] = bulletsTable;
		bulletsTable["getCount"] = Lua::bullets::getCount;
		bulletsTable["getAll"] = Lua::bullets::getAll;
		bulletsTable["create"] = Lua::bullets::create;
	}

	{
		auto rigidBodiesTable = lua->create_table();
		(*lua)["rigidBodies"] = rigidBodiesTable;
		rigidBodiesTable["getCount"] = Lua::rigidBodies::getCount;
		rigidBodiesTable["getAll"] = Lua::rigidBodies::getAll;

		sol::table _meta = lua->create_table();
		rigidBodiesTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::rigidBodies::getCount;
		_meta["__index"] = Lua::rigidBodies::getByIndex;
	}

	{
		auto bondsTable = lua->create_table();
		(*lua)["bonds"] = bondsTable;
		bondsTable["getCount"] = Lua::bonds::getCount;
		bondsTable["getAll"] = Lua::bonds::getAll;

		sol::table _meta = lua->create_table();
		bondsTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::bonds::getCount;
		_meta["__index"] = Lua::bonds::getByIndex;
	}

	{
		auto streetsTable = lua->create_table();
		(*lua)["streets"] = streetsTable;
		streetsTable["getCount"] = Lua::streets::getCount;
		streetsTable["getAll"] = Lua::streets::getAll;

		sol::table _meta = lua->create_table();
		streetsTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::streets::getCount;
		_meta["__index"] = Lua::streets::getByIndex;
	}

	{
		auto intersectionsTable = lua->create_table();
		(*lua)["intersections"] = intersectionsTable;
		intersectionsTable["getCount"] = Lua::intersections::getCount;
		intersectionsTable["getAll"] = Lua::intersections::getAll;

		sol::table _meta = lua->create_table();
		intersectionsTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::intersections::getCount;
		_meta["__index"] = Lua::intersections::getByIndex;
	}

	{
		auto buildingsTable = lua->create_table();
		(*lua)["buildings"] = buildingsTable;
		buildingsTable["getCount"] = Lua::buildings::getCount;
		buildingsTable["getAll"] = Lua::buildings::getAll;

		sol::table _meta = lua->create_table();
		buildingsTable[sol::metatable_key] = _meta;
		_meta["__len"] = Lua::buildings::getCount;
		_meta["__index"] = Lua::buildings::getByIndex;
	}

	{
		auto memoryTable = lua->create_table();
		(*lua)["memory"] = memoryTable;
		memoryTable["getBaseAddress"] = Lua::memory::getBaseAddress;
		memoryTable["getAddress"] = sol::overload(
		    &Lua::memory::getAddressOfConnection, &Lua::memory::getAddressOfAccount,
		    &Lua::memory::getAddressOfPlayer, &Lua::memory::getAddressOfHuman,
		    &Lua::memory::getAddressOfItemType, &Lua::memory::getAddressOfItem,
		    &Lua::memory::getAddressOfVehicleType,
		    &Lua::memory::getAddressOfVehicle, &Lua::memory::getAddressOfBullet,
		    &Lua::memory::getAddressOfBone, &Lua::memory::getAddressOfRigidBody,
		    &Lua::memory::getAddressOfBond, &Lua::memory::getAddressOfAction,
		    &Lua::memory::getAddressOfMenuButton,
		    &Lua::memory::getAddressOfStreetLane, &Lua::memory::getAddressOfStreet,
		    &Lua::memory::getAddressOfStreetIntersection,
		    &Lua::memory::getAddressOfInventorySlot);
		memoryTable["readByte"] = Lua::memory::readByte;
		memoryTable["readUByte"] = Lua::memory::readUByte;
		memoryTable["readShort"] = Lua::memory::readShort;
		memoryTable["readUShort"] = Lua::memory::readUShort;
		memoryTable["readInt"] = Lua::memory::readInt;
		memoryTable["readUInt"] = Lua::memory::readUInt;
		memoryTable["readLong"] = Lua::memory::readLong;
		memoryTable["readULong"] = Lua::memory::readULong;
		memoryTable["readFloat"] = Lua::memory::readFloat;
		memoryTable["readDouble"] = Lua::memory::readDouble;
		memoryTable["readBytes"] = Lua::memory::readBytes;
		memoryTable["writeByte"] = Lua::memory::writeByte;
		memoryTable["writeUByte"] = Lua::memory::writeUByte;
		memoryTable["writeShort"] = Lua::memory::writeShort;
		memoryTable["writeUShort"] = Lua::memory::writeUShort;
		memoryTable["writeInt"] = Lua::memory::writeInt;
		memoryTable["writeUInt"] = Lua::memory::writeUInt;
		memoryTable["writeLong"] = Lua::memory::writeLong;
		memoryTable["writeULong"] = Lua::memory::writeULong;
		memoryTable["writeFloat"] = Lua::memory::writeFloat;
		memoryTable["writeDouble"] = Lua::memory::writeDouble;
		memoryTable["writeBytes"] = Lua::memory::writeBytes;
	}

	(*lua)["RESET_REASON_BOOT"] = RESET_REASON_BOOT;
	(*lua)["RESET_REASON_ENGINECALL"] = RESET_REASON_ENGINECALL;
	(*lua)["RESET_REASON_LUARESET"] = RESET_REASON_LUARESET;
	(*lua)["RESET_REASON_LUACALL"] = RESET_REASON_LUACALL;

	(*lua)["STATE_PREGAME"] = 1;
	(*lua)["STATE_GAME"] = 2;
	(*lua)["STATE_RESTARTING"] = 3;

	(*lua)["TYPE_DRIVING"] = 1;
	(*lua)["TYPE_RACE"] = 2;
	(*lua)["TYPE_ROUND"] = 3;
	(*lua)["TYPE_WORLD"] = 4;
	(*lua)["TYPE_TERMINATOR"] = 5;
	(*lua)["TYPE_COOP"] = 6;
	(*lua)["TYPE_VERSUS"] = 7;

	Console::log(LUA_PREFIX "Running " LUA_ENTRY_FILE "...\n");

	sol::load_result load = lua->load_file(LUA_ENTRY_FILE);
	if (noLuaCallError(&load)) {
		sol::protected_function_result res = load();
		if (noLuaCallError(&res)) {
			Console::log(LUA_PREFIX "No problems!\n");
		}
	}
}

static inline uintptr_t getBaseAddress() {
	std::ifstream file("/proc/self/maps");
	std::string line;
	// First line
	std::getline(file, line);
	auto pos = line.find("-");
	auto truncated = line.substr(0, pos);

	return std::stoul(truncated, nullptr, 16);
}

static inline void printBaseAddress(uintptr_t base) {
	std::ostringstream stream;

	stream << RS_PREFIX "Base address is ";
	stream << std::showbase << std::hex;
	stream << base;
	stream << "\n";

	Console::log(stream.str());
}

static inline void locateMemory(uintptr_t base) {
	Engine::version = (unsigned int*)(base + 0x2e6f08);
	Engine::subVersion = (unsigned int*)(base + 0x2e6f04);
	Engine::serverName = (char*)(base + 0x1fed72d4);
	Engine::serverPort = (unsigned int*)(base + 0x17b9e720);
	Engine::numEvents = (unsigned int*)(base + 0x443f1c64);
	Engine::packetSize = (int*)(base + 0x36e60d5c);
	Engine::packet = (unsigned char*)(base + 0x36e60d64);
	Engine::serverMaxBytesPerSecond = (int*)(base + 0x17b9e724);
	Engine::adminPassword = (char*)(base + 0x17b9eb2c);
	Engine::isPassworded = (int*)(base + 0x1fed76f0);
	Engine::password = (char*)(base + 0x17b9ed2c);
	Engine::maxPlayers = (int*)(base + 0x1fed76f4);

	Engine::World::traffic = (int*)(base + 0x43570ec0);
	Engine::World::startCash = (int*)(base + 0x43570ee8);
	Engine::World::minCash = (int*)(base + 0x43570eec);
	Engine::World::showJoinExit = (bool*)(base + 0x43570ef0);
	Engine::World::respawnTeam = (bool*)(base + 0x43570ef4);
	Engine::World::Crime::civCiv = (int*)(base + 0x43570ec4);
	Engine::World::Crime::civTeam = (int*)(base + 0x43570ec8);
	Engine::World::Crime::teamCiv = (int*)(base + 0x43570ecc);
	Engine::World::Crime::teamTeam = (int*)(base + 0x43570ed0);
	Engine::World::Crime::teamTeamInBase = (int*)(base + 0x43570ed4);
	Engine::World::Crime::noSpawn = (int*)(base + 0x43570ee0);

	Engine::Round::roundTime = (int*)(base + 0x43570eac);
	Engine::Round::startCash = (int*)(base + 0x43570eb0);
	Engine::Round::weekly = (bool*)(base + 0x43570eb4);
	Engine::Round::bonusRatio = (bool*)(base + 0x43570eb8);
	Engine::Round::teamDamage = (int*)(base + 0x43570ebc);

	Engine::gameType = (int*)(base + 0x434b63ac);
	Engine::mapName = (char*)(base + 0x434b63b0);
	Engine::loadedMapName = (char*)(base + 0x37281204);
	Engine::gameState = (int*)(base + 0x434b65cc);
	Engine::gameTimer = (int*)(base + 0x434b65d4);
	Engine::sunTime = (unsigned int*)(base + 0xcda06e0);
	Engine::isLevelLoaded = (int*)(base + 0x37281200);
	Engine::gravity = (float*)(base + 0xd5b98);
	pryMemory(Engine::gravity, 1);
	Engine::originalGravity = *Engine::gravity;

	Engine::lineIntersectResult = (LineIntersectResult*)(base + 0x54fda000);

	Engine::connections = (Connection*)(base + 0x6241e0);
	Engine::accounts = (Account*)(base + 0x353a3d0);
	Engine::voices = (Voice*)(base + 0x9cad280);
	Engine::players = (Player*)(base + 0x149238e0);
	Engine::humans = (Human*)(base + 0xbcec6c8);
	Engine::itemTypes = (ItemType*)(base + 0x58a86840);
	Engine::items = (Item*)(base + 0x80d1c60);
	Engine::vehicleTypes = (VehicleType*)(base + 0x4cbdc20);
	Engine::vehicles = (Vehicle*)(base + 0x1bd30bc0);
	Engine::bullets = (Bullet*)(base + 0x42620ca0);
	Engine::bodies = (RigidBody*)(base + 0x4ac1a0);
	Engine::bonds = (Bond*)(base + 0x1f8c72c0);
	Engine::streets = (Street*)(base + 0x372a3268);
	Engine::streetIntersections = (StreetIntersection*)(base + 0x37281264);
	Engine::buildings = (Building*)(base + 0x37375438);

	Engine::numConnections = (unsigned int*)(base + 0x444c2688);
	Engine::numBullets = (unsigned int*)(base + 0x443f1c60);
	Engine::numStreets = (unsigned int*)(base + 0x372a3264);
	Engine::numStreetIntersections = (unsigned int*)(base + 0x3728125c);
	Engine::numBuildings = (unsigned int*)(base + 0x373753f4);

	Engine::subRosaPuts = (Engine::subRosaPutsFunc)(base + 0x1fa0);
	Engine::subRosa__printf_chk =
	    (Engine::subRosa__printf_chkFunc)(base + 0x22c0);

	Engine::resetGame = (Engine::voidFunc)(base + 0xbc9c0);

	Engine::areaCreateBlock = (Engine::areaCreateBlockFunc)(base + 0x173f0);
	Engine::areaDeleteBlock = (Engine::areaDeleteBlockFunc)(base + 0x112d0);

	Engine::logicSimulation = (Engine::voidFunc)(base + 0xc3570);
	Engine::logicSimulationRace = (Engine::voidFunc)(base + 0xbf010);
	Engine::logicSimulationRound = (Engine::voidFunc)(base + 0xbf790);
	Engine::logicSimulationWorld = (Engine::voidFunc)(base + 0xc2b20);
	Engine::logicSimulationTerminator = (Engine::voidFunc)(base + 0xc0710);
	Engine::logicSimulationCoop = (Engine::voidFunc)(base + 0xbedd0);
	Engine::logicSimulationVersus = (Engine::voidFunc)(base + 0xc1f70);
	Engine::logicPlayerActions = (Engine::voidIndexFunc)(base + 0xb7fa0);

	Engine::physicsSimulation = (Engine::voidFunc)(base + 0xa6c30);
	Engine::rigidBodySimulation = (Engine::voidFunc)(base + 0x7c8e0);
	Engine::serverReceive = (Engine::serverReceiveFunc)(base + 0xcd800);
	Engine::serverSend = (Engine::voidFunc)(base + 0xca800);
	Engine::calculatePlayerVoice =
	    (Engine::calculatePlayerVoiceFunc)(base + 0xb22d0);
	Engine::sendPacket = (Engine::sendPacketFunc)(base + 0xc5600);
	Engine::bulletSimulation = (Engine::voidFunc)(base + 0x88ac0);
	Engine::bulletTimeToLive = (Engine::voidFunc)(base + 0x1c820);

	Engine::economyCarMarket = (Engine::voidFunc)(base + 0x24950);
	Engine::saveAccountsServer = (Engine::voidFunc)(base + 0xbd50);

	Engine::createAccountByJoinTicket =
	    (Engine::createAccountByJoinTicketFunc)(base + 0xb660);
	Engine::serverSendConnectResponse =
	    (Engine::serverSendConnectResponseFunc)(base + 0xc5c20);

	Engine::scenarioArmHuman = (Engine::scenarioArmHumanFunc)(base + 0x783f0);
	Engine::linkItem = (Engine::linkItemFunc)(base + 0x43bc0);
	Engine::itemSetMemo = (Engine::itemSetMemoFunc)(base + 0x3b6d0);
	Engine::itemComputerTransmitLine =
	    (Engine::itemComputerTransmitLineFunc)(base + 0x3b850);
	Engine::itemComputerIncrementLine = (Engine::voidIndexFunc)(base + 0x3bbc0);
	Engine::itemComputerInput = (Engine::itemComputerInputFunc)(base + 0x755d0);

	Engine::humanApplyDamage = (Engine::humanApplyDamageFunc)(base + 0x28c00);
	Engine::humanCollisionVehicle =
	    (Engine::humanCollisionVehicleFunc)(base + 0x65b40);
	Engine::humanLimbInverseKinematics =
	    (Engine::humanLimbInverseKinematicsFunc)(base + 0x6eea0);
	Engine::grenadeExplosion = (Engine::voidIndexFunc)(base + 0x40ef0);
	Engine::serverPlayerMessage =
	    (Engine::serverPlayerMessageFunc)(base + 0xb66a0);
	Engine::playerAI = (Engine::voidIndexFunc)(base + 0x87030);
	Engine::playerDeathTax = (Engine::voidIndexFunc)(base + 0x5960);
	Engine::accountDeathTax = (Engine::voidIndexFunc)(base + 0x5340);
	Engine::playerGiveWantedLevel =
	    (Engine::playerGiveWantedLevelFunc)(base + 0x67f0);
	Engine::createBondRigidBodyToRigidBody =
	    (Engine::createBondRigidBodyToRigidBodyFunc)(base + 0x18930);
	Engine::createBondRigidBodyRotRigidBody =
	    (Engine::createBondRigidBodyRotRigidBodyFunc)(base + 0x18bb0);
	Engine::createBondRigidBodyToLevel =
	    (Engine::createBondRigidBodyToLevelFunc)(base + 0x18810);
	Engine::addCollisionRigidBodyOnRigidBody =
	    (Engine::addCollisionRigidBodyOnRigidBodyFunc)(base + 0x18c90);
	Engine::addCollisionRigidBodyOnLevel =
	    (Engine::addCollisionRigidBodyOnLevelFunc)(base + 0x18e20);

	Engine::createBullet = (Engine::createBulletFunc)(base + 0x1c350);
	Engine::createPlayer = (Engine::createPlayerFunc)(base + 0x6a730);
	Engine::deletePlayer = (Engine::voidIndexFunc)(base + 0x6aa20);
	Engine::createHuman = (Engine::createHumanFunc)(base + 0x74150);
	Engine::deleteHuman = (Engine::voidIndexFunc)(base + 0x6b20);
	Engine::createItem = (Engine::createItemFunc)(base + 0x74d70);
	Engine::deleteItem = (Engine::voidIndexFunc)(base + 0x467c0);
	Engine::createRope = (Engine::createRopeFunc)(base + 0x76140);
	Engine::createVehicle = (Engine::createVehicleFunc)(base + 0x7a4e0);
	Engine::deleteVehicle = (Engine::voidIndexFunc)(base + 0x6d80);
	Engine::createRigidBody = (Engine::createRigidBodyFunc)(base + 0x73f10);

	Engine::createEventMessage = (Engine::createEventMessageFunc)(base + 0x54d0);
	Engine::createEventUpdatePlayer = (Engine::voidIndexFunc)(base + 0x57d0);
	Engine::createEventUpdatePlayerFinance =
	    (Engine::voidIndexFunc)(base + 0x58f0);
	Engine::createEventCreateVehicle = (Engine::voidIndexFunc)(base + 0x55f0);
	Engine::createEventUpdateVehicle =
	    (Engine::createEventUpdateVehicleFunc)(base + 0x5650);
	Engine::createEventSound = (Engine::createEventSoundFunc)(base + 0x5a30);
	Engine::createEventExplosion =
	    (Engine::createEventExplosionFunc)(base + 0x6100);
	Engine::createEventBullet = (Engine::createEventBulletFunc)(base + 0x5390);
	Engine::createEventBulletHit =
	    (Engine::createEventBulletHitFunc)(base + 0x5420);

	Engine::lineIntersectHuman = (Engine::lineIntersectHumanFunc)(base + 0x37ce0);
	Engine::lineIntersectLevel = (Engine::lineIntersectLevelFunc)(base + 0x862e0);
	Engine::lineIntersectVehicle =
	    (Engine::lineIntersectVehicleFunc)(base + 0x66090);
	Engine::lineIntersectTriangle =
	    (Engine::lineIntersectTriangleFunc)(base + 0x8630);
}

static inline void installHook(
    const char* name, subhook::Hook& hook, void* source, void* destination,
    subhook::HookFlags flags = subhook::HookFlags::HookFlag64BitOffset) {
	if (!hook.Install(source, destination, flags)) {
		std::ostringstream stream;
		stream << RS_PREFIX "Hook " << name << " failed to install";

		throw std::runtime_error(stream.str());
	}
}

#define INSTALL(name)                                               \
	installHook(#name "Hook", Hooks::name##Hook, (void*)Engine::name, \
	            (void*)Hooks::name);

static inline void installHooks() {
	INSTALL(subRosaPuts);
	INSTALL(subRosa__printf_chk);
	INSTALL(resetGame);
	INSTALL(areaCreateBlock);
	INSTALL(areaDeleteBlock);
	INSTALL(logicSimulation);
	INSTALL(logicSimulationRace);
	INSTALL(logicSimulationRound);
	INSTALL(logicSimulationWorld);
	INSTALL(logicSimulationTerminator);
	INSTALL(logicSimulationCoop);
	INSTALL(logicSimulationVersus);
	INSTALL(logicPlayerActions);
	INSTALL(physicsSimulation);
	INSTALL(rigidBodySimulation);
	INSTALL(serverReceive);
	INSTALL(serverSend);
	INSTALL(calculatePlayerVoice);
	INSTALL(sendPacket);
	INSTALL(bulletSimulation);
	INSTALL(economyCarMarket);
	INSTALL(saveAccountsServer);
	INSTALL(createAccountByJoinTicket);
	INSTALL(serverSendConnectResponse);
	INSTALL(linkItem);
	INSTALL(itemComputerInput);
	INSTALL(humanApplyDamage);
	INSTALL(humanCollisionVehicle);
	INSTALL(humanLimbInverseKinematics);
	INSTALL(grenadeExplosion);
	INSTALL(serverPlayerMessage);
	INSTALL(playerAI);
	INSTALL(playerDeathTax);
	INSTALL(accountDeathTax);
	INSTALL(playerGiveWantedLevel);
	INSTALL(addCollisionRigidBodyOnRigidBody);
	INSTALL(createBullet);
	INSTALL(createPlayer);
	INSTALL(deletePlayer);
	INSTALL(createHuman);
	INSTALL(deleteHuman);
	INSTALL(createItem);
	INSTALL(deleteItem);
	INSTALL(createVehicle);
	INSTALL(deleteVehicle);
	INSTALL(createRigidBody);
	INSTALL(createEventMessage);
	INSTALL(createEventUpdatePlayer);
	INSTALL(createEventUpdateVehicle);
	INSTALL(createEventBullet);
	INSTALL(createEventBulletHit);
	INSTALL(lineIntersectHuman);
}

static inline void attachSignalHandler() {
	struct sigaction action;
	action.sa_handler = Console::handleInterruptSignal;

	if (sigaction(SIGINT, &action, nullptr) == -1) {
		throw std::runtime_error(strerror(errno));
	}
}

static subhook::Hook getPathsHook;
typedef void (*getPathsFunc)();
static getPathsFunc getPaths;

// 'getpaths' is a tiny function that's called inside of main. It has to be
// recreated since installing a hook before main can't be reversed for some
// reason.
static inline void getPathsNormally() {
	char* pathA = (char*)(Lua::memory::baseAddress + 0x587d5c40);
	char* pathB = (char*)(Lua::memory::baseAddress + 0x587d5e40);

	getcwd(pathA, 0x200);
	getcwd(pathB, 0x200);
}

static void hookedGetPaths() {
	getPathsNormally();

	signal(SIGPIPE, SIG_IGN);

	Console::log(RS_PREFIX "Assuming 38d\n");

	Console::log(RS_PREFIX "Locating memory...\n");
	printBaseAddress(Lua::memory::baseAddress);
	locateMemory(Lua::memory::baseAddress);

	Console::log(RS_PREFIX "Installing hooks...\n");
	installHooks();

	Console::log(RS_PREFIX "Waiting for engine init...\n");

	// Don't load self into future child processes
	unsetenv("LD_PRELOAD");

	attachSignalHandler();
}

void __attribute__((constructor)) entry() {
	Lua::memory::baseAddress = getBaseAddress();
	getPaths = (getPathsFunc)(Lua::memory::baseAddress + 0xd27b0);

	installHook("getPathsHook", getPathsHook, (void*)getPaths,
	            (void*)hookedGetPaths);
}
