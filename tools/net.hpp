#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <iostream>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#endif

namespace smallgine {

// One networked entity snapshot (id + position), sent as a raw UDP datagram.
struct NetPacket { uint32_t id; float x, y, z; };

#ifndef _WIN32
// Loopback UDP client/server for in-process state replication: the server sends
// authoritative entity positions; the client receives and applies them. Proves
// a real socket round-trip (serialization + UDP send/recv), not a fake.
class NetSystem {
private:
    int serverSock = -1, clientSock = -1;
    sockaddr_in clientAddr{};
    bool up = false;

public:
    bool active() const { return up; }

    bool start(unsigned short port = 54000)
    {
        serverSock = socket(AF_INET, SOCK_DGRAM, 0);
        clientSock = socket(AF_INET, SOCK_DGRAM, 0);
        if (serverSock < 0 || clientSock < 0) { std::cout << "Net: socket() failed" << std::endl; return false; }

        sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK); sa.sin_port = htons(port);
        if (bind(serverSock, (sockaddr*)&sa, sizeof(sa)) < 0) { std::cout << "Net: server bind failed" << std::endl; return false; }

        clientAddr = {}; clientAddr.sin_family = AF_INET; clientAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); clientAddr.sin_port = htons(port + 1);
        if (bind(clientSock, (sockaddr*)&clientAddr, sizeof(clientAddr)) < 0) { std::cout << "Net: client bind failed" << std::endl; return false; }
        fcntl(clientSock, F_SETFL, O_NONBLOCK);

        up = true;
        std::cout << "Net: UDP loopback up (server :" << port << " -> client :" << (port + 1) << ")" << std::endl;
        return true;
    }

    void send(uint32_t id, const glm::vec3& p)
    {
        if (!up) return;
        NetPacket pk{ id, p.x, p.y, p.z };
        sendto(serverSock, &pk, sizeof(pk), 0, (sockaddr*)&clientAddr, sizeof(clientAddr));
    }

    // Non-blocking receive of the latest packet. Returns false when none pending.
    bool poll(uint32_t& id, glm::vec3& p)
    {
        if (!up) return false;
        NetPacket pk;
        ssize_t n = recvfrom(clientSock, &pk, sizeof(pk), 0, nullptr, nullptr);
        if (n == (ssize_t)sizeof(pk)) { id = pk.id; p = glm::vec3(pk.x, pk.y, pk.z); return true; }
        return false;
    }

    void stop()
    {
        if (serverSock >= 0) close(serverSock);
        if (clientSock >= 0) close(clientSock);
        serverSock = clientSock = -1;
        up = false;
    }
};

#else // _WIN32: needs Winsock (WSAStartup + closesocket); stubbed for now.
class NetSystem {
public:
    bool active() const { return false; }
    bool start(unsigned short = 54000) { std::cout << "Net: Windows build needs Winsock (stub)" << std::endl; return false; }
    void send(uint32_t, const glm::vec3&) {}
    bool poll(uint32_t&, glm::vec3&) { return false; }
    void stop() {}
};
#endif

} // namespace smallgine
