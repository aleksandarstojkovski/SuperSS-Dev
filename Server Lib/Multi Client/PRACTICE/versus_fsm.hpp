// Two-player official VS (Stroke) state machine. Host creates the room;
// guest joins. Both play hole-out shots on their turn.

#pragma once
#ifndef _STDA_VERSUS_FSM_HPP
#define _STDA_VERSUS_FSM_HPP

#include "versus_protocol.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace stdA {

	struct VersusShared {
		std::atomic<int16_t> room_number{-1};
		std::atomic<bool> guest_ready{false};
		std::string password{"vsbot"};
		std::string name{"vs-bot"};
	};

	class VersusFsm {
		public:
			enum class Role : uint8_t { Host, Guest };
			enum class State : uint8_t {
				Idle,
				CreatingRoom,
				WaitingRoomNumber,
				Joining,
				WaitingGuest,
				WaitingStart,
				Starting,
				WaitingCourse,
				WaitingTurn,
				WaitingHoleResult,
				Finishing,
				Succeeded,
				Failed,
			};

			VersusFsm(Role role, VersusShared& shared, uint8_t holes = 3, uint8_t course = 0);

			void setSend(PracticeSendFn send);
			void setRoomKey(const unsigned char key[16]);
			void setOid(uint32_t oid);
			void setIdentity(const std::string& name, uint32_t uid);
			void setUserInfoSize(size_t size);
			uint32_t oid() const { return m_oid; }
			bool oidResolved() const { return m_oid_resolved; }

			void onLobbyEntered();
			void tick();
			void onServerPacket(unsigned short tipo, packet& p);

			State state() const { return m_state; }
			bool finished() const { return m_state == State::Succeeded || m_state == State::Failed; }
			bool success() const { return m_state == State::Succeeded; }
			uint8_t holesCompleted() const { return m_holes_done; }
			const std::string& lastError() const { return m_error; }
			std::string stateName() const;
			Role role() const { return m_role; }
			const char* roleName() const { return m_role == Role::Host ? "host" : "guest"; }

		private:
			void fail(const std::string& why);
			void beginHole(uint8_t hole_number);
			void shootIfMyTurn(uint32_t turn_oid);
			void maybeStartAsHost();
			void tryParseOid(packet& p);

			PracticeSendFn m_send;
			VersusShared& m_shared;
			Role m_role;
			State m_state;
			uint8_t m_holes;
			uint8_t m_course;
			uint8_t m_holes_done;
			uint8_t m_current_hole;
			uint32_t m_oid;
			uint32_t m_uid;
			size_t m_user_info_size;
			unsigned char m_room_key[16];
			std::string m_error;
			std::string m_self_name;
			bool m_item_sent;
			bool m_finish_sent;
			bool m_join_sent;
			bool m_ready_sent;
			bool m_shot_this_turn;
			bool m_saw_guest_ready_pkt;
			bool m_oid_resolved;
			bool m_hole_data_sent;
	};
}

#endif
