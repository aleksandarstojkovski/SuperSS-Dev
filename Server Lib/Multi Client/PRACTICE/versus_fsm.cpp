#include "versus_fsm.hpp"

#include "../../Projeto IOCP/UTIL/message_pool.h"

#include <cstring>

namespace stdA {

VersusFsm::VersusFsm(Role role, VersusShared& shared, uint8_t holes, uint8_t course)
	: m_shared(shared), m_role(role), m_state(State::Idle),
	  m_holes(holes == 0 ? 3 : holes), m_course(course),
	  m_holes_done(0), m_current_hole(0), m_oid(0), m_uid(0),
	  m_user_info_size(USER_INFO_SIZE),
	  m_error(), m_item_sent(false), m_finish_sent(false),
	  m_join_sent(false), m_ready_sent(false), m_shot_this_turn(false),
	  m_saw_guest_ready_pkt(false), m_oid_resolved(false),
	  m_hole_data_sent(false) {
	std::memset(m_room_key, 0x11, sizeof(m_room_key));
}

void VersusFsm::setSend(PracticeSendFn send) {
	m_send = std::move(send);
}

void VersusFsm::setRoomKey(const unsigned char key[16]) {
	if (key != nullptr)
		std::memcpy(m_room_key, key, 16);
}

void VersusFsm::setOid(uint32_t oid) {
	m_oid = oid;
	m_oid_resolved = true;
}

void VersusFsm::setIdentity(const std::string& name, uint32_t uid) {
	m_self_name = name;
	m_uid = uid;
}

void VersusFsm::setUserInfoSize(size_t size) {
	if (size > 0)
		m_user_info_size = size;
}

std::string VersusFsm::stateName() const {
	switch (m_state) {
	case State::Idle: return "Idle";
	case State::CreatingRoom: return "CreatingRoom";
	case State::WaitingRoomNumber: return "WaitingRoomNumber";
	case State::Joining: return "Joining";
	case State::WaitingGuest: return "WaitingGuest";
	case State::WaitingStart: return "WaitingStart";
	case State::Starting: return "Starting";
	case State::WaitingCourse: return "WaitingCourse";
	case State::WaitingTurn: return "WaitingTurn";
	case State::WaitingHoleResult: return "WaitingHoleResult";
	case State::Finishing: return "Finishing";
	case State::Succeeded: return "Succeeded";
	case State::Failed: return "Failed";
	}
	return "Unknown";
}

void VersusFsm::fail(const std::string& why) {
	m_error = why;
	m_state = State::Failed;
	_smp::message_pool::getInstance().push(new message(
		std::string("[VersusFsm][") + roleName() + "][FAIL] " + why, CL_FILE_LOG_AND_CONSOLE));
}

void VersusFsm::onLobbyEntered() {
	if (!m_send) {
		fail("send callback is not set");
		return;
	}

	if (m_role == Role::Host) {
		m_state = State::CreatingRoom;
		_smp::message_pool::getInstance().push(new message(
			"[VersusFsm][host] Creating Stroke room holes=" + std::to_string(m_holes),
			CL_FILE_LOG_AND_CONSOLE));
		sendCreateVsRoom(m_send, m_holes, m_course, m_shared.name, m_shared.password);
	} else {
		m_state = State::WaitingRoomNumber;
		_smp::message_pool::getInstance().push(new message(
			"[VersusFsm][guest] Waiting for host room number", CL_FILE_LOG_AND_CONSOLE));
	}
}

void VersusFsm::maybeStartAsHost() {
	if (m_role != Role::Host || m_state != State::WaitingGuest)
		return;
	if (!m_shared.guest_ready.load() || !m_saw_guest_ready_pkt)
		return;
	m_state = State::Starting;
	_smp::message_pool::getInstance().push(new message(
		"[VersusFsm][host] Guest ready, starting VS", CL_FILE_LOG_AND_CONSOLE));
	sendStartGame(m_send);
}

void VersusFsm::tick() {
	if (finished() || !m_send)
		return;

	if (m_role == Role::Guest && m_state == State::WaitingRoomNumber && !m_join_sent) {
		const int16_t n = m_shared.room_number.load();
		if (n >= 0) {
			m_join_sent = true;
			m_state = State::Joining;
			_smp::message_pool::getInstance().push(new message(
				"[VersusFsm][guest] Joining room " + std::to_string(n),
				CL_FILE_LOG_AND_CONSOLE));
			sendJoinRoom(m_send, n, m_shared.password);
		}
	}

	maybeStartAsHost();
}

void VersusFsm::beginHole(uint8_t hole_number) {
	m_current_hole = hole_number;
	m_shot_this_turn = false;
	m_hole_data_sent = false;
	m_state = State::WaitingTurn;
	_smp::message_pool::getInstance().push(new message(
		std::string("[VersusFsm][") + roleName() + "] Load hole "
			+ std::to_string(hole_number),
		CL_FILE_LOG_AND_CONSOLE));
	sendLoadPercent(m_send, 100);
	sendInitHole(m_send, hole_number, 4);
	sendFinishLoadHole(m_send);
	sendFinishCharIntro(m_send);
}

void VersusFsm::shootIfMyTurn(uint32_t turn_oid) {
	if (!m_oid_resolved || turn_oid != m_oid || m_shot_this_turn)
		return;
	m_shot_this_turn = true;
	m_state = State::WaitingHoleResult;
	_smp::message_pool::getInstance().push(new message(
		std::string("[VersusFsm][") + roleName() + "] My turn oid="
			+ std::to_string(m_oid) + " hole=" + std::to_string(m_current_hole),
		CL_FILE_LOG_AND_CONSOLE));
	sendStartTurnTime(m_send);
	sendHoleOutShot(m_send, m_oid, m_room_key);
}

// PlayerRoomInfo (pack 1): oid@0, nickname@4[22], uid@108.
static constexpr size_t kPlayerRoomUidOff = 108;
static constexpr size_t kPlayerRoomNickOff = 4;
static constexpr size_t kPlayerRoomNickLen = 22;

static bool paddedNameAt(const unsigned char* buf, size_t n, size_t off,
		const std::string& name) {
	if (name.empty() || off + kPlayerRoomNickLen > n)
		return false;
	if (std::memcmp(buf + off, name.c_str(), name.size()) != 0)
		return false;
	for (size_t k = name.size(); k < kPlayerRoomNickLen; ++k) {
		if (buf[off + k] != 0)
			return false;
	}
	return true;
}

void VersusFsm::tryParseOid(packet& p) {
	const unsigned char* buf = p.getBuffer();
	const size_t n = p.getSize();
	if (buf == nullptr || n < 8)
		return;

	// Prefer the 22-byte nickname field: oid is the uint32 immediately before it.
	if (!m_self_name.empty()) {
		for (size_t i = 4; i + kPlayerRoomNickLen <= n; ++i) {
			if (!paddedNameAt(buf, n, i, m_self_name))
				continue;
			uint32_t oid = 0;
			std::memcpy(&oid, buf + i - 4, 4);
			if (oid >= 256)
				continue;
			if (!m_oid_resolved || m_oid != oid) {
				setOid(oid);
				_smp::message_pool::getInstance().push(new message(
					std::string("[VersusFsm][") + roleName() + "] oid="
						+ std::to_string(oid) + " via nick=" + m_self_name,
					CL_FILE_LOG_AND_CONSOLE));
			}
			return;
		}
	}

	// Fallback: uid at PlayerRoomInfo +108, only if nickname at +4 is printable.
	if (m_uid != 0) {
		for (size_t i = 0; i + kPlayerRoomUidOff + 4 <= n; ++i) {
			uint32_t uid = 0;
			std::memcpy(&uid, buf + i + kPlayerRoomUidOff, 4);
			if (uid != m_uid)
				continue;
			uint32_t oid = 0;
			std::memcpy(&oid, buf + i, 4);
			if (oid >= 256)
				continue;
			const unsigned char nick0 = buf[i + kPlayerRoomNickOff];
			if (nick0 < 32 || nick0 > 126)
				continue;
			if (!m_oid_resolved || m_oid != oid) {
				setOid(oid);
				_smp::message_pool::getInstance().push(new message(
					std::string("[VersusFsm][") + roleName() + "] oid="
						+ std::to_string(oid) + " via uid=" + std::to_string(m_uid),
					CL_FILE_LOG_AND_CONSOLE));
			}
			return;
		}
	}
}

void VersusFsm::onServerPacket(unsigned short tipo, packet& p) {
	if (finished())
		return;

	try {
		switch (tipo) {
		case 0x48:
			tryParseOid(p);
			break;
		case 0x49: {
			int16_t numero = -1;
			unsigned char key[16]{};
			if (!parseRoomInfo(p, numero, key)) {
				if (m_state == State::CreatingRoom || m_state == State::Joining)
					fail("0x49 room error");
				break;
			}
			setRoomKey(key);
			if (m_role == Role::Host && m_state == State::CreatingRoom) {
				m_shared.room_number.store(numero);
				m_state = State::WaitingGuest;
				_smp::message_pool::getInstance().push(new message(
					"[VersusFsm][host] Room " + std::to_string(numero) + " created",
					CL_FILE_LOG_AND_CONSOLE));
			} else if (m_role == Role::Guest && m_state == State::Joining) {
				if (!m_ready_sent) {
					m_ready_sent = true;
					sendReady(m_send);
					m_shared.guest_ready.store(true);
				}
				m_state = State::WaitingStart;
				_smp::message_pool::getInstance().push(new message(
					"[VersusFsm][guest] Joined room " + std::to_string(numero) + ", ready",
					CL_FILE_LOG_AND_CONSOLE));
			}
			break;
		}
		case 0x230:
		case 0x231:
		case 0x77:
			if (m_state == State::Starting || m_state == State::WaitingStart)
				m_state = State::WaitingCourse;
			if (!m_item_sent && m_send) {
				m_item_sent = true;
				sendChangeItemRoom(m_send);
			}
			break;
		case 0x76:
			m_state = State::WaitingCourse;
			break;
		case 0x52:
			beginHole(1);
			break;
		case 0x53:
		case 0x63: {
			if (p.getSize() >= 4) {
				const uint32_t turn_oid = static_cast<uint32_t>(p.readInt32());
				m_shot_this_turn = false;
				_smp::message_pool::getInstance().push(new message(
					std::string("[VersusFsm][") + roleName()
						+ "] turn pkt=0x" + (tipo == 0x53 ? "53" : "63")
						+ " turn_oid=" + std::to_string(turn_oid)
						+ " my_oid=" + std::to_string(m_oid),
					CL_FILE_LOG_AND_CONSOLE));
				if (m_oid_resolved)
					shootIfMyTurn(turn_oid);
			}
			break;
		}
		case 0x65:
		case 0x6D:
			if (!m_hole_data_sent) {
				m_hole_data_sent = true;
				sendFinishHoleData(m_send, m_user_info_size);
			}
			m_holes_done++;
			_smp::message_pool::getInstance().push(new message(
				std::string("[VersusFsm][") + roleName() + "] Hole done "
					+ std::to_string(m_holes_done) + "/" + std::to_string(m_holes)
					+ (tipo == 0x65 ? " (0x65 VS)" : " (0x6D)"),
				CL_FILE_LOG_AND_CONSOLE));
			if (m_holes_done >= m_holes)
				m_state = State::Finishing;
			else
				beginHole(static_cast<uint8_t>(m_holes_done + 1));
			break;
		case 0x199:
			// Broadcast when the first player holes-out the last hole.
			// The other player still has a 0x63 turn — do not leave WaitingTurn.
			if (m_shot_this_turn && m_current_hole >= m_holes)
				m_state = State::Finishing;
			break;
		case 0x66:
		case 0x79:
		case 0xCE:
			if (m_holes_done < m_holes)
				m_holes_done = m_holes;
			if (!m_finish_sent) {
				m_state = State::Finishing;
				m_finish_sent = true;
				if (!m_hole_data_sent) {
					m_hole_data_sent = true;
					sendFinishHoleData(m_send, m_user_info_size);
				}
				sendFinishGame(m_send, m_user_info_size);
			}
			break;
		case 0xC8:
			if (m_state == State::Finishing || m_holes_done >= m_holes) {
				m_state = State::Succeeded;
				_smp::message_pool::getInstance().push(new message(
					std::string("[VersusFsm][") + roleName() + "] SUCCESS holes="
						+ std::to_string(m_holes_done),
					CL_FILE_LOG_AND_CONSOLE));
			}
			break;
		case 0x78:
			if (m_role == Role::Host && m_state == State::WaitingGuest)
				m_saw_guest_ready_pkt = true;
			break;
		case 0x253: {
			uint32_t err = 0;
			if (p.getSize() >= 4)
				err = static_cast<uint32_t>(p.readInt32());
			fail("server rejected start (0x253) err=" + std::to_string(err));
			break;
		}
		default:
			break;
		}
	} catch (const std::exception& e) {
		fail(e.what());
	}

	maybeStartAsHost();
}

}
