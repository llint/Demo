//----------------------------------------------------------------------------//
// main.cpp                                                                   //
// Copyright (C) 2001 Bruno 'Beosil' Heidelberger                             //
//----------------------------------------------------------------------------//
// This program is free software; you can redistribute it and/or modify it    //
// under the terms of the GNU General Public License as published by the Free //
// Software Foundation; either version 2 of the License, or (at your option)  //
// any later version.                                                         //
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// Includes                                                                   //
//----------------------------------------------------------------------------//

#if defined(_MSC_VER) && _MSC_VER <= 0x0600
#pragma warning(disable : 4786)
#endif

#include "demo.h"

//----------------------------------------------------------------------------//
// GLUT callback functions                                                    //
//----------------------------------------------------------------------------//

void displayFunc()
{
  // render the scene
  theDemo.onRender();
}

void exitFunc()
{
  // shut the demo instance down
  theDemo.onShutdown();
}

void idleFunc()
{
  // redirect to the demo instance
  theDemo.onIdle();
}

void keyboardFunc(unsigned char key, int x, int y)
{
  // redirect the message to the demo instance
  theDemo.onKey(key, x, theDemo.getHeight() - y - 1);
}

void motionFunc(int x, int y)
{
  // redirect the message to the demo instance
  theDemo.onMouseMove(x, theDemo.getHeight() - y - 1);
}

void mouseFunc(int button, int state, int x, int y)
{
  // redirect the message to the demo instance
  if(state == GLUT_DOWN)
  {
    theDemo.onMouseButtonDown(button, x, theDemo.getHeight() - y - 1);
  }
  else if(state == GLUT_UP)
  {
    theDemo.onMouseButtonUp(button, x, theDemo.getHeight() - y - 1);
  }
}

void reshapeFunc(int width, int height)
{
  // set the new width/height values
  theDemo.setDimension(width, height);
}

//----------------------------------------------------------------------------//
// Main entry point of the application                                        //
//----------------------------------------------------------------------------//

int main(int argc, char *argv[])
{
  // initialize the GLUT system
  glutInit(&argc, argv);

  // create our demo instance
//  if(!theDemo.onCreate(argc, argv))
//  {
//    std::cerr << "Creation of the demo failed." << std::endl;
//    return -1;
//  }

  // register our own exit callback
  atexit(exitFunc);

  // set all GLUT modes
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(theDemo.getWidth(), theDemo.getHeight());
  glutCreateWindow(theDemo.getCaption().c_str());
  if(theDemo.getFullscreen()) glutFullScreen();
  glutSetCursor(GLUT_CURSOR_NONE);

  // register all GLUT callback functions
  glutIdleFunc(idleFunc);
  glutMouseFunc(mouseFunc);
  glutMotionFunc(motionFunc);
  glutPassiveMotionFunc(motionFunc);
  glutReshapeFunc(reshapeFunc);
  glutDisplayFunc(displayFunc);
  glutKeyboardFunc(keyboardFunc);

  // initialize our demo instance
  if(!theDemo.onInit())
  {
    std::cerr << "Initialization of the demo failed." << std::endl;
    return -1;
  }

    std::unique_ptr<ServerEngine> server;
    std::unique_ptr<ClientEngine> client;

    if (argc > 2)
    {
        std::string type = argv[1];
        std::string addr = argv[2];

        if (type == "-s")
        {
            server.reset( new ServerEngine(theDemo, addr) );
        }
        else if (type == "-c")
        {
            client.reset( new ClientEngine(theDemo, addr) );
        }
    }

  // run the GLUT message loop
  glutMainLoop();

  return 0;
}

//----------------------------------------------------------------------------//


#include <unistd.h>
#include <iostream>

// NB: The reason Xcode can locate the header files without explicitly setting the search path
// is because Netran, Distribution, and Serialization are included in the project
#include "Netran.h"
#include "GameEngineSystem.h"

using namespace Netran;

class Server : public IServer::IListener
{
public:
    Server()
    : m_server( IServer::CreateInstance() )
    {
        m_server->Setup( IServer::IListener::ptr(this) );
        m_server->Host("127.0.0.1:8888");
    }

    ~Server()
    {
        m_server->Shutdown();
    }

    void Tick()
    {
        m_server->Tick();
    }

    void OnCreateConnection(IConnection::ptr connection) override
    {
        std::cout << "Created remote connection: " << connection->GetRemoteAddress() << std::endl;

        m_connections.emplace( connection->GetRemoteAddress(), std::move(connection) );
    }

    void OnDeleteConnection(IConnection::ptr connection) override
    {
        m_connections.erase( connection->GetRemoteAddress() );

        std::cout << "Removed remote connection: " << connection->GetRemoteAddress() << std::endl;
    }

private:
    class Connection : public IConnection::IListener
    {
    public:
        Connection(IConnection::ptr connection)
        : m_connection( std::move(connection) )
        , m_count(0)
        {
            m_connection->Setup( IConnection::IListener::ptr(this) );
        }

        void OnIncomingData(Buffer&& data) override
        {
            std::cout << "[" << m_connection->GetRemoteAddress() << "](" << ++m_count << "): " << (char*)data.data() << std::endl;

            m_connection->Send(data, true);
        }

    private:
        IConnection::ptr m_connection;
        size_t m_count;
    };

    IServer::ptr m_server;

    typedef std::unordered_map<Address, Connection> ConnectionsMap;
    ConnectionsMap m_connections;
};

class Client : public IClient::IListener
{
public:
    Client()
    : m_client( IClient::CreateInstance() )
    {
        m_client->Setup( IClient::IListener::ptr(this) );
        m_client->Connect("127.0.0.1:8888");
    }

    void OnConnectComplete(IConnection::ptr connection) override
    {
        std::cout << "Connected to: " << connection->GetRemoteAddress() << std::endl;

        m_connection.reset( new Connection( std::move(connection) ) );
    }

    void OnConnectionBroken() override
    {
        m_connection = nullptr;
    }

    void Tick()
    {
        m_client->Tick();
    }

private:
    class Connection : public IConnection::IListener
    {
    public:
        typedef std::unique_ptr<Connection> ptr;

        Connection(IConnection::ptr connection)
        : m_connection( std::move(connection) )
        , m_count(0)
        {
            m_connection->Setup( IConnection::IListener::ptr(this) );
            static const Byte s[] = "hello world";
            m_connection->Send( Buffer(s, s+sizeof(s)), true );
        }

        void OnIncomingData(Buffer&& data) override
        {
            std::cout << "[" << m_connection->GetRemoteAddress() << "](" << ++m_count << "): " << (char*)data.data() << std::endl;

            m_connection->Send(data, true);
        }

    private:
        IConnection::ptr m_connection;
        size_t m_count;
    };

    IClient::ptr m_client;
    Connection::ptr m_connection;
};

void Test0()
{
    Server server;
    Client client;

    while (1)
    {
        server.Tick();
        client.Tick();
        usleep(1000);
    }
}

struct TestEngine : public Engine
{
    void OnEntityCreated(Entity::weak_ptr entity) override {}
    void OnEntityDeleted(Entity::weak_ptr entity) override {}
};

void Test1()
{
    TestEngine se;
    TestEngine ce1;
    TestEngine ce2;

    ServerEngine server(se, "127.0.0.1:8888");
    ClientEngine client1(ce1, "127.0.0.1:8888");
    ClientEngine client2(ce2, "127.0.0.1:8888");

    while (true)
    {
        server.Tick();
        client1.Tick();
        client2.Tick();
        usleep(1000);
    }
}

void Test2(const std::string& type, const std::string& addr)
{
    if (type == "-s")
    {
        TestEngine engine;
        ServerEngine server(engine, addr);

        while (true)
        {
            server.Tick();
            usleep(1000);
        }
    }
    else if (type == "-c")
    {
        TestEngine engine;
        ClientEngine client(engine, addr);

        while (true)
        {
            client.Tick();
            usleep(1000);
        }
    }
}

int mainX(int argc, const char * argv[])
{
    if (argc > 2)
    {
        Test2(argv[1], argv[2]);
    }
    else
    {
        Test1();
    }
    
    std::cout << "Hello, World!\n";
    return 0;
}

