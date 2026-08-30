#include "practice_fsm.hpp"

#include "../../Projeto IOCP/UTIL/message_pool.h"

#include <cstring>

namespace stdA {

PracticeFsm::PracticeFsm(uint8_t holes, uint8_t course)
	: m_state(State::Idle), m_holes(holes == 0 ? 3 : holes), m_course(course),
	  m_holes_done(0), m_current_hole(0), m_oid(1), m_user_info_size(256),
	  m_error(), m_shot_sent(false) {
	std::memset(m_room_key, 0x11, sizeof(m_room_key));
}

void PracticeFsm::setSend(PracticeSendFn send) {
	m_send = std::move(send);
}

void PracticeFsm::setRoomKey(const unsigned char key[16]) {
	if (key != nullptr)
		std::memcpy(m_room_key, key, 16);
}

void PracticeFsm::setOid(uint32_t oid) {
	m_oid = oid;
}

void PracticeFsm::setUserInfoSize(size_t size) {
	if (size > 0)
		m_user_info_size = size;
}

std::string PracticeFsm::stateName() const {
	switch (m_state) {
	case State::Idle: return "Idle";
	case State::CreatingRoom: return "CreatingRoom";
	case State::Starting: return "Starting";
	case State::WaitingCourse: return "WaitingCourse";
	case State::PlayingHole: return "PlayingHole";
	case State::WaitingHoleResult: return "WaitingHoleResult";
	case State::Finishing: return "Finishing";
	case State::Succeeded: return "Succeeded";
	case State::Failed: return "Failed";
	}
	return "Unknown";
}

void PracticeFsm::fail(const std::string& why) {
	m_error = why;
	m_state = State::Failed;
	_smp::message_pool::getInstance().push(new message(
		"[PracticeFsm][FAIL] " + why, CL_FILE_LOG_AND_CONSOLE));
}

void PracticeFsm::onLobbyEntered() {
	if (!m_send) {
		fail("send callback is not set");
		return;
	}

	m_state = State::CreatingRoom;
	_smp::message_pool::getInstance().push(new message(
		"[PracticeFsm] Creating Practice room holes=" + std::to_string(m_holes)
			+ " course=" + std::to_string(m_course),
		CL_FILE_LOG_AND_CONSOLE));
	sendCreatePracticeRoom(m_send, m_holes, m_course);
}

void PracticeFsm::beginHole(uint8_t hole_number) {
	m_current_hole = hole_number;
	m_shot_sent = false;
	m_state = State::PlayingHole;
	_smp::message_pool::getInstance().push(new message(
		"[PracticeFsm] Init hole " + std::to_string(hole_number)
			+ " (" + std::to_string(m_holes_done) + "/" + std::to_string(m_holes) + " done)",
		CL_FILE_LOG_AND_CONSOLE));
	sendLoadPercent(m_send, 100);
	sendInitHole(m_send, hole_number, 4);
}

void PracticeFsm::playCurrentHole() {
	if (m_shot_sent)
		return;
	m_shot_sent = true;
	m_state = State::WaitingHoleResult;
	sendFinishLoadHole(m_send);
	sendFinishCharIntro(m_send);
	sendHoleOutShot(m_send, m_oid, m_room_key);
	_smp::message_pool::getInstance().push(new message(
		"[PracticeFsm] Hole-out shot sent for hole " + std::to_string(m_current_hole),
		CL_FILE_LOG_AND_CONSOLE));
}

void PracticeFsm::onServerPacket(unsigned short tipo, packet& p) {
	if (finished())
		return;

	try {
		switch (tipo) {
		case 0x49: {	// room created / room info
			if (m_state != State::CreatingRoom && m_state != State::Idle)
				break;
			m_state = State::Starting;
			_smp::message_pool::getInstance().push(new message(
				"[PracticeFsm] Room created, starting Practice", CL_FILE_LOG_AND_CONSOLE));
			sendStartGame(m_send);
			sendChangeItemRoom(m_send);
			break;
		}
		case 0x230:
		case 0x231:
		case 0x77:
			if (m_state == State::Starting)
				m_state = State::WaitingCourse;
			break;
		case 0x76:
			m_state = State::WaitingCourse;
			break;
		case 0x52: {	// course + hole list
			if (p.getSize() >= 8) {
				// skip course/tipo/modo/qntd + trofel/times already consumed by caller if any
			}
			m_hole_numbers.clear();
			for (uint8_t i = 1; i <= m_holes; ++i)
				m_hole_numbers.push_back(i);

			_smp::message_pool::getInstance().push(new message(
				"[PracticeFsm] Course packet 0x52, starting first hole", CL_FILE_LOG_AND_CONSOLE));
			beginHole(m_hole_numbers.front());
			break;
		}
		case 0x5B:	// wind — hole is live
		case 0x9E:	// weather
		case 0x53:	// player to start hole
			if (m_state == State::PlayingHole)
				playCurrentHole();
			break;
		case 0x6D: {	// hole finished
			sendFinishHoleData(m_send, m_user_info_size);
			m_holes_done++;
			_smp::message_pool::getInstance().push(new message(
				"[PracticeFsm] Hole finished, completed="
					+ std::to_string(m_holes_done) + "/" + std::to_string(m_holes),
				CL_FILE_LOG_AND_CONSOLE));

			if (m_holes_done >= m_holes) {
				m_state = State::Finishing;
			} else {
				beginHole(static_cast<uint8_t>(m_holes_done + 1));
			}
			break;
		}
		case 0x199:	// last hole of the match
			m_state = State::Finishing;
			_smp::message_pool::getInstance().push(new message(
				"[PracticeFsm] Last hole (0x199)", CL_FILE_LOG_AND_CONSOLE));
			break;
		case 0x79:	// placar
		case 0xCE:	// drop
			if (m_state == State::Finishing || m_holes_done >= m_holes) {
				m_state = State::Finishing;
				sendFinishGame(m_send, m_user_info_size);
				_smp::message_pool::getInstance().push(new message(
					"[PracticeFsm] Finish game (0x06) sent", CL_FILE_LOG_AND_CONSOLE));
			}
			break;
		case 0xC8:	// pang update after finish_game
			if (m_state == State::Finishing || m_holes_done >= m_holes) {
				m_state = State::Succeeded;
				_smp::message_pool::getInstance().push(new message(
					"[PracticeFsm] SUCCESS — Practice " + std::to_string(m_holes)
						+ " holes completed",
					CL_FILE_LOG_AND_CONSOLE));
			}
			break;
		case 0x253:	// start-game error
			fail("server rejected start game (0x253)");
			break;
		default:
			break;
		}
	} catch (const std::exception& e) {
		fail(e.what());
	}
}

}
