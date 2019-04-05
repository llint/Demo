//
//  GameEngineSystem.h
//  Netran
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef Netran_GameEngineSystem_h
#define Netran_GameEngineSystem_h

#include "DistributedObjectSystem.h"

struct Vec3
{
    double x, y, z;

    Vec3(double x_ = 0.0, double y_ = 0.0, double z_ = 0.0) : x(x_), y(y_), z(z_) {}
    Vec3(const Vec3& rhs) : x(rhs.x), y(rhs.y), z(rhs.z) {}

    Vec3& operator=(const Vec3& rhs)
    {
        x = rhs.x;
        y = rhs.y;
        z = rhs.z;
        return *this;
    }

    bool Serialize(ISerializationType& s)
    {
        SERIALIZE(s, x);
        SERIALIZE(s, y);
        SERIALIZE(s, z);
        return true;
    }
};

class Engine;

// the entity class contains the position and rotation of entities as well as other graphical representations
// NB: a serious entity system should probably take advantage of a component based architecture
class Entity : public Distributed::IDistributedObject
{
    RMI_DEFINE_SUPER(Distributed::IDistributedObject);
    RMI_DECLARE_INVOKABLE(Entity);

public:
    typedef std::unique_ptr<Entity> ptr;
    typedef std::unique_ptr< Entity, NoDelete<Entity> > weak_ptr;

    Entity(Engine& engine, const Vec3& pos = Vec3(), double yaw = 0.0) : m_engine(engine), m_pos(pos), m_yaw(yaw), m_auto(false) {}

    bool Serialize(ISerializationType& s) override
    {
        SERIALIZE(s, m_pos);
        SERIALIZE(s, m_yaw);
        return true;
    }

    RMI_DECLARE_METHOD(UpdatePhysics, const Vec3& pos, double yaw, U64 timestamp);

    RMI_DECLARE_METHOD(SetAutonomous, bool autonomous);

    RMI_DECLARE_METHOD(Test);

    const Vec3& GetPosition() const
    {
        return m_pos;
    }

    double GetRotation() const
    {
        return m_yaw;
    }

    void SetPhysics(const Vec3& pos, double yaw)
    {
        m_pos = pos;
        m_yaw = yaw;
    }

    void Tick()
    {
        // TODO: based on the physics history states, interpolate the current state based on the currentTime - interpolationDelay
    }

private:
    Engine& m_engine;

    Vec3 m_pos;
    double m_yaw;

    bool m_auto;

    std::map< U64, std::pair<Vec3, Vec3> > m_history; // NB: for movement interpolation U64 is the smoothed timestamp when the state is received (locally)
    // smoothed timestamp is calculated based on the actual receive timestamp - smoothed network latency (TBD)
};

class ServerEngine;
class ClientEngine;

typedef std::unique_ptr< ServerEngine, NoDelete<ServerEngine> > ServerEnginePtr;
typedef std::unique_ptr< ClientEngine, NoDelete<ClientEngine> > ClientEnginePtr;

// the engine class manages a list of entities (server and client)
// the engine class renders the entities based on their current positions and rotations
class Engine
{
public:
    Engine() {}
    ~Engine() {}

    void SetServer(ServerEnginePtr server)
    {
        m_server = std::move(server);
    }

    void SetClient(ClientEnginePtr client)
    {
        m_client = std::move(client);
    }

    const ServerEnginePtr& ServerCast() const { return m_server; }
    const ClientEnginePtr& ClientCast() const { return m_client; }

    Entity::weak_ptr CreateEntity( const Vec3& pos = Vec3(), double yaw = 0.0 )
    {
        Entity::ptr entity( new Entity(*this, pos, yaw) );
        Entity::weak_ptr entityRef( entity.get() );
        m_entities.emplace( Entity::weak_ptr( entity.get() ), std::move(entity) );
        OnEntityCreated( Entity::weak_ptr( entityRef.get() ) );
        return entityRef;
    }

    Entity::weak_ptr CreateEntity(ISerializationType& s)
    {
        Entity::ptr entity( new Entity(*this) );
        if ( entity->Serialize(s) )
        {
            Entity::weak_ptr entityRef( entity.get() );
            m_entities.emplace( Entity::weak_ptr( entity.get() ), std::move(entity) );
            OnEntityCreated( Entity::weak_ptr( entityRef.get() ) );
            return entityRef;
        }
        return nullptr;
    }

    void DeleteEntity(Entity::weak_ptr entity)
    {
        OnEntityDeleted( Entity::weak_ptr( entity.get() ) );

        if (m_autonomousEntity == entity)
        {
            m_autonomousEntity = nullptr;
        }
        m_entities.erase(entity);
    }

    void SetAutonomousEntity(Entity::weak_ptr entity)
    {
        if ( m_entities.find(entity) != m_entities.end() )
        {
            m_autonomousEntity = std::move(entity);
        }
    }

    Entity::weak_ptr GetAutonomousEntity() const
    {
        return Entity::weak_ptr( m_autonomousEntity.get() );
    }

    void Tick();

protected:
    virtual void OnEntityCreated(Entity::weak_ptr entity) = 0;
    virtual void OnEntityDeleted(Entity::weak_ptr entity) = 0;

private:
    std::unordered_map<Entity::weak_ptr, Entity::ptr> m_entities;
    Entity::weak_ptr m_autonomousEntity;

    ServerEnginePtr m_server;
    ClientEnginePtr m_client;
};

// the master object that intercepts the highest level RPC requests and forwards it to the actual server/client backend
// The master object exists both on the client and sserver when they start up, and always should have the same ID 0 (wip)
class MasterObject : public Distributed::IDistributedObject
{
    RMI_DEFINE_SUPER(Distributed::IDistributedObject);
    RMI_DECLARE_INVOKABLE(MasterObject);

public:
    MasterObject() {}
    ~MasterObject() {}

    bool Serialize(ISerializationType& s) override
    {
        return true;
    }

    RMI_DECLARE_METHOD(ClientRequestLogin, const String& credential)
    {
        return true;
    }

    RMI_DECLARE_METHOD(ServerSetupDone)
    {
        return true;
    }

    RMI_DECLARE_METHOD(KeepAlive)
    {
        return true;
    }

private:
    std::unique_ptr< ServerEngine, NoDelete<ServerEngine> > m_server;
    std::unique_ptr< ClientEngine, NoDelete<ClientEngine> > m_client;
};

// ServerEngine and ClientEngine manage entity creation into engine based on a series of network events (player entity creation and distribution - no "aperture")
class ServerEngine : public Distributed::DistributedObjectSystemServer
{
public:
    ServerEngine(Engine& engine, const Netran::Address& addr)
    : Distributed::DistributedObjectSystemServer(addr)
    , m_engine(engine)
    {
        m_engine.SetServer( ServerEnginePtr(this) );
        BindObjectBase( Distributed::MASTER_OBJECT, Distributed::IDistributedObject::weak_ptr(&m_master) ); // INVALID_OBJECT is used for MasterObject ID
    }

    void Tick()
    {
        Distributed::DistributedObjectSystemServer::Tick();

        // TODO: collect the current snapshot of the entities (pos/rot) and invoke the RPC (UpdatePhysics) of each entity on all the connections
        // for now, the server just relays the UpdatePhysics call to all the other connections inside Entity::UpdatePhysics

        // Keep alive ...
        // since this RMI is reliable, any dead connection would timeout - we don't need the clients to send us back anything!
        if ( m_keepAliveTimer.GetElapsedMilliseconds(false) > 1000.f )
        {
            InvokeRemoteMethod({}, true, Distributed::MASTER_OBJECT, RMI_COMPOSE_SIGNATURE(MasterObject, KeepAlive), {}, true);
            m_keepAliveTimer.Reset();
        }
    }

    ~ServerEngine()
    {}

private:
    void OnConnectionCreated(const Netran::Address& connID) override
    {
        std::cout << "Created remote connection: " << connID << std::endl;

        // create the entity in the engine
        // bind the object to the server
        // spawn this entity to all the existing connections remotely
        // spawn all the *other* server bound entities to the new connection
        Entity::weak_ptr entity = m_engine.CreateEntity();
        Distributed::ObjectID objID = BindObject(Distributed::IDistributedObject::weak_ptr(entity.get()));
        CreateRemoteObject({}, true, objID); // NB: "except nothing" means "all"!
        for ( const auto& each : GetBoundObjects() )
        {
            if (each.first != objID)
            {
                CreateRemoteObject({connID}, false, each.first);
            }
        }

        // mark as autonomous locally
        m_connections[connID].autonomousObjects.insert(objID);

        // mark as autonomous remotely (reliable messages are always ordered, so this RMI should happen *after* the entity is created on the client side)
        InvokeRemoteMethod({connID}, false, objID, RMI_COMPOSE_SIGNATURE(Entity, SetAutonomous), {true}, true);
    }

    void OnConnectionDeleted(const Netran::Address& connID) override
    {
        // unbind the connection's autonomous entity on the server
        // which implicitly deletes the entity to all the other connections

        auto it = m_connections.find(connID);
        if ( it != m_connections.end() )
        {
            for (const auto& objID : it->second.autonomousObjects)
            {
                auto pobj = Translate(objID);
                UnbindObject(objID);
                m_engine.DeleteEntity( Entity::weak_ptr( static_cast<Entity*>( pobj.get() ) ) );
            }
        }

        m_connections.erase(connID);

        std::cout << "Removed remote connection: " << connID << std::endl;
    }

    struct ConnectionInfo
    {
        // objects that are autonomously controlled by the connection (so the server won't try to sync the state of those entities to the client
        std::unordered_set<Distributed::ObjectID> autonomousObjects;

        ConnectionInfo() {}
    };

    std::unordered_map<Netran::Address, ConnectionInfo> m_connections;

    Engine& m_engine;
    MasterObject m_master;

    Netran::Timer m_keepAliveTimer;
};

class ClientEngine : public Distributed::DistributedObjectSystemClient
{
public:
    ClientEngine(Engine& engine, const Netran::Address& addr)
    : Distributed::DistributedObjectSystemClient(addr)
    , m_engine(engine)
    {
        m_engine.SetClient( ClientEnginePtr(this) );
        BindObjectBase( Distributed::MASTER_OBJECT, Distributed::IDistributedObject::weak_ptr(&m_master) ); // INVALID_OBJECT is used for MasterObject ID
    }

    void Tick()
    {
        Distributed::DistributedObjectSystemClient::Tick();
        
        // Send updated autonomous entity state to the server - UpdatePhysics: client -> server
        // Based on user input, update the state of autonomous entity
        auto autonomous = m_engine.GetAutonomousEntity();
        if (autonomous)
        {
            InvokeRemoteMethod( autonomous->GetID(), RMI_COMPOSE_SIGNATURE(Entity, UpdatePhysics), { autonomous->GetPosition(), autonomous->GetRotation(), 0}, false );
        }
        // Render the entities at their interpolated position
    }

protected:
    Distributed::IDistributedObject::weak_ptr CreateObject(ISerializationType& s) override
    {
        Entity::weak_ptr entity = m_engine.CreateEntity(s);
        return Distributed::IDistributedObject::weak_ptr( entity.get() );
    }

    void DeleteObject(Distributed::IDistributedObject::weak_ptr pobj) override
    {
        m_engine.DeleteEntity( Entity::weak_ptr( static_cast<Entity*>( pobj.get() ) ) );
    }

private:
    Engine& m_engine;
    MasterObject m_master;
};

#endif
