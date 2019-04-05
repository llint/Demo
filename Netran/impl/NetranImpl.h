//
//  Netran.cpp
//  Netran
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef Netran_NetranImpl_h
#define Netran_NetranImpl_h

#include "Netran.h"
#include "IDatagram.h"

namespace Netran
{
    class Server;
    class Client;

    typedef std::unique_ptr< Server, NoDelete<Server> > ServerPtr;
    typedef std::unique_ptr< Client, NoDelete<Client> > ClientPtr;

    class Connection : public IConnection
    {
    public:
        typedef std::unique_ptr<Connection> ptr;

        Connection(bool master);
        ~Connection();

        void Listen(ServerPtr server);

        void Connect(const Address& raddr, ClientPtr client);

        void Setup(IListener::ptr listener) override;

        void Close() override;

        void Send(const Buffer& data, bool reliable) override;

        const Address& GetRemoteAddress() const override;

        float GetRTT() const override;

        float GetBandwidth() const override;

        void Kick(const Address& raddr);

        // NOTE: this tick function is only called with the master connection
        void Tick();

    private:
        const bool m_master;

        Timer m_timer;
        Timer m_timer_bw;

        typedef std::unordered_map<Address, ptr> ConnectionsMap;
        ConnectionsMap m_children;

        ServerPtr m_server;
        ClientPtr m_client;
        IDatagram::weak_ptr m_socket; // this is a reference either to m_server->m_socket, or m_client->m_socket

        IListener::ptr m_listener;

        Address m_raddr;

        enum class State {STATE_CLOSED, STATE_LISTEN, STATE_SYNRCVD, STATE_SYNSENT, STATE_ESTABED, STATE_MAXNUM};
        State m_state;

        uint16_t m_unreliable_outgoing_sequence;
        uint16_t m_unreliable_incoming_sequence;

        struct RetransmissionInfo
        {
            float   timeout;
            size_t  count;
            Buffer  buffer;

            RetransmissionInfo() : timeout(0.0f), count(0) {}
            RetransmissionInfo(float timeout_, size_t count_, Buffer&& buffer_) : timeout(timeout_), count(count_), buffer( std::move(buffer_) ) {}
            RetransmissionInfo(RetransmissionInfo&& rhs) noexcept : timeout(rhs.timeout), count(rhs.count), buffer( std::move(rhs.buffer) ) {}

            RetransmissionInfo& operator=(RetransmissionInfo&& rhs) noexcept
            {
                timeout = rhs.timeout;
                count = rhs.count;
                buffer = std::move(rhs.buffer);
                return *this;
            }
        };

        struct Less
        {
            bool operator()(uint16_t s1, uint16_t s2) const;
        };

        typedef std::map<uint16_t, RetransmissionInfo, Less> RetransmissionQueue;
        RetransmissionQueue m_reliable_retransmission_queue;

        float m_ping_time; // rtt
        float m_ping_timeout;
        float m_ping_timestamp;

        float m_bandwidth;
        float m_bandwidth_timeout;
        float m_bandwidth_timestamp;

        uint16_t m_reliable_outgoing_sequence;
        uint16_t m_reliable_lowest_acceptable_sequence;

        uint16_t m_reliable_latest_legal_ack;
        uint16_t m_reliable_duplicated_ack_count;

        typedef std::deque<Buffer> PacketQueue;
        PacketQueue m_reliable_incoming_queue;

        typedef std::map<uint16_t, Buffer, Less> ReassemblyList; // NB: the packet list is sequence number ordered
        ReassemblyList m_reliable_reassembly_list;

        typedef void (Connection::*StateMachineMethod)(const Address& raddr, Buffer&& data);
        StateMachineMethod m_fsm[(size_t)State::STATE_MAXNUM];

        void state_closed(const Address& raddr, Buffer&& data);
        void state_listen(const Address& raddr, Buffer&& data);
        void state_synrcvd(const Address& raddr, Buffer&& data);
        void state_synsent(const Address& raddr, Buffer&& data);
        void state_estabed(const Address& raddr, Buffer&& data);

        // This function resets the connection; when broken is false, the reset is considered to be active (initiated locally),
        // thus no callback notification to the user layer; when it is true, the reset is considered to be passive, and inside
        // if the connection state is ESTABLISHED, then we need to callback user layer to notify connection broken event; when
        // connection state is not established while the reset is passive, since user layer hasn't got the actual connection (
        // user will only get an well established connection), so in this case, no notification will be sent to the user layer,
        // since user doesn't even know about this premature connection (mostly happens for server child connection); the case
        // for client connection is a bit different, in a sense that the client actually initiates a connection, so premature
        // connection broken event is also delivered to user layer callback
        void reset(bool broken = false);

        void check_timeout(float elapsed);
        
        void send_ping(const Address& raddr, float timestamp);
        void send_pong(const Address& raddr, float timestamp);
        
        void send_bw_poll(const Address& raddr, float timestamp);
        void send_bw_rslt(const Address& raddr, float bandwidth);
        
        void send_ack(const Address& raddr, uint16_t acknum);
        void send_reset(const Address& raddr);
    };

    class Server : public IServer
    {
        friend class Connection;

    public:
        Server();
        ~Server();

        void Setup(IListener::ptr listener) override;

        void Host(const Address& local) override;

        void Kick(const Address& raddr) override;

        void Tick() override;

        void Shutdown() override;

    private:
        IDatagram::ptr m_socket;
        IListener::ptr m_listener;
        Connection::ptr m_master;
    };

    class Client : public IClient
    {
        friend class Connection;

    public:
        Client();
        ~Client();

        void Setup(IListener::ptr listener) override;

        void Connect(const Address& raddr) override;

        void Disconnect() override;

        void Tick() override;

        void Shutdown() override;

    private:
        IDatagram::ptr m_socket;
        IListener::ptr m_listener;
        Connection::ptr m_master;
    };
}

#endif

