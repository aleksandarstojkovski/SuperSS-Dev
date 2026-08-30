// Autonomous Practice 3-hole bot (loopback or real Login/Game Server).
#include "../Projeto IOCP/TYPE/pangya_st.h"
#include "../Projeto IOCP/UTIL/exception.h"
#include "../Projeto IOCP/UTIL/message_pool.h"
#include "../Projeto IOCP/UTIL/hex_util.h"
#include "../Multi Client/PRACTICE/practice_fsm.hpp"
#include "../Multi Client/PRACTICE/versus_fsm.hpp"
#include "../Multi Client/PRACTICE/bot_config.hpp"
#include "../Multi Client/PRACTICE/practice_loopback.hpp"
#include "../Multi Client/PRACTICE/practice_tcp.hpp"
#include "../Multi Client/PRACTICE/tourney_fsm.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

using namespace stdA;
using namespace std::chrono_literals;

namespace {

constexpr const char* kPacketVerKey = "{a65ec0d3-7bde-4ec1-8e73-4b3e0eac6abb}";

std::mutex g_log_mu;

void log_line(const std::string& text) {
	std::lock_guard<std::mutex> lock(g_log_mu);
	std::cout << text << std::endl;
	_smp::message_pool::getInstance().push(new message(text, CL_FILE_LOG_AND_CONSOLE));
}

std::string tipo_hex(unsigned short tipo) {
	std::ostringstream os;
	os << "0x" << std::hex << std::uppercase << tipo;
	return os.str();
}

int decrypt_packet_ver(int packet_ver) {
	unsigned char* p = reinterpret_cast<unsigned char*>(&packet_ver);
	size_t index = 0;
	for (size_t i = 0; i < std::strlen(kPacketVerKey); ++i) {
		p[index] ^= static_cast<unsigned char>(kPacketVerKey[i]);
		index = (index == 3) ? 0 : index + 1;
	}
	return packet_ver;
}

bool recv_until(PracticeTcp& tcp, unsigned short want, packet& out, int max_pkts = 32) {
	for (int i = 0; i < max_pkts; ++i) {
		if (!tcp.recv_server(out))
			return false;
		log_line("[practice_bot] recv " + tipo_hex(out.getTipo()));
		if (out.getTipo() == want)
			return true;
	}
	return false;
}

struct LoginCtx {
	std::string user;
	std::string pass;
	std::string nickname;
	int32_t uid = 0;
	int32_t game_uid = 20203;
	std::string game_ip = "127.0.0.1";
	uint16_t game_port = 20203;
	std::string key_login;
	std::string key_game;
};

bool do_login(PracticeTcp& login, LoginCtx& ctx) {
	packet first;
	if (!login.recv_server(first) || first.getTipo() != 0x00) {
		log_line("[practice_bot] login hello failed");
		return false;
	}
	login.setKey(static_cast<unsigned char>(first.readInt32() & 0xFF));
	first.readInt32();

	packet req;
	req.init_plain(0x01);
	req.addString(ctx.user);
	req.addString(ctx.pass);
	req.addInt8(2);
	req.addInt64(0);
	req.addInt64(0x7FFFFFFFFFFFFFFF);
	req.addString("00-00-00-00-00-00");
	if (!login.send_client(req))
		return false;

	// Official first-login nick cannot equal the account ID and must be >= 4 chars.
	std::string setup_nick = ctx.nickname;
	if (setup_nick.empty() || setup_nick == ctx.user)
		setup_nick = ctx.user + "n";
	if (setup_nick.size() < 4)
		setup_nick = ctx.user + "bot";

	bool have_list = false;
	bool nick_saved = false;
	bool char_sent = false;
	for (int i = 0; i < 32 && !have_list; ++i) {
		packet reply;
		if (!login.recv_server(reply))
			break;
		log_line("[practice_bot] login " + tipo_hex(reply.getTipo()));
		if (reply.getTipo() == 0x0F) {
			continue;
		} else if (reply.getTipo() == 0x0E) {
			const int nick_opt = reply.readInt32();
			if (nick_opt != 0) {
				log_line("[practice_bot] nick check failed opt=" + std::to_string(nick_opt));
				return false;
			}
			if (!nick_saved) {
				nick_saved = true;
				ctx.nickname = setup_nick;
				packet save;
				save.init_plain(0x06);
				save.addString(setup_nick);
				if (!login.send_client(save))
					return false;
				log_line("[practice_bot] first-login save nick=" + setup_nick);
			}
		} else if (reply.getTipo() == 0x11) {
			log_line("[practice_bot] first-set character ack");
		} else if (reply.getTipo() == 0x01) {
			const int err = static_cast<unsigned char>(reply.readInt8());
			if (err == 4) {
				log_line("[practice_bot] already logged in; continue for SAME_ID_LOGIN");
				continue;
			}
			if (err == 0xD8) {
				// FIRST_LOGIN: check nick (0x07) then save (0x06 after 0x0E).
				packet chk;
				chk.init_plain(0x07);
				chk.addString(setup_nick);
				if (!login.send_client(chk))
					return false;
				log_line("[practice_bot] first-login check nick=" + setup_nick);
				continue;
			}
			if (err == 0xD9) {
				// FIRST_SET: official character pick (Nuri 0x4000000B, hair 0).
				if (!char_sent) {
					char_sent = true;
					packet ch;
					ch.init_plain(0x08);
					ch.addInt32(67108875);
					ch.addUint8(0);
					ch.addUint8(0);
					if (!login.send_client(ch))
						return false;
					log_line("[practice_bot] first-set character typeid=67108875");
				}
				continue;
			}
			if (err != 0) {
				log_line("[practice_bot] login error=" + std::to_string(err));
				return false;
			}
			reply.readString();	// id
			ctx.uid = reply.readInt32();
			try {
				reply.readInt32();	// capability
				reply.readInt16();	// level
				reply.readInt32();
				reply.readInt32();
				(void)reply.readString();	// addFixedString build date (len-prefixed)
				(void)reply.readString();	// auth token
				reply.readUint32();
				reply.readUint32();
				ctx.nickname = reply.readString();
			} catch (...) {
			}
			if (ctx.nickname.empty()) {
				if (ctx.user == "test")
					ctx.nickname = "test123";
				else if (ctx.user == "ciao")
					ctx.nickname = "ciaoo";
				else
					ctx.nickname = setup_nick;
			}
			log_line("[practice_bot] logged uid=" + std::to_string(ctx.uid)
				+ " nick=" + ctx.nickname);
		} else if (reply.getTipo() == 0x02) {
			const int n = reply.readInt8();
			if (n > 0) {
				ServerInfo si{};
				reply.readBuffer(&si, sizeof(si));
				ctx.game_uid = si.uid;
				ctx.game_ip = si.ip;
				ctx.game_port = static_cast<uint16_t>(si.port);
				log_line("[practice_bot] game list " + ctx.game_ip + ":"
					+ std::to_string(ctx.game_port) + " uid=" + std::to_string(ctx.game_uid));
			}
			have_list = true;
		} else if (reply.getTipo() == 0x10) {
			ctx.key_login = reply.readString();
		}
	}
	if (!have_list)
		return false;

	packet ask;
	ask.init_plain(0x03);
	ask.addInt32(ctx.game_uid);
	if (!login.send_client(ask))
		return false;

	packet key;
	if (!recv_until(login, 0x03, key)) {
		log_line("[practice_bot] missing auth key 0x03");
		return false;
	}
	const int opt = key.readInt32();
	if (opt == 0)
		ctx.key_game = key.readString();
	log_line("[practice_bot] auth key opt=" + std::to_string(opt) + " k=" + ctx.key_game);
	login.close();
	return true;
}

void apply_game_from_config(LoginCtx& ctx, const BotConfig& cfg) {
	if (!cfg.loaded)
		return;
	if (cfg.game.override_list) {
		ctx.game_ip = cfg.game.host;
		ctx.game_port = cfg.game.port;
		log_line("[practice_bot] GAME override " + ctx.game_ip + ":"
			+ std::to_string(ctx.game_port));
	} else if (ctx.game_ip.empty() || ctx.game_ip[0] == 0) {
		ctx.game_ip = !cfg.game.host.empty() ? cfg.game.host : cfg.login.host;
		if (ctx.game_port == 0)
			ctx.game_port = cfg.game.port;
	}
}

void print_usage() {
	std::cout
		<< "practice_bot — SuperSS IOCP Practice / Versus client\n"
		<< "\n"
		<< "Without --real or --config the bot stays on localhost loopback\n"
		<< "(login 11030, game 12030) and does not touch the Docker stack.\n"
		<< "\n"
		<< "  --real                 connect to official Login/Game (127.0.0.1 by default)\n"
		<< "  --vs                   two-bot Versus (needs --real or --config)\n"
		<< "  --tourney              4-bot Tourney tipo 4 (test1..test4 / 123456)\n"
		<< "  --config, -c FILE      INI with LOGIN/GAME/AUTH/MESSAGE/RANK/ACCOUNT\n"
		<< "                         implies --real; omitted hosts use [DEFAULT] or 127.0.0.1\n"
		<< "  --write-template [FILE] write a ready-to-edit bot.ini and exit\n"
		<< "  --user NAME            override [ACCOUNT] user\n"
		<< "  --pass PASS            override [ACCOUNT] pass\n"
		<< "  --guest-user NAME      override [ACCOUNT] guest_user\n"
		<< "  --guest-pass PASS      override [ACCOUNT] guest_pass\n"
		<< "  --help, -h             this help\n"
		<< "\n"
		<< "Template: Multi Client/bot.ini.template\n";
}

bool enter_game(PracticeTcp& game, const LoginCtx& ctx, bool wait_login_done) {
	packet hello;
	if (!game.recv_server(hello) || hello.getTipo() != 0x3F) {
		log_line("[practice_bot] game hello failed");
		return false;
	}
	hello.readInt8();
	hello.readInt8();
	game.setKey(static_cast<unsigned char>(hello.readInt8()));

	int packet_ver = decrypt_packet_ver(2019110100);
	packet enter;
	enter.init_plain(0x02);
	enter.addString(ctx.user);
	enter.addInt32(ctx.uid);
	enter.addInt32(0);
	enter.addInt16(0x6696);
	enter.addString(ctx.key_login);
	enter.addString("SS.R7.995.00");
	enter.addInt32(packet_ver);
	enter.addString("00-00-00-00-00-00");
	enter.addString(ctx.key_game);
	if (!game.send_client(enter))
		return false;

	packet ch_list;
	if (!recv_until(game, 0x4D, ch_list, 256)) {
		log_line("[practice_bot] missing channel list 0x4D");
		return false;
	}

	// Official login_task keeps sending after 0x4D; wait for the last packet
	// (0x1B1) so enter-channel/lobby do not race the cache load.
	if (wait_login_done) {
		packet done;
		if (!recv_until(game, 0x1B1, done, 256))
			log_line("[practice_bot] warning: no 0x1B1, entering channel anyway");
	}

	packet ch;
	ch.init_plain(0x04);
	ch.addInt8(0);
	if (!game.send_client(ch))
		return false;

	packet ch_ok;
	if (!recv_until(game, 0x4E, ch_ok, 64)) {
		log_line("[practice_bot] missing channel enter 0x4E");
		return false;
	}
	const int ch_opt = static_cast<unsigned char>(ch_ok.readInt8());
	log_line("[practice_bot] channel enter opt=" + std::to_string(ch_opt));
	if (ch_opt != 0 && ch_opt != 1) {
		log_line("[practice_bot] channel enter failed");
		return false;
	}

	packet lobby;
	lobby.init_plain(0x81);
	if (!game.send_client(lobby))
		return false;

	packet lobby_ok;
	if (!recv_until(game, 0xF5, lobby_ok, 256)) {
		log_line("[practice_bot] missing lobby 0xF5");
		return false;
	}
	log_line("[practice_bot] lobby entered");
	return true;
}

bool play_practice(PracticeTcp& game, bool require_loopback, PracticeLoopback* loopback) {
	PracticeFsm fsm;
	fsm.setSend([&](packet& pkt) {
		if (!game.send_client(pkt))
			throw exception("[practice_bot] send failed", 1);
	});
	fsm.onLobbyEntered();

	const auto deadline = std::chrono::steady_clock::now() + 180s;
	while (!fsm.finished() && std::chrono::steady_clock::now() < deadline) {
		packet inbound;
		if (!game.recv_server(inbound)) {
			log_line("[practice_bot] recv failed while fsm=" + fsm.stateName());
			break;
		}
		log_line("[practice_bot] sv " + tipo_hex(inbound.getTipo()) + " fsm=" + fsm.stateName());
		fsm.onServerPacket(inbound.getTipo(), inbound);
	}

	const bool ok = fsm.success() && fsm.holesCompleted() == 3
		&& (!require_loopback || (loopback && loopback->gameFinished() && loopback->holesPlayed() == 3));

	log_line(std::string("[practice_bot] result=") + (ok ? "PASS" : "FAIL")
		+ " fsm=" + fsm.stateName()
		+ " holes_fsm=" + std::to_string(fsm.holesCompleted())
		+ (fsm.lastError().empty() ? "" : " err=" + fsm.lastError()));
	return ok;
}

bool play_versus_one(PracticeTcp& game, VersusFsm::Role role, VersusShared& shared,
		const std::string& tag, const std::string& user, uint32_t uid) {
	VersusFsm fsm(role, shared);
	fsm.setIdentity(user, uid);
	fsm.setSend([&](packet& pkt) {
		if (!game.send_client(pkt))
			throw exception("[practice_bot] send failed", 1);
	});
	game.setRecvTimeout(2);
	fsm.onLobbyEntered();

	const auto deadline = std::chrono::steady_clock::now() + 180s;
	while (!fsm.finished() && std::chrono::steady_clock::now() < deadline) {
		fsm.tick();
		packet inbound;
		if (!game.recv_server(inbound)) {
			if (fsm.finished())
				break;
			fsm.tick();
			continue;
		}
		if (inbound.getTipo() == 0x48) {
			const size_t n = inbound.getSize();
			log_line(std::string("[practice_bot][") + tag + "] 0x48 bytes="
				+ std::to_string(n) + " nick=" + user
				+ " " + hex_util::BufferToHexString(inbound.getBuffer(),
					n > 96 ? 96 : n));
		}
		if (inbound.getTipo() == 0x53 || inbound.getTipo() == 0x63) {
			uint32_t turn_oid = 0;
			if (inbound.getSize() >= 6)
				std::memcpy(&turn_oid, inbound.getBuffer() + 2, 4);
			log_line(std::string("[practice_bot][") + tag + "] turn "
				+ tipo_hex(inbound.getTipo()) + " turn_oid=" + std::to_string(turn_oid)
				+ " my_oid=" + std::to_string(fsm.oid())
				+ " resolved=" + (fsm.oidResolved() ? "1" : "0"));
		}
		log_line(std::string("[practice_bot][") + tag + "] sv " + tipo_hex(inbound.getTipo())
			+ " fsm=" + fsm.stateName()
			+ " oid=" + std::to_string(fsm.oid())
			+ (fsm.oidResolved() ? "" : " (unresolved)"));
		fsm.onServerPacket(inbound.getTipo(), inbound);
		fsm.tick();
		if (inbound.getTipo() == 0x48)
			log_line(std::string("[practice_bot][") + tag + "] after 0x48 oid="
				+ std::to_string(fsm.oid()) + " resolved="
				+ (fsm.oidResolved() ? "1" : "0") + " nick=" + user
				+ " uid=" + std::to_string(uid));
	}

	const bool ok = fsm.success() && fsm.holesCompleted() >= 3;
	log_line(std::string("[practice_bot][") + tag + "] result=" + (ok ? "PASS" : "FAIL")
		+ " fsm=" + fsm.stateName()
		+ " holes=" + std::to_string(fsm.holesCompleted())
		+ " oid=" + std::to_string(fsm.oid())
		+ (fsm.lastError().empty() ? "" : " err=" + fsm.lastError()));
	return ok;
}

int run_real_versus(const std::string& host, uint16_t login_port,
		const LoginCtx& host_in, const LoginCtx& guest_in, const BotConfig& cfg) {
	VersusShared shared;
	std::atomic<bool> host_ok{false};
	std::atomic<bool> guest_ok{false};
	std::atomic<int> host_rc{1};
	std::atomic<int> guest_rc{1};

	std::thread host_th([&]() {
		try {
			LoginCtx ctx = host_in;
			PracticeTcp login;
			if (!login.connect_to(host, login_port) || !do_login(login, ctx)) {
				log_line("[practice_bot][host] login failed");
				return;
			}
			apply_game_from_config(ctx, cfg);
			if (ctx.game_ip.empty() || ctx.game_ip[0] == 0)
				ctx.game_ip = host;
			PracticeTcp game;
			if (!game.connect_to(ctx.game_ip, ctx.game_port) || !enter_game(game, ctx, true)) {
				log_line("[practice_bot][host] game enter failed");
				return;
			}
			host_ok = play_versus_one(game, VersusFsm::Role::Host, shared, "host",
				ctx.nickname.empty() ? ctx.user : ctx.nickname,
				static_cast<uint32_t>(ctx.uid));
			host_rc = host_ok ? 0 : 1;
			game.close();
		} catch (exception& e) {
			log_line("[practice_bot][host] exception: " + e.getFullMessageError());
		}
	});

	std::thread guest_th([&]() {
		try {
			std::this_thread::sleep_for(400ms);
			LoginCtx ctx = guest_in;
			PracticeTcp login;
			if (!login.connect_to(host, login_port) || !do_login(login, ctx)) {
				log_line("[practice_bot][guest] login failed");
				return;
			}
			apply_game_from_config(ctx, cfg);
			if (ctx.game_ip.empty() || ctx.game_ip[0] == 0)
				ctx.game_ip = host;
			PracticeTcp game;
			if (!game.connect_to(ctx.game_ip, ctx.game_port) || !enter_game(game, ctx, true)) {
				log_line("[practice_bot][guest] game enter failed");
				return;
			}
			guest_ok = play_versus_one(game, VersusFsm::Role::Guest, shared, "guest",
				ctx.nickname.empty() ? ctx.user : ctx.nickname,
				static_cast<uint32_t>(ctx.uid));
			guest_rc = guest_ok ? 0 : 1;
			game.close();
		} catch (exception& e) {
			log_line("[practice_bot][guest] exception: " + e.getFullMessageError());
		}
	});

	host_th.join();
	guest_th.join();

	const bool ok = host_ok && guest_ok;
	log_line(std::string("[practice_bot] VS two-bot result=") + (ok ? "PASS" : "FAIL")
		+ " host=" + (host_ok ? "PASS" : "FAIL")
		+ " guest=" + (guest_ok ? "PASS" : "FAIL"));
	return ok ? 0 : 1;
}

bool play_tourney_one(PracticeTcp& game, TourneyFsm::Role role, TourneyShared& shared,
		const std::string& tag, const std::string& user, uint32_t uid) {
	TourneyFsm fsm(role, shared);
	fsm.setIdentity(user, uid);
	fsm.setSend([&](packet& pkt) {
		if (!game.send_client(pkt))
			throw exception("[practice_bot] send failed", 1);
	});
	game.setRecvTimeout(2);
	fsm.onLobbyEntered();

	const auto deadline = std::chrono::steady_clock::now() + 180s;
	while (!fsm.finished() && std::chrono::steady_clock::now() < deadline) {
		fsm.tick();
		packet inbound;
		if (!game.recv_server(inbound)) {
			if (fsm.finished())
				break;
			fsm.tick();
			continue;
		}
		log_line(std::string("[practice_bot][") + tag + "] sv " + tipo_hex(inbound.getTipo())
			+ " fsm=" + fsm.stateName()
			+ " oid=" + std::to_string(fsm.oid())
			+ (fsm.oidResolved() ? "" : " (unresolved)"));
		fsm.onServerPacket(inbound.getTipo(), inbound);
		fsm.tick();
	}

	const bool ok = fsm.success() && fsm.holesCompleted() >= 3;
	log_line(std::string("[practice_bot][") + tag + "] result=" + (ok ? "PASS" : "FAIL")
		+ " fsm=" + fsm.stateName()
		+ " holes=" + std::to_string(fsm.holesCompleted())
		+ " oid=" + std::to_string(fsm.oid())
		+ (fsm.lastError().empty() ? "" : " err=" + fsm.lastError()));
	return ok;
}

constexpr const char* kTourneyPass = "123456";
constexpr const char* kTourneyUsers[4] = { "test1", "test2", "test3", "test4" };

int run_real_tourney(const std::string& host, uint16_t login_port, const BotConfig& cfg) {
	TourneyShared shared;
	std::atomic<bool> ok_flag[4]{};
	std::thread th[4];

	auto run_one = [&](int idx) {
		const char* tag = (idx == 0) ? "host" : (idx == 1 ? "g1" : (idx == 2 ? "g2" : "g3"));
		try {
			if (idx > 0)
				std::this_thread::sleep_for(std::chrono::milliseconds(400 * idx));
			LoginCtx ctx;
			ctx.user = kTourneyUsers[idx];
			ctx.pass = kTourneyPass;
			PracticeTcp login;
			if (!login.connect_to(host, login_port) || !do_login(login, ctx)) {
				log_line(std::string("[practice_bot][") + tag + "] login failed user="
					+ ctx.user);
				return;
			}
			apply_game_from_config(ctx, cfg);
			if (ctx.game_ip.empty() || ctx.game_ip[0] == 0)
				ctx.game_ip = host;
			PracticeTcp game;
			if (!game.connect_to(ctx.game_ip, ctx.game_port) || !enter_game(game, ctx, true)) {
				log_line(std::string("[practice_bot][") + tag + "] game enter failed");
				return;
			}
			const TourneyFsm::Role role = (idx == 0)
				? TourneyFsm::Role::Host : TourneyFsm::Role::Guest;
			ok_flag[idx] = play_tourney_one(game, role, shared, tag,
				ctx.nickname.empty() ? ctx.user : ctx.nickname,
				static_cast<uint32_t>(ctx.uid));
			game.close();
		} catch (exception& e) {
			log_line(std::string("[practice_bot][") + tag + "] exception: "
				+ e.getFullMessageError());
		}
	};

	for (int i = 0; i < 4; ++i)
		th[i] = std::thread(run_one, i);
	for (int i = 0; i < 4; ++i)
		th[i].join();

	bool ok = true;
	std::string summary;
	for (int i = 0; i < 4; ++i) {
		ok = ok && ok_flag[i];
		if (!summary.empty())
			summary += " ";
		summary += std::string(kTourneyUsers[i]) + "=" + (ok_flag[i] ? "PASS" : "FAIL");
	}
	log_line(std::string("[practice_bot] Tourney 4-bot result=") + (ok ? "PASS" : "FAIL")
		+ " " + summary);
	return ok ? 0 : 1;
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

	bool real = false;
	bool versus = false;
	bool tourney = false;
	bool show_help = false;
	std::string host = "127.0.0.1";
	uint16_t login_port = 11030;
	uint16_t game_port = 12030;
	std::string config_path;
	std::string write_template_path;
	std::string cli_user;
	std::string cli_pass;
	std::string cli_guest_user;
	std::string cli_guest_pass;
	LoginCtx ctx;
	ctx.user = "nat0";
	ctx.pass = "123456";
	LoginCtx guest_ctx;
	guest_ctx.user = "ciao";
	guest_ctx.pass = "123456";

	for (int i = 1; i < argc; ++i) {
		const std::string a = argv[i];
		if (a == "--help" || a == "-h") {
			show_help = true;
		} else if (a == "--real") {
			real = true;
			login_port = 10303;
			game_port = 20203;
			ctx.user = "test";
			ctx.pass = "123456";
		} else if (a == "--vs") {
			versus = true;
		} else if (a == "--tourney" || a == "--torneo") {
			tourney = true;
		} else if (a == "--config" || a == "-c") {
			if (i + 1 >= argc) {
				std::cerr << "[practice_bot] --config needs a file path" << std::endl;
				return 2;
			}
			config_path = argv[++i];
		} else if (a == "--write-template") {
			if (i + 1 < argc && argv[i + 1][0] != '-')
				write_template_path = argv[++i];
			else
				write_template_path = "bot.ini";
		} else if (a == "--user" && i + 1 < argc) {
			cli_user = argv[++i];
		} else if (a == "--pass" && i + 1 < argc) {
			cli_pass = argv[++i];
		} else if (a == "--guest-user" && i + 1 < argc) {
			cli_guest_user = argv[++i];
		} else if (a == "--guest-pass" && i + 1 < argc) {
			cli_guest_pass = argv[++i];
		} else if (a.rfind("--", 0) != 0 && config_path.empty()) {
			if (login_port == 11030 || (real && login_port == 10303 && i == 1))
				login_port = static_cast<uint16_t>(std::stoi(a));
			else
				game_port = static_cast<uint16_t>(std::stoi(a));
		}
	}

	if (show_help) {
		print_usage();
		return 0;
	}

	if (!write_template_path.empty()) {
		std::string err;
		if (!writeBotConfigTemplate(write_template_path, err)) {
			std::cerr << "[practice_bot] " << err << std::endl;
			return 2;
		}
		std::cout << "[practice_bot] wrote template " << write_template_path << std::endl;
		return 0;
	}

	BotConfig cfg;
	if (!config_path.empty()) {
		std::string err;
		if (!loadBotConfig(config_path, cfg, err)) {
			log_line("[practice_bot] config: " + err);
			return 2;
		}
		real = true;
		host = cfg.login.host;
		login_port = cfg.login.port;
		game_port = cfg.game.port;
		if (!cfg.user.empty())
			ctx.user = cfg.user;
		if (!cfg.pass.empty())
			ctx.pass = cfg.pass;
		if (!cfg.guest_user.empty())
			guest_ctx.user = cfg.guest_user;
		if (!cfg.guest_pass.empty())
			guest_ctx.pass = cfg.guest_pass;
		if (ctx.user == "nat0") {
			ctx.user = "test";
			if (cli_pass.empty() && cfg.pass.empty())
				ctx.pass = "123456";
		}
		log_line("[practice_bot] config " + botConfigSummary(cfg));
	}

	if (!cli_user.empty())
		ctx.user = cli_user;
	if (!cli_pass.empty())
		ctx.pass = cli_pass;
	if (!cli_guest_user.empty())
		guest_ctx.user = cli_guest_user;
	if (!cli_guest_pass.empty())
		guest_ctx.pass = cli_guest_pass;

	if ((versus || tourney) && !real) {
		log_line("[practice_bot] --vs / --tourney needs --real or --config");
		return 2;
	}
	if (versus && tourney) {
		log_line("[practice_bot] pick one of --vs or --tourney");
		return 2;
	}

	if (real && tourney) {
		log_line("[practice_bot] REAL Tourney 3-hole users=test1,test2,test3,test4"
			" login=" + host + ":" + std::to_string(login_port));
		return run_real_tourney(host, login_port, cfg);
	}

	if (real && versus) {
		log_line("[practice_bot] REAL VS 3-hole host=" + ctx.user
			+ " guest=" + guest_ctx.user
			+ " login=" + host + ":" + std::to_string(login_port));
		return run_real_versus(host, login_port, ctx, guest_ctx, cfg);
	}

	if (real) {
		log_line("[practice_bot] REAL Practice 3-hole user=" + ctx.user
			+ " login=" + host + ":" + std::to_string(login_port)
			+ " game=" + std::to_string(game_port));

		PracticeTcp login;
		if (!login.connect_to(host, login_port)) {
			log_line("[practice_bot] cannot connect login " + host + ":"
				+ std::to_string(login_port));
			return 2;
		}
		if (!do_login(login, ctx))
			return 1;

		apply_game_from_config(ctx, cfg);
		if (ctx.game_ip.empty() || ctx.game_ip[0] == 0)
			ctx.game_ip = host;
		if (ctx.game_port == 0)
			ctx.game_port = game_port;

		PracticeTcp game;
		if (!game.connect_to(ctx.game_ip, ctx.game_port)) {
			log_line("[practice_bot] cannot connect game " + ctx.game_ip + ":"
				+ std::to_string(ctx.game_port));
			return 2;
		}
		if (!enter_game(game, ctx, true))
			return 1;
		const bool ok = play_practice(game, false, nullptr);
		game.close();
		return ok ? 0 : 1;
	}

	log_line("[practice_bot] loopback Practice 3-hole login="
		+ std::to_string(login_port) + " game=" + std::to_string(game_port));

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

	ctx.user = "nat0";
	int rc = 1;
	try {
		if (!do_login(login, ctx)) {
			loopback.stop();
			return 1;
		}
		if (!enter_game(game, ctx, false)) {
			loopback.stop();
			return 1;
		}
		rc = play_practice(game, true, &loopback) ? 0 : 1;
		game.close();
		loopback.stop();
	} catch (exception& e) {
		log_line("[practice_bot] exception: " + e.getFullMessageError());
		game.close();
		loopback.stop();
		rc = 1;
	}
	return rc;
}
