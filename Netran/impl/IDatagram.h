//
//  IDatagram.h
//  Netran
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef Netran_IDatagram_h
#define Netran_IDatagram_h

#include "Netran.h"

namespace Netran
{
    /**
     * The datagram interface
     */
    struct IDatagram
    {
        typedef std::unique_ptr<IDatagram> ptr;
        typedef std::unique_ptr< IDatagram, NoDelete<IDatagram> > weak_ptr;

        static ptr CreateInstance();

        /**
         * Initializes the datagram socket
         */
        virtual void Init(const Address& addr = "") = 0;

        /**
         * Terminates the datagram socket
         */
        virtual void Term() = 0;

        /**
         * Sends the data to the socket layer without delays
         */
        virtual void Send(const Address& addr, const Buffer& data) = 0;

        /**
         * Attempts to receive an incoming datagram, returns true if one packet is successfully received, false otherwise
         */
        virtual bool Recv(Address& addr, Buffer& data) = 0;

        virtual ~IDatagram() {}
    };
}

#endif
