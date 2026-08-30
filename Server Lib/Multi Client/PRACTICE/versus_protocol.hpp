// Official VS (Stroke) room create / join / ready helpers.
#pragma once
#ifndef _STDA_VERSUS_PROTOCOL_HPP
#define _STDA_VERSUS_PROTOCOL_HPP

#include "practice_protocol.hpp"

#include <string>

namespace stdA {

	constexpr unsigned char ROOM_TIPO_STROKE = 0;	// RoomInfo::TIPO::STROKE

	inline void sendCreateVsRoom(const PracticeSendFn& send, uint8_t holes, uint8_t course,
			const std::string& name, const std::string& password) {
		packet p;
		p.init_plain((unsigned short)0x08);
		p.addUint8(0);				// option
		p.addUint32(0);				// time_vs
		p.addUint32(40 * 60000);	// time_30s
		p.addUint8(2);				// max_player
		p.addUint8(ROOM_TIPO_STROKE);
		p.addUint8(holes);
		p.addUint8(course);
		p.addUint8(0);				// FRONT
		p.addUint32(0);				// natural / short game
		p.addString(name);
		p.addString(password);
		p.addUint32(0);				// artefact
		send(p);
	}

	inline void sendJoinRoom(const PracticeSendFn& send, int16_t numero, const std::string& password) {
		packet p;
		p.init_plain((unsigned short)0x09);
		p.addInt16(numero);
		p.addString(password);
		send(p);
	}

	// 0x0D: server stores ready = !value, so 0 means become ready.
	inline void sendReady(const PracticeSendFn& send) {
		packet p;
		p.init_plain((unsigned short)0x0D);
		p.addUint8(0);
		send(p);
	}

	inline void sendStartTurnTime(const PracticeSendFn& send) {
		packet p;
		p.init_plain((unsigned short)0x22);
		send(p);
	}

	inline bool parseRoomInfo(packet& p, int16_t& numero, unsigned char room_key[16]) {
		if (p.getSize() < 4)
			return false;
		const int16_t option = p.readInt16();
		if (option != 0)
			return false;
		for (int i = 0; i < 64; ++i)
			(void)p.readUint8();	// nome
		(void)p.readUint8();		// senha_flag
		(void)p.readUint8();		// state
		(void)p.readUint8();		// flag
		(void)p.readUint8();		// max_player
		(void)p.readUint8();		// num_player
		for (int i = 0; i < 16; ++i)
			room_key[i] = static_cast<unsigned char>(p.readUint8());
		(void)p.readUint8();		// key[16]
		(void)p.readUint8();		// _30s
		(void)p.readUint8();		// qntd_hole
		(void)p.readUint8();		// tipo_show
		numero = p.readInt16();
		return numero >= 0;
	}
}

#endif
