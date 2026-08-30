// Optional SuperSS endpoint config for the IOCP Practice / VS bot.
// Default is 127.0.0.1 and the official Login/Game/Auth/Message/Rank ports.

#pragma once
#ifndef _STDA_BOT_CONFIG_HPP
#define _STDA_BOT_CONFIG_HPP

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

namespace stdA {

	struct BotEndpoint {
		std::string host{"127.0.0.1"};
		uint16_t port = 0;
		bool override_list = false;
		bool host_set = false;
	};

	struct BotConfig {
		std::string default_host{"127.0.0.1"};
		BotEndpoint login{ "127.0.0.1", 10303, false };
		BotEndpoint game{ "127.0.0.1", 20203, false };
		BotEndpoint auth{ "127.0.0.1", 7777, false };
		BotEndpoint message{ "127.0.0.1", 30303, false };
		BotEndpoint rank{ "127.0.0.1", 4774, false };
		std::string user;
		std::string pass;
		std::string guest_user;
		std::string guest_pass;
		std::string source;
		bool loaded = false;
	};

	inline std::string botConfigTemplateText() {
		return
			"; SuperSS IOCP bot endpoints.\n"
			"; Default is localhost and the official Docker / server.ini ports.\n"
			";\n"
			"; Copy this file and pass it to the bot:\n"
			";   ./Multi Client/practice_bot --real --config bot.ini\n"
			";   ./Multi Client/practice_bot --real --vs --config bot.ini\n"
			";   ./Multi Client/practice_bot --real --tourney --config bot.ini\n"
			";\n"
			"; Write a fresh copy:\n"
			";   ./Multi Client/practice_bot --write-template bot.ini\n"
			";\n"
			"; Keys in [DEFAULT] apply to any section that omits host.\n"
			"; Command-line --user / --pass / --guest-* override [ACCOUNT].\n"
			"\n"
			"[DEFAULT]\n"
			"host = 127.0.0.1\n"
			"\n"
			"[LOGIN]\n"
			"host = 127.0.0.1\n"
			"port = 10303\n"
			"\n"
			"[GAME]\n"
			"host = 127.0.0.1\n"
			"port = 20203\n"
			"; 1 = connect here even if Login Server lists another IP (needed when\n"
			"; the Game Server advertises 127.0.0.1 but the bot is on another host)\n"
			"override = 1\n"
			"\n"
			"[AUTH]\n"
			"host = 127.0.0.1\n"
			"port = 7777\n"
			"\n"
			"[MESSAGE]\n"
			"host = 127.0.0.1\n"
			"port = 30303\n"
			"\n"
			"[RANK]\n"
			"host = 127.0.0.1\n"
			"port = 4774\n"
			"\n"
			"[ACCOUNT]\n"
			"user = test\n"
			"pass = 123456\n"
			"guest_user = ciao\n"
			"guest_pass = 123456\n";
	}

	inline bool writeBotConfigTemplate(const std::string& path, std::string& err) {
		std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
		if (!out) {
			err = "cannot write " + path;
			return false;
		}
		out << botConfigTemplateText();
		if (!out) {
			err = "failed writing " + path;
			return false;
		}
		return true;
	}

	namespace bot_config_detail {

		inline std::string trim(std::string s) {
			const auto not_space = [](unsigned char c) { return !std::isspace(c); };
			s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
			s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
			return s;
		}

		inline std::string upper(std::string s) {
			for (char& c : s)
				c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			return s;
		}

		inline std::string lower(std::string s) {
			for (char& c : s)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return s;
		}

		inline void applyHostPort(BotEndpoint& ep, const std::string& key, const std::string& val) {
			if (key == "HOST" || key == "IP") {
				ep.host = val;
				ep.host_set = true;
			} else if (key == "PORT")
				ep.port = static_cast<uint16_t>(std::stoi(val));
			else if (key == "OVERRIDE")
				ep.override_list = (val == "1" || lower(val) == "true" || lower(val) == "yes");
		}
	}

	inline bool loadBotConfig(const std::string& path, BotConfig& cfg, std::string& err) {
		std::ifstream in(path.c_str());
		if (!in) {
			err = "cannot open " + path;
			return false;
		}

		cfg = BotConfig{};
		cfg.login = BotEndpoint{};
		cfg.game = BotEndpoint{};
		cfg.auth = BotEndpoint{};
		cfg.message = BotEndpoint{};
		cfg.rank = BotEndpoint{};
		cfg.source = path;
		std::string section;
		std::string line;
		int lineno = 0;
		try {
			while (std::getline(in, line)) {
				++lineno;
				auto hash = line.find_first_of(";#");
				if (hash != std::string::npos)
					line = line.substr(0, hash);
				line = bot_config_detail::trim(line);
				if (line.empty())
					continue;
				if (line.front() == '[' && line.back() == ']') {
					section = bot_config_detail::upper(bot_config_detail::trim(line.substr(1, line.size() - 2)));
					continue;
				}
				const auto eq = line.find('=');
				if (eq == std::string::npos) {
					err = path + ":" + std::to_string(lineno) + ": missing '='";
					return false;
				}
				const std::string key = bot_config_detail::upper(bot_config_detail::trim(line.substr(0, eq)));
				const std::string val = bot_config_detail::trim(line.substr(eq + 1));
				if (section == "DEFAULT") {
					if (key == "HOST" || key == "IP")
						cfg.default_host = val;
				} else if (section == "LOGIN") {
					bot_config_detail::applyHostPort(cfg.login, key, val);
				} else if (section == "GAME") {
					bot_config_detail::applyHostPort(cfg.game, key, val);
				} else if (section == "AUTH") {
					bot_config_detail::applyHostPort(cfg.auth, key, val);
				} else if (section == "MESSAGE" || section == "MSN" || section == "MSG") {
					bot_config_detail::applyHostPort(cfg.message, key, val);
				} else if (section == "RANK") {
					bot_config_detail::applyHostPort(cfg.rank, key, val);
				} else if (section == "ACCOUNT") {
					if (key == "USER")
						cfg.user = val;
					else if (key == "PASS" || key == "PASSWORD")
						cfg.pass = val;
					else if (key == "GUEST_USER" || key == "GUESTUSER")
						cfg.guest_user = val;
					else if (key == "GUEST_PASS" || key == "GUESTPASS" || key == "GUEST_PASSWORD")
						cfg.guest_pass = val;
				}
			}
		} catch (const std::exception& e) {
			err = path + ":" + std::to_string(lineno) + ": " + e.what();
			return false;
		}

		auto fill_host = [&](BotEndpoint& ep) {
			if (!ep.host_set || ep.host.empty())
				ep.host = cfg.default_host.empty() ? "127.0.0.1" : cfg.default_host;
		};
		fill_host(cfg.login);
		fill_host(cfg.game);
		fill_host(cfg.auth);
		fill_host(cfg.message);
		fill_host(cfg.rank);
		if (cfg.login.port == 0)
			cfg.login.port = 10303;
		if (cfg.game.port == 0)
			cfg.game.port = 20203;
		if (cfg.auth.port == 0)
			cfg.auth.port = 7777;
		if (cfg.message.port == 0)
			cfg.message.port = 30303;
		if (cfg.rank.port == 0)
			cfg.rank.port = 4774;
		cfg.loaded = true;
		return true;
	}

	inline std::string botConfigSummary(const BotConfig& cfg) {
		std::ostringstream os;
		os << "login=" << cfg.login.host << ":" << cfg.login.port
			<< " game=" << cfg.game.host << ":" << cfg.game.port
			<< " override=" << (cfg.game.override_list ? "1" : "0")
			<< " auth=" << cfg.auth.host << ":" << cfg.auth.port
			<< " message=" << cfg.message.host << ":" << cfg.message.port
			<< " rank=" << cfg.rank.host << ":" << cfg.rank.port;
		if (!cfg.source.empty())
			os << " file=" << cfg.source;
		return os.str();
	}
}

#endif
