// Minimal blocking TCP helper used by the Linux Practice bot and loopback peer.

#pragma once
#ifndef _STDA_PRACTICE_TCP_HPP
#define _STDA_PRACTICE_TCP_HPP

#include "../../Projeto IOCP/PACKET/packet.h"
#include "../../Projeto IOCP/UTIL/exception.h"
#include "../../Projeto IOCP/UTIL/message_pool.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace stdA {

	class PracticeTcp {
		public:
			PracticeTcp() : m_fd(-1), m_key(0) {}
			~PracticeTcp() { close(); }

			PracticeTcp(const PracticeTcp&) = delete;
			PracticeTcp& operator=(const PracticeTcp&) = delete;

			void setKey(unsigned char key) { m_key = key; }
			unsigned char key() const { return m_key; }
			int fd() const { return m_fd; }

			void attach(int fd) {
				close();
				m_fd = fd;
			}

			void close() {
#if defined(__linux__)
				if (m_fd >= 0) {
					::shutdown(m_fd, SHUT_RDWR);
					::close(m_fd);
				}
#endif
				m_fd = -1;
			}

			bool connect_to(const std::string& host, uint16_t port) {
#if defined(__linux__)
				close();
				m_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (m_fd < 0)
					return false;
				int nodelay = 1;
				setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
				sockaddr_in sa{};
				sa.sin_family = AF_INET;
				sa.sin_port = htons(port);
				if (inet_pton(AF_INET, host.c_str(), &sa.sin_addr) != 1) {
					close();
					return false;
				}
				if (::connect(m_fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) != 0) {
					close();
					return false;
				}
				return true;
#else
				(void)host;
				(void)port;
				return false;
#endif
			}

			bool send_client(packet& p) {
				try {
					p.make(m_key);
					return write_all(p.getMakedBuf().buf, p.getMakedBuf().len);
				} catch (exception& e) {
					std::cout << "[PracticeTcp] send_client: " << e.getFullMessageError() << std::endl;
					return false;
				}
			}

			bool send_server(packet& p) {
				try {
					p.makeFull(m_key);
					return write_all(p.getMakedBuf().buf, p.getMakedBuf().len);
				} catch (exception& e) {
					std::cout << "[PracticeTcp] send_server: " << e.getFullMessageError() << std::endl;
					return false;
				}
			}

			// First Login (0x00) / Game (0x3F) hello: no crypt, no compress.
			bool send_raw(packet& p) {
				try {
					p.makeRaw();
					return write_all(p.getMakedBuf().buf, p.getMakedBuf().len);
				} catch (exception& e) {
					std::cout << "[PracticeTcp] send_raw: " << e.getFullMessageError() << std::endl;
					return false;
				}
			}

			// Read one framed server->client packet (packet_head + payload) and unMakeFull.
			bool recv_server(packet& out) {
				unsigned char hdr[3];
				if (!read_exact(hdr, sizeof(hdr)))
					return false;
				unsigned short size = static_cast<unsigned short>(hdr[1] | (hdr[2] << 8));
				std::vector<unsigned char> body(size);
				if (size > 0 && !read_exact(body.data(), size))
					return false;
				out.reset();
				out.init_maked(sizeof(hdr) + size);
				out.add_maked(hdr, sizeof(hdr));
				if (size > 0)
					out.add_maked(body.data(), size);
				try {
					out.unMakeFull(m_key);
					out.init_maked();
				} catch (exception& e) {
					std::cout << "[PracticeTcp] recv_server unMakeFull: " << e.getFullMessageError() << std::endl;
					return false;
				}
				return true;
			}

			// Read one framed client->server packet (packet_head_client + payload) and unMake.
			bool recv_client(packet& out) {
				unsigned char hdr[4];
				if (!read_exact(hdr, sizeof(hdr)))
					return false;
				unsigned short size = static_cast<unsigned short>(hdr[1] | (hdr[2] << 8));
				std::vector<unsigned char> body(size);
				if (size > 0 && !read_exact(body.data(), size))
					return false;
				out.reset();
				out.init_maked(sizeof(hdr) + size);
				out.add_maked(hdr, sizeof(hdr));
				if (size > 0)
					out.add_maked(body.data(), size);
				try {
					out.unMake(m_key);
					out.init_maked();
				} catch (exception& e) {
					std::cout << "[PracticeTcp] recv_client unMake: " << e.getFullMessageError() << std::endl;
					return false;
				}
				return true;
			}

		private:
			bool write_all(const char* data, size_t len) {
#if defined(__linux__)
				size_t off = 0;
				while (off < len) {
					ssize_t n = ::send(m_fd, data + off, len - off, MSG_NOSIGNAL);
					if (n <= 0)
						return false;
					off += static_cast<size_t>(n);
				}
				return true;
#else
				(void)data;
				(void)len;
				return false;
#endif
			}

			bool read_exact(void* data, size_t len) {
#if defined(__linux__)
				auto* p = static_cast<unsigned char*>(data);
				size_t off = 0;
				while (off < len) {
					ssize_t n = ::recv(m_fd, p + off, len - off, 0);
					if (n <= 0)
						return false;
					off += static_cast<size_t>(n);
				}
				return true;
#else
				(void)data;
				(void)len;
				return false;
#endif
			}

			int m_fd;
			unsigned char m_key;
	};
}

#endif
