// Autonomous Practice 3-hole bot: talks to PracticeLoopback using the same
// IOCP packet framing as Multi Client (make / unMakeFull).
#include "../Projeto IOCP/UTIL/exception.h"
#include "../Projeto IOCP/UTIL/message_pool.h"
#include "../Multi Client/PRACTICE/practice_fsm.hpp"
#include "../Multi Client/PRACTICE/practice_loopback.hpp"
#include "../Multi Client/PRACTICE/practice_tcp.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

using namespace stdA;
using namespace std::chrono_literals;

namespace {

constexpr const char* kLoginUser = "nat0";
constexpr const char* kLoginPass = "123456";

void log_line(const std::string& text) {
	std::cout << text << std::endl;
	_smp::message_pool::getInstance().push(new message(text, CL_FILE_LOG_AND_CONSOLE));
}

std::string tipo_hex(unsigned short tipo) {
	std::ostringstream os;
	os << "0x" << std::hex << std::uppercase << tipo;
	return os.str();
}

bool loginAndEnterGame(PracticeTcp& login, PracticeTcp& game) {
	packet first;
	if (!login.recv_server(first)) {
		log_line("[practice_bot] login hello recv failed");
		return false;
	}
	if (first.getTipo() != 0x00) {
		log_line("[practice_bot] login hello tipo=" + std::to_string(first.getTipo()));
		return false;
	}

	const auto login_key = static_cast<unsigned char>(first.readInt32() & 0xFF);
	first.readInt32();	// login server uid
	login.setKey(login_key);

	packet login_req;
	login_req.init_plain(0x01);
	login_req.addString(kLoginUser);
	login_req.addString(kLoginPass);
	login_req.addInt8(2);
	login_req.addInt64(0);
	login_req.addInt64(0x7FFFFFFFFFFFFFFF);
	login_req.addString("00-00-00-00-00-00");
	if (!login.send_client(login_req)) {
		log_line("[practice_bot] login 0x01 send failed");
		return false;
	}

	bool have_server_list = false;
	for (int i = 0; i < 8 && !have_server_list; ++i) {
		packet reply;
		if (!login.recv_server(reply))
			break;
		log_line("[practice_bot] login reply " + tipo_hex(reply.getTipo()));
		if (reply.getTipo() == 0x02)
			have_server_list = true;
	}
	login.close();
	if (!have_server_list) {
		log_line("[practice_bot] missing login server list 0x02");
		return false;
	}

	packet gfirst;
	if (!game.recv_server(gfirst)) {
		log_line("[practice_bot] game hello recv failed");
		return false;
	}
	if (gfirst.getTipo() != 0x3F) {
		log_line("[practice_bot] game hello tipo=" + std::to_string(gfirst.getTipo()));
		return false;
	}
	gfirst.readInt8();
	gfirst.readInt8();
	game.setKey(static_cast<unsigned char>(gfirst.readInt8()));

	packet enter;
	enter.init_plain(0x02);
	enter.addString(kLoginUser);
	enter.addInt32(10);
	enter.addInt32(0);
	enter.addInt16(0x6696);
	enter.addString("");
	enter.addString("SS.R7.989.00");
	enter.addInt32(2019110100);
	enter.addString("00-00-00-00-00-00");
	enter.addString("");
	if (!game.send_client(enter)) {
		log_line("[practice_bot] game 0x02 send failed");
		return false;
	}

	bool have_channels = false;
	for (int i = 0; i < 16 && !have_channels; ++i) {
		packet reply;
		if (!game.recv_server(reply)) {
			log_line("[practice_bot] game lobby recv failed");
			return false;
		}
		log_line("[practice_bot] game reply " + tipo_hex(reply.getTipo()));
		if (reply.getTipo() == 0x4D)
			have_channels = true;
	}
	if (!have_channels) {
		log_line("[practice_bot] missing channel list 0x4D");
		return false;
	}

	packet ch;
	ch.init_plain(0x04);
	ch.addInt8(0);
	if (!game.send_client(ch))
		return false;

	packet ch_ok;
	if (!game.recv_server(ch_ok) || ch_ok.getTipo() != 0x4E) {
		log_line("[practice_bot] missing channel enter 0x4E");
		return false;
	}

	packet lobby;
	lobby.init_plain(0x81);
	if (!game.send_client(lobby))
		return false;

	packet lobby_ok;
	if (!game.recv_server(lobby_ok) || lobby_ok.getTipo() != 0xF5) {
		log_line("[practice_bot] missing lobby 0xF5");
		return false;
	}

	log_line("[practice_bot] lobby entered");
	return true;
}

}  // namespace

int main(int argc, char** argv) {
	std::set_terminate([]() {
		try {
			if (std::current_exception())
				std::rethrow_exception(std::current_exception());
		} catch (exception& e) {
			std::cerr << "[practice_bot] terminate: " << e.getFullMessageError() << std::endl;
		} catch (const std::exception& e) {
			std::cerr << "[practice_bot] terminate std: " << e.what() << std::endl;
		} catch (...) {
			std::cerr << "[practice_bot] terminate unknown" << std::endl;
		}
		std::_Exit(134);
	});

	uint16_t login_port = 11030;
	uint16_t game_port = 12030;
	if (argc >= 2)
		login_port = static_cast<uint16_t>(std::stoi(argv[1]));
	if (argc >= 3)
		game_port = static_cast<uint16_t>(std::stoi(argv[2]));

	log_line("[practice_bot] Practice 3-hole autonomous test (loopback login="
		+ std::to_string(login_port) + " game=" + std::to_string(game_port) + ")");

	PracticeLoopback loopback(login_port, game_port);
	loopback.start();
	std::this_thread::sleep_for(80ms);

	PracticeTcp login;
	PracticeTcp game;
	if (!login.connect_to("127.0.0.1", login_port) || !game.connect_to("127.0.0.1", game_port)) {
		log_line("[practice_bot] failed to connect loopback");
		loopback.stop();
		return 2;
	}

	int rc = 1;
	try {
		if (!loginAndEnterGame(login, game)) {
			loopback.stop();
			return 1;
		}

		PracticeFsm fsm;
		fsm.setSend([&](packet& pkt) {
			if (!game.send_client(pkt))
				throw exception("[practice_bot] send failed", 1);
		});
		fsm.onLobbyEntered();

		const auto deadline = std::chrono::steady_clock::now() + 20s;
		while (!fsm.finished() && std::chrono::steady_clock::now() < deadline) {
			packet inbound;
			if (!game.recv_server(inbound)) {
				log_line("[practice_bot] recv failed while fsm=" + fsm.stateName());
				break;
			}
			log_line("[practice_bot] sv " + tipo_hex(inbound.getTipo())
				+ " fsm=" + fsm.stateName());
			fsm.onServerPacket(inbound.getTipo(), inbound);
		}

		game.close();
		loopback.stop();

		const bool ok = fsm.success()
			&& loopback.gameFinished()
			&& loopback.holesPlayed() == 3;

		log_line(std::string("[practice_bot] result=") + (ok ? "PASS" : "FAIL")
			+ " fsm=" + fsm.stateName()
			+ " holes_fsm=" + std::to_string(fsm.holesCompleted())
			+ " holes_sv=" + std::to_string(loopback.holesPlayed())
			+ " finished=" + (loopback.gameFinished() ? "1" : "0")
			+ " last=" + loopback.lastEvent()
			+ (fsm.lastError().empty() ? "" : " err=" + fsm.lastError()));

		rc = ok ? 0 : 1;
	} catch (exception& e) {
		log_line("[practice_bot] exception: " + e.getFullMessageError());
		game.close();
		loopback.stop();
		rc = 1;
	}

	return rc;
}
