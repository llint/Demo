//
//  NetranImpl.cpp
//  Netran
//
//  Created by Lin Luo on 05/01/2015.
//

#include <cassert>

#include "NetranImpl.h"

using namespace Netran;

// two's complement unsigned comparison
static inline bool eq(uint16_t seq1, uint16_t seq2)
{
	return seq1 == seq2;
}

static inline bool gt(uint16_t seq1, uint16_t seq2)
{
	int16_t diff = seq1 - seq2;

	return diff > 0;
}

static inline bool ge(uint16_t seq1, uint16_t seq2)
{
	int16_t diff = seq1 - seq2;

	return diff >= 0;
}

static inline bool lt(uint16_t seq1, uint16_t seq2)
{
	int16_t diff = seq1 - seq2;

	return diff < 0;
}

static inline bool le(uint16_t seq1, uint16_t seq2)
{
	int16_t diff = seq1 - seq2;

	return diff <= 0;
}

bool Connection::Less::operator ()(uint16_t s1, uint16_t s2) const
{
    return lt(s1, s2);
}

struct Header
{
	uint16_t seqnum; // sequence number of this packet
	uint16_t acknum; // acknowledgment
	uint16_t pflags; // higher order byte denotes rwnd, lower order byte denotes packet flags (reliability, acknowledgment, etc.)
	uint16_t length; // length of the following data in bytes
};

static const uint16_t FLAG_ALL = 0x00ff;
static const uint16_t FLAG_RLB = 0x0001; // reliable
static const uint16_t FLAG_ACK = 0x0002; // acknowledgment
static const uint16_t FLAG_SYN = 0x0004; // synchronization
static const uint16_t FLAG_RST = 0x0008; // reset
static const uint16_t FLAG_PIN = 0x0010; // ping
static const uint16_t FLAG_PON = 0x0020; // pong
static const uint16_t FLAG_BWP = 0x0040; // bandwidth polling
static const uint16_t FLAG_BWR = 0x0080; // bandwidth report

#define RETX_INTERVAL 500.0 // retransmission interval, in milliseconds
#define RETX_COUNT 120 // retransmission count

#define PING_TIMEOUT 1000.0 // milliseconds
#define BANDWIDTH_ESTIMATION_TIMEOUT 1000.0 // milliseconds

static const size_t MAXNUM_PACKETS_PER_CYCLE = 256;
static const size_t SIZE_BW_POLL = 512;

Connection::Connection(bool master) :
m_master(master),
m_state(State::STATE_CLOSED),
m_unreliable_outgoing_sequence(0),
m_unreliable_incoming_sequence(0),
m_reliable_outgoing_sequence(0),
m_reliable_lowest_acceptable_sequence(0),
m_reliable_latest_legal_ack(0),
m_reliable_duplicated_ack_count(0),
m_ping_time(0.0),
m_ping_timeout(0.0),
m_ping_timestamp(0.0),
m_bandwidth(0.0),
m_bandwidth_timeout(0.0),
m_bandwidth_timestamp(0.0)
{
    m_fsm[(size_t)State::STATE_CLOSED] = &Connection::state_closed;
    m_fsm[(size_t)State::STATE_LISTEN] = &Connection::state_listen;
    m_fsm[(size_t)State::STATE_SYNRCVD] = &Connection::state_synrcvd;
    m_fsm[(size_t)State::STATE_SYNSENT] = &Connection::state_synsent;
    m_fsm[(size_t)State::STATE_ESTABED] = &Connection::state_estabed;
}

Connection::~Connection()
{
}

void Connection::reset(bool broken)
{
    if (broken)
    {
        switch (m_state)
        {
            case State::STATE_ESTABED:
                if (m_server)
                    m_server->m_listener->OnDeleteConnection( IConnection::ptr(this) );
                if (m_client)
                    m_client->m_listener->OnConnectionBroken();
                break;

            case State::STATE_SYNSENT:
                m_client->m_listener->OnConnectComplete(nullptr);
                break;
        }
    }

    m_server.reset();
    m_client.reset();
    m_socket.reset();
    m_listener.reset();
    m_raddr.resize(0);
    m_state = State::STATE_CLOSED;
    m_unreliable_outgoing_sequence = 0;
    m_unreliable_incoming_sequence = 0;
    m_reliable_retransmission_queue.clear();
    m_reliable_incoming_queue.resize(0);
    m_reliable_reassembly_list.clear();
    m_reliable_outgoing_sequence = 0;
    m_reliable_lowest_acceptable_sequence = 0;
    m_reliable_latest_legal_ack = 0;
    m_reliable_duplicated_ack_count = 0;
    m_ping_time = 0.0;
    m_ping_timeout = 0.0;
    m_ping_timestamp = 0.0;
    m_bandwidth = 0.0;
    m_bandwidth_timeout = 0.0;
    m_bandwidth_timestamp = 0.0;
}

void Connection::Listen(ServerPtr server)
{
    if (!m_master || m_state != State::STATE_CLOSED)
    {
        // only a master connection can "listen"
        return;
    }

    m_socket = IDatagram::weak_ptr( server->m_socket.get() );
    m_server = std::move(server);

    m_state = State::STATE_LISTEN;
}

void Connection::Connect(const Address& raddr, ClientPtr client)
{
    if (!m_master || m_state != State::STATE_CLOSED)
    {
        // only a master connection can "connect"
        return;
    }

    m_raddr = raddr;

    m_socket = IDatagram::weak_ptr( client->m_socket.get() );
    m_client = std::move(client);

    time_t t;
    time(&t);
    uint16_t isn = (uint16_t)t;

    Buffer packet(sizeof(Header));
    Header* header = reinterpret_cast<Header*>(&packet[0]);
    header->seqnum = isn;
    header->acknum = 0;
    header->pflags = FLAG_RLB | FLAG_SYN;
    header->length = 0;

    m_socket->Send(raddr, packet);

    m_reliable_retransmission_queue.emplace( std::piecewise_construct, std::forward_as_tuple(header->seqnum), std::forward_as_tuple(RETX_INTERVAL, RETX_COUNT, std::move(packet)) );

    m_unreliable_outgoing_sequence = isn;
    m_reliable_outgoing_sequence = isn + 1;

    m_state = State::STATE_SYNSENT;
}

void Connection::Close()
{
    if (m_state == State::STATE_CLOSED)
    {
        return;
    }

    if (m_state == State::STATE_LISTEN)
    {
        for (auto& each : m_children)
        {
            each.second->Close();
        }

        m_children.clear();
    }
    else
    {
        send_reset(m_raddr);
    }

    reset();
}

void Connection::Setup(IListener::ptr listener)
{
    m_listener = std::move(listener);

    while ( !m_reliable_incoming_queue.empty() )
    {
        m_listener->OnIncomingData( std::move( m_reliable_incoming_queue.front() ) );
        m_reliable_incoming_queue.pop_front();
    }
}

void Connection::Send(const Buffer& data, bool reliable)
{
    if (m_state != State::STATE_ESTABED)
    {
        return;
    }

    Buffer packet( sizeof(Header) + data.size() );
    Header* header = reinterpret_cast<Header*>(&packet[0]);

    if (reliable)
    {
        header->seqnum = m_reliable_outgoing_sequence++;
        header->pflags = FLAG_RLB;
    }
    else
    {
        header->seqnum = m_unreliable_outgoing_sequence++;
        header->pflags = 0;
    }

    header->acknum = 0;
    header->length = data.size();
    memcpy( header + 1, &data[0], data.size() );
    m_socket->Send(m_raddr, packet);

    if (reliable)
    {
        m_reliable_retransmission_queue.emplace( std::piecewise_construct, std::forward_as_tuple(header->seqnum), std::forward_as_tuple(RETX_INTERVAL, RETX_COUNT, std::move(packet)) );
    }
}

void Connection::Kick(const Address& raddr)
{
    if (!m_master || m_state != State::STATE_LISTEN)
    {
        return;
    }

    auto it = m_children.find(raddr);
    if (it != m_children.end())
    {
        it->second->Close();
        m_children.erase(it);
    }
}

void Connection::check_timeout(float elapsed) // milliseconds
{
    // NB: I'm going through the entire retransmission queue, hoping that the size of this queue should not be large
    // I'm not reordering the queue based on time, since the queue is ordered by packet sequence number, so whenever
    // there is an ack coming in, I could quickly remove as many entries as the ack covers!
    for (auto& each : m_reliable_retransmission_queue)
    {
        RetransmissionInfo& info = each.second;
        if (info.timeout <= elapsed)
        {
            if (info.count == 0)
            {
                reset(true);
                return;
            }

            m_socket->Send(m_raddr, info.buffer);

            info.timeout = RETX_INTERVAL;
            --info.count;
        }
        else
        {
            info.timeout -= elapsed;
        }
    }

    if (m_state == State::STATE_ESTABED)
    {
        if (m_ping_timeout <= elapsed)
        {
            m_ping_timestamp = Timer::Now();
            send_ping(m_raddr, m_ping_timestamp);

            m_ping_timeout = PING_TIMEOUT;
        }
        else
        {
            m_ping_timeout -= elapsed;
        }

        if (m_bandwidth_timeout <= elapsed)
        {
            send_bw_poll(m_raddr, Timer::Now());

            m_bandwidth_timeout = BANDWIDTH_ESTIMATION_TIMEOUT;
        }
        else
        {
            m_bandwidth_timeout -= elapsed;
        }
    }
}

void Connection::send_bw_poll(const Address& raddr, float timestamp)
{
    Buffer packet(SIZE_BW_POLL);
    Header* header = reinterpret_cast<Header*>(&packet[0]);
    *(float*)header = timestamp; // NB: float is 32 bits
    header->length = SIZE_BW_POLL - sizeof(Header);
    header->pflags = FLAG_BWP | 0x0000;
    m_socket->Send(raddr, packet);
    header->pflags = FLAG_BWP | 0x0100;
    m_socket->Send(raddr, packet);
}

void Connection::send_bw_rslt(const Address& raddr, float bandwidth)
{
    Buffer packet( sizeof(Header) );
    Header* header = reinterpret_cast<Header*>(&packet[0]);
    *(float*)header = bandwidth;
    header->pflags = FLAG_BWR;
    header->length = 0;
    m_socket->Send(raddr, packet);
}

void Connection::send_ping(const Address& raddr, float timestamp)
{
    Buffer packet( sizeof(Header) );
    Header* header = reinterpret_cast<Header*>(&packet[0]);
    *(float*)header = timestamp;
    header->pflags = FLAG_PIN;
    header->length = 0;
    m_socket->Send(raddr, packet);
}

void Connection::send_pong(const Address& raddr, float timestamp)
{
    Buffer packet( sizeof(Header) );
    Header* header = reinterpret_cast<Header*>(&packet[0]);
    *(float*)header = timestamp;
    header->pflags = FLAG_PON;
    header->length = 0;
    m_socket->Send(raddr, packet);
}

void Connection::send_reset(const Address& raddr)
{
    Buffer packet( sizeof(Header) );
    Header* header = reinterpret_cast<Header*>(&packet[0]);
    header->seqnum = 0;
    header->acknum = 0;
    header->pflags = FLAG_RST;
    header->length = 0;
    m_socket->Send(raddr, packet);
}

void Connection::send_ack(const Address& raddr, uint16_t acknum)
{
    Buffer packet( sizeof(Header) );
    Header* header = reinterpret_cast<Header*>(&packet[0]);
    header->seqnum = 0;
    header->acknum = acknum;
    header->pflags = FLAG_ACK;
    header->length = 0;
    m_socket->Send(raddr, packet);
}

void Connection::Tick()
{
    if (!m_master || m_state == State::STATE_CLOSED)
    {
        return;
    }

    // 1. incoming packets
    for (size_t count = 0; count < MAXNUM_PACKETS_PER_CYCLE; ++count)
    {
        Buffer packet;
        Address raddr;
        if ( !m_socket->Recv(raddr, packet) )
            break;
        if ( packet.size() < sizeof(Header) )
            continue; // malicious?
        Header* header = reinterpret_cast<Header*>(&packet[0]);
        if ( sizeof(Header) + header->length != packet.size() )
            continue; // malicious?
        (this->*m_fsm[(size_t)m_state])( raddr, std::move(packet) );
        if (m_state == State::STATE_CLOSED)
            return; // this could happen as a result of handling incoming packets
    }

    // 2. timeouts and retransmission
    float elapsed = m_timer.GetElapsedMilliseconds();
    if (m_state == State::STATE_LISTEN)
    {
        ConnectionsMap::iterator it, nx = m_children.begin();
        while (nx != m_children.end())
        {
            it = nx++;

            Connection::ptr& connection = it->second;
            connection->check_timeout(elapsed);
            if (connection->m_state == State::STATE_CLOSED)
            {
                m_children.erase(it);
            }
        }
    }
    else
    {
        check_timeout(elapsed);
    }
}

void Connection::state_closed(const Address& raddr, Buffer&& packet)
{
    const Header* header = reinterpret_cast<const Header*>(&packet[0]);
    if ( (header->pflags & FLAG_RST) == 0 )
        send_reset(raddr);
}

void Connection::state_listen(const Address& raddr, Buffer&& packet)
{
    if (!m_master) return;

    auto it = m_children.find(raddr);
    if (it != m_children.end())
    {
        Connection* connection = it->second.get();
        (connection->*connection->m_fsm[(size_t)connection->m_state])(raddr, std::move(packet));
        if (connection->m_state == State::STATE_CLOSED)
        {
            m_children.erase(it);
        }
    }
    else
    {
        const Header* header = reinterpret_cast<const Header*>(&packet[0]);
        if ( header->pflags != (FLAG_RLB | FLAG_SYN) ) // malicious?
        {
            if ( (header->pflags & FLAG_RST) == 0 )
            {
                send_reset(raddr);
            }
        }
        else
        {
            auto r = m_children.emplace( raddr, ptr(new Connection(false)) );
            auto& connection = r.first->second;
            connection->m_server.reset( m_server.get() );
            connection->m_socket.reset( m_socket.get() );
            connection->m_raddr = raddr;
            connection->m_unreliable_incoming_sequence = header->seqnum;
            connection->m_reliable_lowest_acceptable_sequence = header->seqnum + 1;

            time_t t;
            time(&t);
            uint16_t isn = (uint16_t)t;

            Buffer pkt(sizeof(Header));
            Header* hdr = reinterpret_cast<Header*>(&pkt[0]);
            hdr->seqnum = isn;
            hdr->acknum = header->seqnum + 1;
            hdr->pflags = FLAG_RLB | FLAG_SYN | FLAG_ACK;
            hdr->length = 0;
            m_socket->Send(raddr, pkt);

            connection->m_reliable_retransmission_queue.emplace( std::piecewise_construct, std::forward_as_tuple(hdr->seqnum), std::forward_as_tuple(RETX_INTERVAL, RETX_COUNT, std::move(pkt)) );

            connection->m_unreliable_outgoing_sequence = isn;
            connection->m_reliable_outgoing_sequence = isn + 1;
            connection->m_state = State::STATE_SYNRCVD;
        }
    }
}

void Connection::state_synsent(const Address& raddr, Buffer&& packet)
{
    // the connection in question must be the client master
    if (!m_master || !m_client) return;

    const Header* header = reinterpret_cast<const Header*>(&packet[0]);

    try
    {
        if (header->pflags & FLAG_RST)
            throw -1;

        if (m_raddr != raddr)
        {
            send_reset(raddr);
            throw -1;
        }

        if ( (header->pflags & FLAG_ALL) != (FLAG_RLB | FLAG_SYN | FLAG_ACK) )
        {
            send_reset(raddr);
            throw -1;
        }

        if ( !eq(header->acknum, m_reliable_outgoing_sequence) )
        {
            send_reset(raddr);
            throw -1;
        }
    }
    catch(int)
    {
        m_client->m_listener->OnConnectComplete(nullptr);

        reset();
        return;
    }

    m_reliable_latest_legal_ack = header->acknum;

    assert( m_reliable_retransmission_queue.size() == 1 );
    m_reliable_retransmission_queue.erase( m_reliable_retransmission_queue.begin() );

    m_unreliable_incoming_sequence = header->seqnum;
    m_reliable_lowest_acceptable_sequence = header->seqnum + 1;

    send_ack(raddr, m_reliable_lowest_acceptable_sequence);

    m_state = State::STATE_ESTABED;

    m_client->m_listener->OnConnectComplete( IConnection::ptr(this) );
}

void Connection::state_synrcvd(const Address& raddr, Buffer&& packet)
{
    const Header* header = reinterpret_cast<const Header*>(&packet[0]);

    if ( (header->pflags & FLAG_RST) != 0 )
    {
        reset(true);
        return;
    }

    if ( (header->pflags & FLAG_ALL) != FLAG_ACK )
        return; // could be an early arriving data packet

    if (header->acknum != m_reliable_outgoing_sequence)
    {
        send_reset(raddr);
        reset(true);
        return;
    }

    m_reliable_latest_legal_ack = header->acknum;

    assert( m_reliable_retransmission_queue.size() == 1 );
    m_reliable_retransmission_queue.erase( m_reliable_retransmission_queue.begin() );

    m_state = State::STATE_ESTABED;

    m_server->m_listener->OnCreateConnection( IConnection::ptr(this) );
}

void Connection::state_estabed(const Address& raddr, Buffer&& packet)
{
    if (m_master && m_raddr != raddr)
    {
        // the master client connection receives a packet not destined correctly
        send_reset(raddr);
        return;
    }

    const Header* header = reinterpret_cast<const Header*>(&packet[0]);

    if (header->pflags & FLAG_RST)
    {
        reset(true);
        return;
    }

    if (header->pflags & FLAG_PIN)
    {
        send_pong(raddr, *(float*)header);
        return;
    }

    if (header->pflags & FLAG_PON)
    {
        float timestamp = *(float*)header;
        if (m_ping_timestamp == timestamp)
        {
            m_ping_time = Timer::Now() - m_ping_timestamp;
        }
        return;
    }

    if (header->pflags & FLAG_BWP)
    {
        if ( (header->pflags & 0xff00) == 0x0000 )
        {
            m_bandwidth_timestamp = *(float*)header;
            m_timer_bw.Reset();
        }
        else if ( (header->pflags & 0xff00) == 0x0100 )
        {
            if ( m_bandwidth_timestamp == *(float*)header )
            {
                float bandwidth = SIZE_BW_POLL / m_timer_bw.GetElapsedMilliseconds() * 1000.0f; // Bytes per second
                send_bw_rslt(raddr, bandwidth);
            }
        }
        return;
    }

    if (header->pflags & FLAG_BWR)
    {
        m_bandwidth = *(float*)header;
        return;
    }

    if (header->pflags & FLAG_ACK)
    {
        if (header->length > 0)
            return; // currently we don't support embedded ACK

        if ( gt(header->acknum, m_reliable_outgoing_sequence) )
        {
            // ACK beyond realistic sequence
            send_reset(raddr);
            reset(true);
            return;
        }

        // fast retransmit
        if ( eq(header->acknum, m_reliable_latest_legal_ack) && !m_reliable_retransmission_queue.empty() && ++m_reliable_duplicated_ack_count >= 3 )
        {
            const Buffer& pkt = m_reliable_retransmission_queue.begin()->second.buffer;
            m_socket->Send(raddr, pkt);

            m_reliable_duplicated_ack_count = 0;

            return;
        }

        // NB: ack is one bigger than the receiver received!
        auto it = m_reliable_retransmission_queue.lower_bound(header->acknum);
        bool legal = !m_reliable_retransmission_queue.empty() && it != m_reliable_retransmission_queue.begin();
        if (legal)
        {
            m_reliable_latest_legal_ack = header->acknum;
            m_reliable_duplicated_ack_count = 0;
        }
        m_reliable_retransmission_queue.erase( m_reliable_retransmission_queue.begin(), it );
    }

    if (header->length == 0)
        return;

    // reliable packet
    if (header->pflags & FLAG_RLB)
    {
        // buffer new packets in the reassembly list, and process contiguous ones;
        // old packets are discarded silently
        if ( ge(header->seqnum, m_reliable_lowest_acceptable_sequence) )
        {
            // put the new packet into the reassembly list regardlessly, then start flushing
            // the contiguous ones starting from the lowest acceptable seqnum
            m_reliable_reassembly_list.emplace(header->seqnum, std::move(packet)); // take ownership of the packet

            uint16_t current = m_reliable_lowest_acceptable_sequence;
            auto i = m_reliable_reassembly_list.begin();
            for (auto e = m_reliable_reassembly_list.end(); i != e; ++i)
            {
                // bail if the packet seqnum is not equal to the current acceptable seqnum
                if (i->first != current)
                    break;

                // packet is deliverable to the application layer
                const Buffer& pkt = i->second;
                const Header* hdr = reinterpret_cast<const Header*>(&pkt[0]);
                Buffer data(hdr->length);
                memcpy( &data[0], &pkt[sizeof(Header)], data.size() );

                if (m_listener)
                {
                    m_listener->OnIncomingData( std::move(data) );
                }
                else
                {
                    m_reliable_incoming_queue.push_back( std::move(data) );
                }

                ++current;
            }

            m_reliable_reassembly_list.erase( m_reliable_reassembly_list.begin(), i );
            m_reliable_lowest_acceptable_sequence = current;
        }

        send_ack(raddr, m_reliable_lowest_acceptable_sequence);
    }
    else // unreliable packet
    {
        if ( lt(header->seqnum, m_unreliable_incoming_sequence) )
            return; // delayed or duplicated (out of order)

        m_unreliable_incoming_sequence = header->seqnum + 1;

        Buffer data(header->length);
        memcpy( &data[0], &packet[sizeof(Header)], data.size() );

        if (m_listener)
        {
            m_listener->OnIncomingData( std::move(data) );
        }
    }
}

const Address& Connection::GetRemoteAddress() const
{
    return m_raddr;
}

float Connection::GetRTT() const
{
    return m_ping_time;
}

float Connection::GetBandwidth() const
{
    return m_bandwidth;
}

Server::Server() :
m_socket( IDatagram::CreateInstance() ),
m_master( new Connection(true) )
{
}

Server::~Server()
{
}

void Server::Setup(IListener::ptr listener)
{
    m_listener = std::move(listener);
}

void Server::Host(const Address& local)
{
    m_socket->Init(local);
    m_master->Listen( ServerPtr(this) );
}

void Server::Kick(const Address& raddr)
{
    m_master->Kick(raddr);
}

void Server::Tick()
{
    m_master->Tick();
}

void Server::Shutdown()
{
    m_master->Close();
    m_socket->Term();

    m_master = nullptr;
    m_socket = nullptr;
}

IServer::ptr IServer::CreateInstance()
{
    return IServer::ptr( new Server() );
}

Client::Client() :
m_socket( IDatagram::CreateInstance() ),
m_master( new Connection(true) )
{
}

Client::~Client()
{
}

void Client::Setup(IListener::ptr listener)
{
    m_listener = std::move(listener);
}

void Client::Connect(const Address& raddr)
{
    m_socket->Init();
    m_master->Connect( raddr, ClientPtr(this) );
}

void Client::Disconnect()
{
    m_master->Close();
}

void Client::Tick()
{
    m_master->Tick();
}

void Client::Shutdown()
{
    m_master->Close();
    m_socket->Term();

    m_master = nullptr;
    m_socket = nullptr;
}

IClient::ptr IClient::CreateInstance()
{
    return IClient::ptr( new Client() );
}

