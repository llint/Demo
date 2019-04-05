//
//  DatagramUnix.cpp
//  Netran
//
//  Created by Lin Luo on 05/01/2015.
//

#include "IDatagram.h"

#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

namespace Netran
{
    // the output ipv4 and port are all in network byte order
    static inline void parse_address(const Address& addr, in_addr* ipv4, uint16_t* port)
    {
        ipv4->s_addr = 0;
        *port = 0;

        size_t p = addr.find(':');
        if (p != Address::npos)
            *port = htons( std::stoi(addr.substr(p + 1).c_str()) );
        ipv4->s_addr = inet_addr( addr.substr(0, p).c_str() );
    }

    // the input ipv4 and port are all in the network byte order
    static inline void make_address(Address& addr, in_addr ipv4, uint16_t port)
    {
        std::stringstream ss;
        ss << inet_ntoa(ipv4) << ":" << ntohs(port);
        addr = ss.str();
    }
    
    static const size_t MAX_PACKET_SIZE = 8 * 1024;
    
    class DatagramUnix : public IDatagram
    {
    public:
        DatagramUnix()
            : m_socket(-1)
            , m_buffer(MAX_PACKET_SIZE)
            , m_nbytes(0)
        {
            memset(&m_sain, 0, sizeof(m_sain));
        }

        ~DatagramUnix()
        {
            Term();
        }

        void Init(const Address& addr)
        {
            Term();

            m_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

            int flags = 0;
            if (-1 == (flags = fcntl(m_socket, F_GETFL, 0)))
                flags = 0;
            fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);

            in_addr ipv4 = {0};
            uint16_t port = 0;
            parse_address(addr, &ipv4, &port);
            memset(&m_sain, 0, sizeof(m_sain));
            m_sain.sin_family = AF_INET;
            m_sain.sin_addr = ipv4;
            m_sain.sin_port = port;
            bind(m_socket, (sockaddr*)&m_sain, sizeof(m_sain));
        }

        void Term()
        {
            close(m_socket);
            m_socket = -1;
        }

        void Send(const Address& addr, const Buffer& data)
        {
            in_addr ipv4 = {0};
            uint16_t port = 0;
            parse_address(addr, &ipv4, &port);

            sockaddr_in sain;
            memset(&sain, 0, sizeof(sain));
            sain.sin_family = AF_INET;
            sain.sin_addr = ipv4;
            sain.sin_port = port;
            sendto(m_socket, (const char*)&data[0], (int)data.size(), 0, (sockaddr*)&sain, sizeof(sain));
        }

        bool Recv(Address& addr, Buffer& data)
        {
            sockaddr_in sain;
            memset(&sain, 0, sizeof(sain));
            socklen_t slen = sizeof(sain);
            ssize_t ret = recvfrom(m_socket, m_buffer.data(), m_buffer.size(), 0, (sockaddr*)&sain, &slen);
            if (ret == -1)
                return false; // in case of no data or other errors

            make_address(addr, sain.sin_addr, sain.sin_port);
            data.resize(ret);
            memcpy(&data[0], &m_buffer[0], ret);
            return true;
        }

    private:
        typedef int SOCKET;
        SOCKET m_socket;

        sockaddr_in m_sain;

        Buffer m_buffer;
        size_t m_nbytes;
    };

    IDatagram::ptr IDatagram::CreateInstance()
    {
        return IDatagram::ptr( new DatagramUnix() );
    }
}

