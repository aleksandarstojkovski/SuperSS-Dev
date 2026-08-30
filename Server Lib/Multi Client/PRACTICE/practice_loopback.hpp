// Loopback Login + Game peer that speaks enough of the official protocol
// for the Practice FSM to complete a 3-hole match without IFF / full GS.

#pragma once
#ifndef _STDA_PRACTICE_LOOPBACK_HPP
#define _STDA_PRACTICE_LOOPBACK_HPP

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace stdA {

	class PracticeLoopback {
		public:
			PracticeLoopback(uint16_t login_port, uint16_t game_port);
			~PracticeLoopback();

			void start();
			void stop();

			uint16_t loginPort() const { return m_login_port; }
			uint16_t gamePort() const { return m_game_port; }
			bool gameFinished() const { return m_game_finished.load(); }
			uint8_t holesPlayed() const { return m_holes_played.load(); }
			std::string lastEvent() const { return m_last_event; }

		private:
			void loginThread();
			void loginThreadInner();
			void gameThread();
			void gameThreadInner();

			uint16_t m_login_port;
			uint16_t m_game_port;
			std::atomic<bool> m_run;
			std::atomic<bool> m_game_finished;
			std::atomic<uint8_t> m_holes_played;
			std::string m_last_event;
			std::thread m_login_th;
			std::thread m_game_th;
	};
}

#endif
