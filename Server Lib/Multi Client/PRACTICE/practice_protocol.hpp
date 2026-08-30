// Shared Practice protocol helpers for the IOCP Multi Client bot.
// Layouts match Game Server packed structs (ShotData / ShotSyncData / hole init).

#pragma once
#ifndef _STDA_PRACTICE_PROTOCOL_HPP
#define _STDA_PRACTICE_PROTOCOL_HPP

#include "../../Projeto IOCP/PACKET/packet.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

namespace stdA {

#if defined(__linux__)
#pragma pack(1)
#endif

	struct PracticeShotData {
		float bar_point[2];
		float ball_effect[2];
		unsigned char acerto_pangya_flag;
		uint32_t special_shot;
		uint32_t time_hole_sync;
		float mira;
		uint32_t time_shot;
		float bar_point1;
		unsigned char club;
		float fUnknown[2];
		float impact_zone_pixel;
		int32_t natural_wind[2];
		float spend_time_game;
	};

	struct PracticeShotSyncData {
		uint32_t oid;
		float x, y, z;
		unsigned char state;		// ShotSyncData::INTO_HOLE = 4
		unsigned char bunker_flag;
		unsigned char ucUnknown;
		uint32_t pang;
		uint32_t bonus_pang;
		uint32_t display_state;
		uint32_t shot_state;
		uint16_t tempo_shot;
		unsigned char grand_prix_penalidade;
	};

	struct PracticeInitHole {
		unsigned char numero;
		uint32_t option;
		uint32_t ulUnknown;
		unsigned char par;
		float tee_x, tee_z;
		float pin_x, pin_z;
	};

#if defined(__linux__)
#pragma pack()
#endif

	inline void encrypt16(unsigned char* buffer, size_t size, const unsigned char* key) {
		if (buffer == nullptr || key == nullptr || size == 0)
			return;
		for (size_t i = 0; i < size; ++i)
			buffer[i] ^= key[i % 16];
	}

	constexpr unsigned char ROOM_TIPO_PRACTICE = 19;
	constexpr unsigned char SHOT_STATE_INTO_HOLE = 4;
	constexpr uint32_t DISPLAY_ACERTO_HOLE = (1u << 8);
	// packed sizeof(UserInfo) on Linux (pangya_game_st.h #pragma pack(1))
	constexpr size_t USER_INFO_SIZE = 265;

	using PracticeSendFn = std::function<void(packet&)>;

	inline void sendCreatePracticeRoom(const PracticeSendFn& send, uint8_t holes, uint8_t course) {
		packet p;
		p.init_plain((unsigned short)0x08);
		p.addUint8(0);				// option
		p.addUint32(0);				// time_vs
		p.addUint32(40 * 60000);	// time_30s
		p.addUint8(1);				// max_player
		p.addUint8(ROOM_TIPO_PRACTICE);
		p.addUint8(holes);
		p.addUint8(course);
		p.addUint8(0);				// FRONT
		p.addUint32(0);				// natural / short game
		p.addString("practice-bot");
		p.addString("");			// no password
		p.addUint32(0);				// artefact
		send(p);
	}

	inline void sendStartGame(const PracticeSendFn& send) {
		packet p;
		p.init_plain((unsigned short)0x0E);
		send(p);
	}

	inline void sendChangeItemRoom(const PracticeSendFn& send) {
		// TC_ALL (7) is the official post-start 0x0C; room::startGame()
		// only runs from that branch and is what sends course 0x76/0x52.
		packet p;
		p.init_plain((unsigned short)0x0C);
		p.addUint8(7);
		p.addUint32(0);	// character id (0 = keep / first owned)
		p.addUint32(0);	// caddie
		p.addUint32(0);	// clubset
		p.addUint32(0);	// ball
		send(p);
	}

	inline void sendLoadPercent(const PracticeSendFn& send, uint8_t percent) {
		packet p;
		p.init_plain((unsigned short)0x48);
		p.addUint8(percent);
		send(p);
	}

	inline void sendInitHole(const PracticeSendFn& send, uint8_t hole_number, uint8_t par) {
		PracticeInitHole ctx{};
		ctx.numero = hole_number;
		ctx.par = par;
		ctx.tee_x = 0.f;
		ctx.tee_z = 0.f;
		ctx.pin_x = 10.f;
		ctx.pin_z = 10.f;

		packet p;
		p.init_plain((unsigned short)0x1A);
		p.addBuffer(&ctx, sizeof(ctx));
		send(p);
	}

	inline void sendFinishLoadHole(const PracticeSendFn& send) {
		packet p;
		p.init_plain((unsigned short)0x11);
		send(p);
	}

	inline void sendFinishCharIntro(const PracticeSendFn& send) {
		packet p;
		p.init_plain((unsigned short)0x34);
		send(p);
	}

	inline void sendShotSync(const PracticeSendFn& send, uint32_t oid, const unsigned char room_key[16]) {
		PracticeShotSyncData ssd{};
		ssd.oid = oid;
		ssd.x = 10.f;
		ssd.y = 0.f;
		ssd.z = 10.f;
		ssd.state = SHOT_STATE_INTO_HOLE;
		ssd.display_state = DISPLAY_ACERTO_HOLE;
		encrypt16(reinterpret_cast<unsigned char*>(&ssd), sizeof(ssd), room_key);

		packet sync;
		sync.init_plain((unsigned short)0x1B);
		sync.addBuffer(&ssd, sizeof(ssd));
		send(sync);

		packet finish;
		finish.init_plain((unsigned short)0x1C);
		finish.addUint8(0);	// no cube/coin
		finish.addUint8(0);
		send(finish);
	}

	inline void sendInitShot(const PracticeSendFn& send) {
		PracticeShotData sd{};
		sd.club = 13;	// putter-ish
		sd.bar_point[0] = 1.f;
		sd.bar_point[1] = 0.f;

		packet init_shot;
		init_shot.init_plain((unsigned short)0x12);
		init_shot.addUint16(0);	// no power shot
		init_shot.addBuffer(&sd, sizeof(sd));
		send(init_shot);

		packet arrows;
		arrows.init_plain((unsigned short)0x42);
		arrows.addUint8(1);
		arrows.addUint32(1);	// one arrow (up)
		send(arrows);
	}

	inline void sendHoleOutShot(const PracticeSendFn& send, uint32_t oid, const unsigned char room_key[16]) {
		sendInitShot(send);
		sendShotSync(send, oid, room_key);
	}

	inline void sendFinishHoleData(const PracticeSendFn& send, size_t user_info_size) {
		packet p;
		p.init_plain((unsigned short)0x31);
		p.addZeroByte(user_info_size);
		send(p);
	}

	inline void sendFinishGame(const PracticeSendFn& send, size_t user_info_size) {
		packet p;
		p.init_plain((unsigned short)0x06);
		p.addZeroByte(user_info_size);
		send(p);
	}
}

#endif
