#include "practice_loopback.hpp"
#include "practice_protocol.hpp"
#include "practice_tcp.hpp"

#include "../../Projeto IOCP/TYPE/pangya_st.h"
#include "../../Projeto IOCP/UTIL/message_pool.h"
#include "../TYPE/pangya_client_st.h"

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cstring>
#include <vector>

namespace stdA {

namespace {

int listen_port(uint16_t port) {
#if defined(__linux__)
	int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0)
		return -1;
	int yes = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	sockaddr_in sa{};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sa.sin_port = htons(port);
	if (::bind(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) != 0) {
		::close(fd);
		return -1;
	}
	if (::listen(fd, 2) != 0) {
		::close(fd);
		return -1;
	}
	return fd;
#else
	(void)port;
	return -1;
#endif
}

int accept_one(int listen_fd) {
#if defined(__linux__)
	return ::accept(listen_fd, nullptr, nullptr);
#else
	(void)listen_fd;
	return -1;
#endif
}

void send_login_hello(PracticeTcp& tcp) {
	packet p;
	p.init_plain((unsigned short)0x00);
	// unMakeFull treats the first Login hello as raw only when the key
	// int32 is 0 (ph.size == 0x0B and the next 3 bytes are zero).
	p.addInt32(0);		// session key
	p.addInt32(100);	// login server uid
	tcp.send_raw(p);
	tcp.setKey(0);
}

void send_login_ok(PracticeTcp& tcp, uint16_t game_port) {
	packet ok;
	ok.init_plain((unsigned short)0x01);
	ok.addUint8(0);
	ok.addString("practice");
	ok.addInt32(10);	// uid
	ok.addInt32(0);		// cap
	ok.addInt8(1);		// level
	ok.addInt32(0);
	ok.addInt8(0);
	ok.addInt32(0);		// opt nickname
	ok.addString("practice-bot");
	ok.addInt16(0);
	tcp.send_server(ok);

	ServerInfo si{};
	si.clear();
	std::strncpy(si.nome, "PracticeGS", sizeof(si.nome) - 1);
	si.uid = 1;
	si.max_user = 10;
	si.curr_user = 0;
	std::strncpy(si.ip, "127.0.0.1", sizeof(si.ip) - 1);
	si.port = game_port;

	packet list;
	list.init_plain((unsigned short)0x02);
	list.addInt8(1);
	list.addBuffer(&si, sizeof(si));
	tcp.send_server(list);
}

void send_login_key(PracticeTcp& tcp) {
	packet p;
	p.init_plain((unsigned short)0x03);
	p.addInt32(0);
	p.addString("GSKEY01");
	tcp.send_server(p);
}

void send_game_hello(PracticeTcp& tcp) {
	packet p;
	p.init_plain((unsigned short)0x3F);
	p.addInt8(0);
	p.addInt8(0);
	p.addInt8(2);	// game session key
	tcp.send_raw(p);
	tcp.setKey(2);
}

void send_game_logged_in(PracticeTcp& tcp) {
	packet ok;
	ok.init_plain((unsigned short)0x44);
	ok.addUint8(0xD3);
	tcp.send_server(ok);

	ChannelInfo ch{};
	ch.clear();
	std::strncpy(ch.name, "Practice", sizeof(ch.name) - 1);
	ch.max_user = 20;
	ch.curr_user = 0;
	ch.id = 0;
	ch.min_level_allow = 0;
	ch.max_level_allow = 70;

	packet canals;
	canals.init_plain((unsigned short)0x4D);
	canals.addInt8(1);
	canals.addBuffer(&ch, sizeof(ch));
	tcp.send_server(canals);
}

void send_course(PracticeTcp& tcp, uint8_t holes) {
	packet start;
	start.init_plain((unsigned short)0x76);
	start.addUint8(ROOM_TIPO_PRACTICE);
	start.addUint32(1);
	start.addZeroByte(16);	// SYSTEMTIME
	tcp.send_server(start);

	packet course;
	course.init_plain((unsigned short)0x52);
	course.addUint8(0);					// Blue Lagoon
	course.addUint8(ROOM_TIPO_PRACTICE);
	course.addUint8(0);					// FRONT
	course.addUint8(holes);
	course.addUint32(0);				// trofel
	course.addUint32(0);				// time_vs
	course.addUint32(40 * 60000);

	for (uint8_t i = 1; i <= 18; ++i) {
		course.addUint32(1000 + i);	// hole id
		course.addUint8(0);			// pin
		course.addUint8(0);			// course
		course.addUint8(i);			// number
	}
	course.addUint32(12345);		// seed
	for (uint8_t i = 0; i < 18; ++i)
		course.addUint8(0);			// no cubes
	tcp.send_server(course);
}

void send_hole_live(PracticeTcp& tcp, uint32_t oid) {
	packet weather;
	weather.init_plain((unsigned short)0x9E);
	weather.addUint16(0);
	weather.addUint8(0);
	tcp.send_server(weather);

	packet wind;
	wind.init_plain((unsigned short)0x5B);
	wind.addUint8(2);
	wind.addUint8(0);
	wind.addUint16(90);
	wind.addUint8(1);
	tcp.send_server(wind);

	packet who;
	who.init_plain((unsigned short)0x53);
	who.addUint32(oid);
	tcp.send_server(who);
}

void send_hole_done(PracticeTcp& tcp, uint32_t oid, uint8_t hole, bool last) {
	if (last) {
		packet last_hole;
		last_hole.init_plain((unsigned short)0x199);
		tcp.send_server(last_hole);
	}

	packet done;
	done.init_plain((unsigned short)0x6D);
	done.addUint32(oid);
	done.addUint8(hole);
	done.addUint8(1);
	done.addInt32(-2);
	done.addUint64(0);
	done.addUint64(0);
	done.addUint8(1);
	tcp.send_server(done);

	if (last) {
		packet placar;
		placar.init_plain((unsigned short)0x79);
		placar.addUint8(1);
		tcp.send_server(placar);
	}
}

} // namespace

PracticeLoopback::PracticeLoopback(uint16_t login_port, uint16_t game_port)
	: m_login_port(login_port), m_game_port(game_port), m_run(false),
	  m_game_finished(false), m_holes_played(0), m_last_event("idle") {
}

PracticeLoopback::~PracticeLoopback() {
	stop();
}

void PracticeLoopback::start() {
	m_run = true;
	m_login_th = std::thread(&PracticeLoopback::loginThread, this);
	m_game_th = std::thread(&PracticeLoopback::gameThread, this);
}

void PracticeLoopback::stop() {
	m_run = false;
#if defined(__linux__)
	// Unblock accept by connecting to ourselves.
	int poke = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (poke >= 0) {
		sockaddr_in sa{};
		sa.sin_family = AF_INET;
		sa.sin_port = htons(m_login_port);
		inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
		::connect(poke, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
		::close(poke);
	}
	poke = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (poke >= 0) {
		sockaddr_in sa{};
		sa.sin_family = AF_INET;
		sa.sin_port = htons(m_game_port);
		inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
		::connect(poke, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
		::close(poke);
	}
#endif
	if (m_login_th.joinable())
		m_login_th.join();
	if (m_game_th.joinable())
		m_game_th.join();
}

void PracticeLoopback::loginThread() {
	try {
		loginThreadInner();
	} catch (exception& e) {
		m_last_event = "login-exception";
		_smp::message_pool::getInstance().push(new message(
			"[PracticeLoopback] login thread: " + e.getFullMessageError(), CL_FILE_LOG_AND_CONSOLE));
	} catch (const std::exception& e) {
		m_last_event = "login-exception";
		_smp::message_pool::getInstance().push(new message(
			std::string("[PracticeLoopback] login thread std: ") + e.what(), CL_FILE_LOG_AND_CONSOLE));
	}
}

void PracticeLoopback::loginThreadInner() {
	int ls = listen_port(m_login_port);
	if (ls < 0) {
		_smp::message_pool::getInstance().push(new message(
			"[PracticeLoopback] cannot listen login port", CL_FILE_LOG_AND_CONSOLE));
		return;
	}

	_smp::message_pool::getInstance().push(new message(
		"[PracticeLoopback] login listen 127.0.0.1:" + std::to_string(m_login_port),
		CL_FILE_LOG_AND_CONSOLE));

	int fd = accept_one(ls);
#if defined(__linux__)
	::close(ls);
#endif
	if (fd < 0 || !m_run)
		return;

	PracticeTcp tcp;
	tcp.attach(fd);
	send_login_hello(tcp);

	while (m_run) {
		packet p;
		if (!tcp.recv_client(p))
			break;
		switch (p.getTipo()) {
		case 0x01:
			send_login_ok(tcp, m_game_port);
			break;
		case 0x03:
			send_login_key(tcp);
			return;
		default:
			break;
		}
	}
}

void PracticeLoopback::gameThread() {
	try {
		gameThreadInner();
	} catch (exception& e) {
		m_last_event = "game-exception";
		_smp::message_pool::getInstance().push(new message(
			"[PracticeLoopback] game thread: " + e.getFullMessageError(), CL_FILE_LOG_AND_CONSOLE));
	} catch (const std::exception& e) {
		m_last_event = "game-exception";
		_smp::message_pool::getInstance().push(new message(
			std::string("[PracticeLoopback] game thread std: ") + e.what(), CL_FILE_LOG_AND_CONSOLE));
	}
}

void PracticeLoopback::gameThreadInner() {
	int ls = listen_port(m_game_port);
	if (ls < 0) {
		_smp::message_pool::getInstance().push(new message(
			"[PracticeLoopback] cannot listen game port", CL_FILE_LOG_AND_CONSOLE));
		return;
	}

	_smp::message_pool::getInstance().push(new message(
		"[PracticeLoopback] game listen 127.0.0.1:" + std::to_string(m_game_port),
		CL_FILE_LOG_AND_CONSOLE));

	int fd = accept_one(ls);
#if defined(__linux__)
	::close(ls);
#endif
	if (fd < 0 || !m_run)
		return;

	PracticeTcp tcp;
	tcp.attach(fd);
	send_game_hello(tcp);

	const uint32_t oid = 1;
	uint8_t holes = 3;
	uint8_t current_hole = 0;
	bool course_sent = false;

	while (m_run) {
		packet p;
		if (!tcp.recv_client(p))
			break;

		switch (p.getTipo()) {
		case 0x02:
			m_last_event = "game-enter";
			send_game_logged_in(tcp);
			break;
		case 0x04: {
			m_last_event = "enter-channel";
			packet enter;
			enter.init_plain((unsigned short)0x4E);
			enter.addInt8(0);
			tcp.send_server(enter);
			break;
		}
		case 0x81: {
			m_last_event = "enter-lobby";
			packet lobby;
			lobby.init_plain((unsigned short)0xF5);
			tcp.send_server(lobby);
			break;
		}
		case 0x08: {
			m_last_event = "create-room";
			packet room;
			room.init_plain((unsigned short)0x49);
			room.addInt16(0);
			room.addZeroByte(200);	// RoomInfo blob (loopback FSM only needs 0x49)
			tcp.send_server(room);
			break;
		}
		case 0x0E: {
			m_last_event = "start-game";
			packet a, b, rate;
			a.init_plain((unsigned short)0x230);
			b.init_plain((unsigned short)0x231);
			rate.init_plain((unsigned short)0x77);
			rate.addUint32(100);
			tcp.send_server(a);
			tcp.send_server(b);
			tcp.send_server(rate);
			break;
		}
		case 0x0C:
			if (!course_sent) {
				course_sent = true;
				m_last_event = "course";
				send_course(tcp, holes);
			}
			break;
		case 0x1A: {
			PracticeInitHole ctx{};
			if (p.getSize() >= sizeof(ctx) + 2)
				p.readBuffer(&ctx, sizeof(ctx));
			current_hole = ctx.numero ? ctx.numero : static_cast<uint8_t>(current_hole + 1);
			m_last_event = "init-hole-" + std::to_string(current_hole);
			send_hole_live(tcp, oid);
			break;
		}
		case 0x1C: {
			const bool last = (m_holes_played.load() + 1) >= holes;
			m_holes_played.fetch_add(1);
			m_last_event = "finish-shot-" + std::to_string(m_holes_played.load());
			send_hole_done(tcp, oid, current_hole, last);
			break;
		}
		case 0x06: {
			packet pang;
			pang.init_plain((unsigned short)0xC8);
			pang.addUint64(0);
			pang.addUint64(0);
			tcp.send_server(pang);
			m_game_finished = true;
			m_last_event = "finish-game";
			_smp::message_pool::getInstance().push(new message(
				"[PracticeLoopback] finish_game accepted, holes="
					+ std::to_string(m_holes_played.load()),
				CL_FILE_LOG_AND_CONSOLE));
			return;
		}
		default:
			break;
		}
	}
}

}
