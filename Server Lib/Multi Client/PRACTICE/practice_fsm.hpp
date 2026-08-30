// Autonomous Practice (3-hole) state machine for the IOCP Multi Client.

#pragma once
#ifndef _STDA_PRACTICE_FSM_HPP
#define _STDA_PRACTICE_FSM_HPP

#include "practice_protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace stdA {

	class PracticeFsm {
		public:
			enum class State : uint8_t {
				Idle,
				CreatingRoom,
				Starting,
				WaitingCourse,
				PlayingHole,
				WaitingHoleResult,
				Finishing,
				Succeeded,
				Failed,
			};

			explicit PracticeFsm(uint8_t holes = 3, uint8_t course = 0);

			void setSend(PracticeSendFn send);
			void setRoomKey(const unsigned char key[16]);
			void setOid(uint32_t oid);
			void setUserInfoSize(size_t size);

			void onLobbyEntered();
			void onServerPacket(unsigned short tipo, packet& p);

			State state() const { return m_state; }
			bool finished() const { return m_state == State::Succeeded || m_state == State::Failed; }
			bool success() const { return m_state == State::Succeeded; }
			uint8_t holesCompleted() const { return m_holes_done; }
			uint8_t holesTarget() const { return m_holes; }
			const std::string& lastError() const { return m_error; }
			std::string stateName() const;

		private:
			void fail(const std::string& why);
			void beginHole(uint8_t hole_number);
			void playCurrentHole();

			PracticeSendFn m_send;
			State m_state;
			uint8_t m_holes;
			uint8_t m_course;
			uint8_t m_holes_done;
			uint8_t m_current_hole;
			uint32_t m_oid;
			size_t m_user_info_size;
			unsigned char m_room_key[16];
			std::vector<uint8_t> m_hole_numbers;
			std::string m_error;
			bool m_shot_sent;
			bool m_item_sent;
			bool m_finish_sent;
	};
}

#endif
