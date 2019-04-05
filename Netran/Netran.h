//
//  Netran.h
//  Network Transport Layer
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef Netran_Netran_h
#define Netran_Netran_h

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <deque>
#include <chrono>
#include <map>
#include <unordered_map>

namespace Netran {

    /**
     * Timer utility
     */
    class Timer
    {
    public:
        Timer() : m_start( std::chrono::high_resolution_clock::now() ) {}

        float GetElapsedMilliseconds(bool reset = true)
        {
            float elapsed = ( std::chrono::high_resolution_clock::now() - m_start ) / std::chrono::milliseconds(1);
            if (reset)
            {
                Reset();
            }
            return elapsed;
        }

        void Reset()
        {
            m_start = std::chrono::high_resolution_clock::now();
        }

        static float Now()
        {
            return std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
        }

    private:
        std::chrono::high_resolution_clock::time_point m_start;
    };
    
    /**
     * A generic NoDelete class which doesn't do actual deletion
     * Used to weaken a unique_ptr so it won't be mistakenly deleted by the user
     */
    template <typename T>
    struct NoDelete
    {
        void operator()(T*) {}
    };
    
    typedef unsigned char Byte;
    typedef std::vector<Byte> Buffer;

    typedef std::string Address;

    /**
     * The connection interface
     */
    struct IConnection
    {
        typedef std::unique_ptr< IConnection, NoDelete<IConnection> > ptr;

        /**
         * The connection event listener, mostly for handling asynchronous incoming data
         */
        struct IListener
        {
            typedef std::unique_ptr< IListener, NoDelete<IListener> > ptr;

            virtual void OnIncomingData(Buffer&& data) = 0; // NB: the listener is supposed to take ownership of the buffer
        };

        /**
         * This method initializes the new connection with a listener
         */
        virtual void Setup(IListener::ptr listener) = 0;

        /**
         * This method shuts down the connection per user request, and appropriate connection event callback would be fired accordingly
         */
        virtual void Close() = 0;

        /**
         * This method sends the data to the other side of the connection
         */
        virtual void Send(const Buffer& data, bool reliable) = 0;

        /**
         * This method retrieves the remote address in the form of "<ipaddr>:<port>"
         */
        virtual const Address& GetRemoteAddress() const = 0;

        /**
         * Get Round Trip Time, in milliseconds
         */
        virtual float GetRTT() const = 0;

        /**
         * Get the bandwidth, in bytes per second
         */
        virtual float GetBandwidth() const = 0;

        virtual ~IConnection() {}
    };

    /**
     * The server interface
     */
    struct IServer
    {
        typedef std::unique_ptr<IServer> ptr;

        /**
         * This static method creates a new instance of the server within the process
         */
        static ptr CreateInstance();

        /**
         * The connection event listener, mostly for handling connection creation and deletion events
         */
        struct IListener
        {
            typedef std::unique_ptr< IListener, NoDelete<IListener> > ptr;

            virtual void OnCreateConnection(IConnection::ptr connection) = 0;
            virtual void OnDeleteConnection(IConnection::ptr connection) = 0;
        };
        
        /**
         * Sets up the connection listener for the server instance
         */
        virtual void Setup(IListener::ptr listener) = 0;

        /**
         * Starts the server up and binds it to a local address in the form of "<ipaddr>:<port>"
         * If successful, the server is listening and accepting incoming connections
         * Returns true when the startup is successful; false otherwise
         */
        virtual void Host(const Address& local) = 0;

        /**
         * Kicks the connection by remote address
         */
        virtual void Kick(const Address& raddr) = 0;

        /**
         * Ticks the server instance from the main thread; user level event callbacks are fired inside here
         */
        virtual void Tick() = 0;

        /**
         * Shuts down the server instance
         * All the open connections are shutdown as well, with each of them receiving their corresponding connection deletion callback to cleanup application level resources
         */
        virtual void Shutdown() = 0;

        virtual ~IServer() {}
    };

    /**
     * The client interface
     */
    struct IClient
    {
        typedef std::unique_ptr<IClient> ptr;

        /**
         * The static method creates a new instance of the client within the process
         */
        static ptr CreateInstance();

        /**
         * The connection event listener
         */
        struct IListener
        {
            typedef std::unique_ptr< IListener, NoDelete<IListener> > ptr;

            /**
             * This method is called when a connection attempt is completed (either success or failure)
             * on successful connection, connection is a valid pointer to the new connection; NULL otherwise
             */
            virtual void OnConnectComplete(IConnection::ptr connection) = 0;

            /**
             * This method is called when the connection to server is broken without local disconnect
             */
            virtual void OnConnectionBroken() = 0;
        };
        
        /**
         * Sets up the client instance by supplying a connection listener
         */
        virtual void Setup(IListener::ptr listener) = 0;

        /**
         * Attempts to open a connection to a remote address; if any previous attempts is still in progress, this function fails
         */
        virtual void Connect(const Address& remote) = 0;

        /**
         * Shuts down the client; any in progress connection attempt is cancelled; any established connection is shutdown and appropriate connection callback should be fired
         * For established connection, IConnection::shudown is not required to put the client to a shut down state, since the connection itself has no knowledge about its environment
         */
        virtual void Disconnect() = 0;

        /**
         * Ticks the client instance from the main thread, so connection event would be fired appropriately
         */
        virtual void Tick() = 0;

        /**
         * Shuts down the client, closes the underlying socket
         */
        virtual void Shutdown() = 0;
        
        virtual ~IClient() {}
    };

}

#endif
