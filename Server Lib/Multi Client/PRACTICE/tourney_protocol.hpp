// Official Tourney (tipo 4) room create helper.
#pragma once
#ifndef _STDA_TOURNEY_PROTOCOL_HPP
#define _STDA_TOURNEY_PROTOCOL_HPP

#include "versus_protocol.hpp"

namespace stdA {

	constexpr unsigned char ROOM_TIPO_TOURNEY = 4;	// RoomInfo::TIPO::TOURNEY

	inline void sendCreateTourneyRoom(const PracticeSendFn& send, uint8_t holes, uint8_t course,
			const std::string& name, const std::string& password) {
		packet p;
		p.init_plain((unsigned short)0x08);
		p.addUint8(0);				// option
		p.addUint32(0);				// time_vs (unused in Tourney)
		p.addUint32(40 * 60000);	// time_30s
		p.addUint8(2);				// max_player (Tourney cannot start with 1 real player)
		p.addUint8(ROOM_TIPO_TOURNEY);
		p.addUint8(holes);
		p.addUint8(course);
		p.addUint8(0);				// FRONT
		p.addUint32(0);				// natural / short game
		p.addString(name);
		p.addString(password);		// not "bot" — that path consumes a visual-bot ticket
		p.addUint32(0);				// artefact
		send(p);
	}
}

#endif
