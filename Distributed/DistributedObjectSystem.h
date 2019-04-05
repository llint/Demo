//
//  DistributedObjectSystem.h
//  Netran
//
//  Created by Lin Luo on 05/01/2015.
//

#ifndef Netran_DistributedObjectSystem_h
#define Netran_DistributedObjectSystem_h

#include "Netran.h"
#include "Serialization.h"

namespace Distributed
{
    class DataPolicyContainerWrapper
    {
    public:
        static DataPolicyContainerWrapper& Singleton()
        {
            static DataPolicyContainerWrapper s_singleton;
            return s_singleton;
        }

        operator DataPolicyContainerType&()
        {
            return m_container;
        }

    private:
        DataPolicyContainerWrapper()
        {
            m_container.Setup( DataPolicyContainerPreloadType::Singleton().Retrieve() );
        }
        
        DataPolicyContainerType m_container;
    };
    
    template <typename T>
    struct MemberFunctionTraits;

    template <typename R_, class C_, typename... As_>
    struct MemberFunctionTraits<R_ (C_::*)(As_...)>
    {
        typedef R_ R;
        typedef C_ C;
        typedef std::tuple< typename std::remove_const< typename std::remove_reference<As_>::type >::type ...> As;
    };

    template <typename R_, class C_, typename... As_>
    struct MemberFunctionTraits<R_ (C_::*)(As_...) const>
    {
        typedef R_ R;
        typedef C_ C;
        typedef std::tuple< typename std::remove_const< typename std::remove_reference<As_>::type >::type ...> As;
    };

    template <size_t... Is>
    struct Indices
    {
    };

    template <size_t I, size_t... Is>
    struct IndicesGenerator : IndicesGenerator<I - 1, I - 1, Is...>
    {
    };

    template <size_t... Is>
    struct IndicesGenerator<0, Is...>
    {
        typedef Indices<Is...> Type;
    };
    
    template <typename M, size_t N>
    struct ArgumentsSerializerHelper
    {
        static bool Apply(ISerializationType& s, typename MemberFunctionTraits<M>::As& args)
        {
            return ::Serialize( s, std::get<N - 1>(args) ) && ArgumentsSerializerHelper<M, N - 1>::Apply(s, args);
        }
    };

    template <typename M>
    struct ArgumentsSerializerHelper<M, 0>
    {
        static bool Apply(ISerializationType& s, typename MemberFunctionTraits<M>::As& args)
        {
            return true;
        }
    };

    template <typename M>
    struct ArgumentsSerializer
    {
        ISerializationType& s;
        typename MemberFunctionTraits<M>::As& args;

        ArgumentsSerializer(ISerializationType& s_, typename MemberFunctionTraits<M>::As& args_)
        : s(s_)
        , args(args_)
        {}

        bool operator()()
        {
            return ArgumentsSerializerHelper< M, std::tuple_size< typename MemberFunctionTraits<M>::As >::value >::Apply(s, args);
        }
    };

    /**
     * Remotely invoke-able methods should all return 'void', since the core RMI system is essentially asynchronous!
     */
    template < typename M, typename = typename std::enable_if< std::is_same< typename MemberFunctionTraits<M>::R, bool >::value >::type >
    struct MemberFunctionInvoker
    {
        MemberFunctionInvoker(typename MemberFunctionTraits<M>::C* const o_, M m_, const typename MemberFunctionTraits<M>::As& args_)
        : o(o_)
        , m(m_)
        , args(args_)
        {}

        typename MemberFunctionTraits<M>::C* const o;
        M m;
        const typename MemberFunctionTraits<M>::As& args;

        template <size_t... Is>
        typename MemberFunctionTraits<M>::R Invoke(Indices<Is...>)
        {
            return (o->*m)( std::get<Is>(args) ... );
        }

        typename MemberFunctionTraits<M>::R operator()()
        {
            return Invoke( typename IndicesGenerator< std::tuple_size< typename MemberFunctionTraits<M>::As >::value >::Type() );
        }
    };

    template <typename R>
    typename std::enable_if< std::is_void<R>::value >::type GenericReturn()
    {
        return;
    }

    template <typename R>
    typename std::enable_if< !std::is_void<R>::value, R >::type GenericReturn()
    {
        static const typename std::remove_reference< typename std::remove_const<R>::type >::type v{};
        return v;
    }

    template <typename M>
    static bool StaticInvoke(M m, typename MemberFunctionTraits<M>::C* o, ISerializationType& s)
    {
        typename MemberFunctionTraits<M>::As args;
        if ( ArgumentsSerializer<M>(s, args)() )
        {
            return MemberFunctionInvoker<M>(o, m, args)();
        }
        return false;
    }

    typedef U64 ObjectID;

    static const ObjectID MASTER_OBJECT = 0;

    static inline ObjectID StaticGenerateObjectID()
    {
        static ObjectID s_id = MASTER_OBJECT;

        return ++s_id != MASTER_OBJECT ? s_id : ++s_id;
    }

    static const Netran::Address GENERIC_CONNECTION = "";
    
    class IDistributedObject
    {
    public:
        typedef std::unique_ptr< IDistributedObject, NoDelete<IDistributedObject> > weak_ptr;

        IDistributedObject()
        : m_objID(MASTER_OBJECT)
        , m_connID(GENERIC_CONNECTION)
        {}

        virtual ~IDistributedObject() {}

        /**
         * Retrieves creation parameter for remote object creation
         */
        virtual bool Serialize(ISerializationType& s) = 0;

        /**
         * Binds the system generated object id to the object
         */
        void SetID(ObjectID objID)
        {
            m_objID = objID;
        }

        ObjectID GetID() const
        {
            return m_objID;
        }

        /**
         * The connection the remote method invocation is conducted (NOT meant for multithreading!)
         * otherwise, m_connID has to be made a TLS!
         */
        void SetInvokeConnection(Netran::Address connID)
        {
            m_connID = connID;
        }

        /**
         * This is the top level remote method invocation stub
         */
        virtual bool Invoke(const String& signature, ISerializationType& s) { return false; }

    protected:
        Netran::Address GetInvokeConnection() const
        {
            return m_connID;
        }

    private:
        ObjectID m_objID;
        Netran::Address m_connID;
    };

    /**
     * Common base code for both server and client (common state)
     */
    class DistributedObjectSystemBase
    {
    public:
        typedef std::reference_wrapper<DistributedObjectSystemBase> ref;

        DistributedObjectSystemBase() {}

        ~DistributedObjectSystemBase() {}

        bool ProcessInvokeMethod(Netran::Address connID, ISerializationType& s)
        {
            ObjectID objID = MASTER_OBJECT;
            SERIALIZE(s, objID);

            auto it = m_boundObjects.find(objID);
            if ( it != m_boundObjects.end() )
            {
                String signature;
                SERIALIZE(s, signature);

                // The implementation of the method would use GetInvokeConnection to retrieve the connID for use
                it->second->SetInvokeConnection(connID);
                return it->second->Invoke(signature, s);
            }

            return false;
        }

        void BindObjectBase(ObjectID objID, IDistributedObject::weak_ptr pobj)
        {
            std::cout << "DistributedObjectSystemBase::BindObjectBase: " << objID << std::endl;

            pobj->SetID(objID);
            m_boundObjects[objID] = std::move(pobj);
        }

        void UnbindObjectBase(ObjectID objID)
        {
            std::cout << "DistributedObjectSystemBase::UnbindObjectBase: " << objID << std::endl;

            m_boundObjects.erase(objID);
        }

        IDistributedObject::weak_ptr Translate(ObjectID objID)
        {
            auto it = m_boundObjects.find(objID);
            if ( it != m_boundObjects.end() )
            {
                return IDistributedObject::weak_ptr( it->second.get() );
            }
            return nullptr;
        }

    protected:
        typedef std::unordered_map<ObjectID, IDistributedObject::weak_ptr> ObjectsMap;
        const ObjectsMap& GetBoundObjects() const
        {
            return m_boundObjects;
        }

    private:
        ObjectsMap m_boundObjects; // globally bound objects
    };

    typedef U8 MessageType;

    static const MessageType MESSAGE_INVALID_TYPE = 0;

    static const MessageType MESSAGE_CREATE_OBJECT = 1;
    static const MessageType MESSAGE_DELETE_OBJECT = 2;
    static const MessageType MESSAGE_UPDATE_OBJECT = 3;
    static const MessageType MESSAGE_INVOKE_METHOD = 4;

    class DistributedObjectSystemConnection : public Netran::IConnection::IListener
    {
    public:
        DistributedObjectSystemConnection(Netran::IConnection::ptr connection, DistributedObjectSystemBase::ref owner)
        : m_connection( std::move(connection) )
        , m_owner(owner)
        {
            m_connection->Setup( Netran::IConnection::IListener::ptr(this) );
        }

        ~DistributedObjectSystemConnection() {}

        bool CreateRemoteObject(ObjectID objID, const IDistributedObject::weak_ptr& pobj)
        {
            auto it = m_spawnedObjects.find(objID);
            if ( it != m_spawnedObjects.end() )
            {
                return true;
            }

            if (pobj)
            {
                Buffer buffer;
                SerializationOutputWrapperType output( DataPolicyContainerWrapper::Singleton(), buffer );
                ISerializationType& s = output;

                MessageType msgType = MESSAGE_CREATE_OBJECT;
                SERIALIZE(s, msgType);
                SERIALIZE(s, objID);

                if ( !pobj->Serialize(s) )
                {
                    return false;
                }

                m_connection->Send(buffer, true);
            }

            m_spawnedObjects.insert(objID);

            return true;
        }

        bool DeleteRemoteObject(ObjectID objID)
        {
            auto it = m_spawnedObjects.find(objID);
            if ( it != m_spawnedObjects.end() )
            {
                m_spawnedObjects.erase(it);

                Buffer buffer;
                SerializationOutputWrapperType output( DataPolicyContainerWrapper::Singleton(), buffer );
                ISerializationType& s = output;

                MessageType msgType = MESSAGE_DELETE_OBJECT;
                SERIALIZE(s, msgType);
                SERIALIZE(s, objID);

                m_connection->Send(buffer, true);

                return true;
            }

            return false;
        }

        template <typename M>
        bool InvokeRemoteMethod(ObjectID objID, const String& signature, M m, typename MemberFunctionTraits<M>::As&& args, bool reliable)
        {
            Buffer buffer;
            SerializationOutputWrapperType output( DataPolicyContainerWrapper::Singleton(), buffer );
            ISerializationType& s = output; // TODO: why an explicit cast is needed here??

            MessageType msgType = MESSAGE_INVOKE_METHOD;
            SERIALIZE(s, msgType);
            SERIALIZE(s, objID);
            SERIALIZE(s, const_cast<String&>(signature));

            if ( !ArgumentsSerializer<M>(s, args)() )
            {
                return false;
            }

            m_connection->Send(buffer, reliable);

            return true;
        }

    protected:
        void OnIncomingData(Buffer&& buffer) override
        {
            SerializationInputWrapperType input( DataPolicyContainerWrapper::Singleton(), buffer );
            ISerializationType& s = input;
            MessageType msgType = MESSAGE_INVALID_TYPE;
            if ( !::Serialize(s, msgType) )
            {
                return;
            }

            switch (msgType)
            {
            case MESSAGE_CREATE_OBJECT:
                ProcessCreateObject(s);
                break;

            case MESSAGE_DELETE_OBJECT:
                ProcessDeleteObject(s);
                break;

            case MESSAGE_UPDATE_OBJECT:
                ProcessUpdateObject(s);
                break;

            case MESSAGE_INVOKE_METHOD:
                m_owner.get().ProcessInvokeMethod( m_connection->GetRemoteAddress(), s );
                break;

            default:
                break;
            }
        }

        virtual bool ProcessCreateObject(ISerializationType&) { return true; }
        virtual bool ProcessDeleteObject(ISerializationType&) { return true; }
        virtual bool ProcessUpdateObject(ISerializationType&) { return true; }

        Netran::IConnection::ptr m_connection;
        DistributedObjectSystemBase::ref m_owner;

        std::unordered_set<ObjectID> m_spawnedObjects;
    };

    class DistributedObjectSystemServer : public DistributedObjectSystemBase, public Netran::IServer::IListener
    {
    public:
        DistributedObjectSystemServer(const Netran::Address& addr)
        : m_server( Netran::IServer::CreateInstance() )
        {
            m_server->Setup( Netran::IServer::IListener::ptr(this) );
            m_server->Host(addr);
        }

        ~DistributedObjectSystemServer()
        {
            m_server->Shutdown();
        }

        void Tick()
        {
            m_server->Tick();
        }

        // NB:
        // - for demo purposes, no "local" objects
        // - for demo purposes, no implicit object aspect properties automatic synchronization
        ObjectID BindObject(IDistributedObject::weak_ptr pobj)
        {
            ObjectID objID = StaticGenerateObjectID();
            BindObjectBase( objID, std::move(pobj) );
            return objID;
        }

        void UnbindObject(ObjectID objID)
        {
            DeleteRemoteObject({}, true, objID);
            UnbindObjectBase(objID);
        }

        // TODO: take a list of objects?
        void CreateRemoteObject(const std::unordered_set<Netran::Address>& connIDs, bool except, ObjectID objID)
        {
            IDistributedObject::weak_ptr pobj = Translate(objID);
            if (pobj)
            {
                if (!except)
                {
                    for (auto& connID : connIDs)
                    {
                        auto it = m_connections.find(connID);
                        it->second.CreateRemoteObject(objID, pobj);
                    }
                }
                else
                {
                    for (auto& connection : m_connections)
                    {
                        if ( connIDs.find(connection.first) == connIDs.end() )
                        {
                            connection.second.CreateRemoteObject(objID, pobj);
                        }
                    }
                }
            }
        }

        void DeleteRemoteObject(const std::unordered_set<Netran::Address>& connIDs, bool except, ObjectID objID)
        {
            IDistributedObject::weak_ptr pobj = Translate(objID);
            if (pobj)
            {
                if (!except)
                {
                    for (auto& connID : connIDs)
                    {
                        auto it = m_connections.find(connID);
                        it->second.DeleteRemoteObject(objID);
                    }
                }
                else
                {
                    for (auto& connection : m_connections)
                    {
                        if ( connIDs.find(connection.first) == connIDs.end() )
                        {
                            connection.second.DeleteRemoteObject(objID);
                        }
                    }
                }
            }
        }

        // syntax:
        // All: InvokeRemoteMethod({}, true, objID, COMPOSE_SIGNATURE(MyDistributedObject, DoSomething), {arg1, arg2, arg3}, false);
        // All but one: InvokeRemoteMethod({connID}, true, objID, COMPOSE_SIGNATURE(MyDistributedObject, DoSomething), {arg1, arg2, arg3}, false);
        // One: InvokeRemoteMethod({connID}, false, objID, COMPOSE_SIGNATURE(MyDistributedObject, DoSomething), {arg1, arg2, arg3}, false);
        template <typename M>
        void InvokeRemoteMethod(const std::unordered_set<Netran::Address>& connIDs, bool except, ObjectID objID, const String& signature, M m, typename MemberFunctionTraits<M>::As&& args, bool reliable)
        {
            if (!except)
            {
                for (auto& connID : connIDs)
                {
                    auto it = m_connections.find(connID);
                    it->second.InvokeRemoteMethod(objID, signature, m, std::move(args), reliable);
                }
            }
            else
            {
                for (auto& connection : m_connections)
                {
                    if ( connIDs.find(connection.first) == connIDs.end() )
                    {
                        connection.second.InvokeRemoteMethod(objID, signature, m, std::move(args), reliable);
                    }
                }
            }
        }

    protected:
        class Connection : public DistributedObjectSystemConnection
        {
        public:
            using DistributedObjectSystemConnection::DistributedObjectSystemConnection;
        private:
        };

        virtual void OnConnectionCreated(const Netran::Address& connID) = 0;
        virtual void OnConnectionDeleted(const Netran::Address& connID) = 0;

        void OnCreateConnection(Netran::IConnection::ptr connection) override
        {
            Netran::Address connID = connection->GetRemoteAddress();

            m_connections.emplace( std::piecewise_construct, std::forward_as_tuple(connID), std::forward_as_tuple( std::move(connection), *this ) );

            OnConnectionCreated(connID);
        }

        void OnDeleteConnection(Netran::IConnection::ptr connection) override
        {
            Netran::Address connID = connection->GetRemoteAddress();

            OnConnectionDeleted(connID);

            m_connections.erase(connID);
        }
        
    private:
        Netran::IServer::ptr m_server;
        std::unordered_map<Netran::Address, Connection> m_connections;
    };

    class DistributedObjectSystemClient : public DistributedObjectSystemBase, public Netran::IClient::IListener
    {
    public:
        DistributedObjectSystemClient(const Netran::Address& addr)
        : m_client( Netran::IClient::CreateInstance() )
        {
            m_client->Setup( Netran::IClient::IListener::ptr(this) );
            m_client->Connect(addr);
        }

        ~DistributedObjectSystemClient()
        {
            m_client->Shutdown();
        }

        void OnConnectComplete(Netran::IConnection::ptr connection) override
        {
            //std::cout << "Connected to: " << connection->GetRemoteAddress() << std::endl;

            m_connection.reset( new Connection( std::move(connection), *this ) );
        }

        void OnConnectionBroken() override
        {
            m_connection = nullptr;
        }
        
        void Tick()
        {
            m_client->Tick();
        }

        bool IsConnected() const
        {
            return (bool)m_connection;
        }

        template <typename M>
        void InvokeRemoteMethod(ObjectID objID, const String& signature, M m, typename MemberFunctionTraits<M>::As&& args, bool reliable)
        {
            if (m_connection)
            {
                m_connection->InvokeRemoteMethod(objID, signature, m, std::move(args), reliable);
            }
        }

    protected:
        class Connection : public DistributedObjectSystemConnection
        {
        public:
            using DistributedObjectSystemConnection::DistributedObjectSystemConnection;

        protected:
            bool ProcessCreateObject(ISerializationType& s) override
            {
                ObjectID objID = MASTER_OBJECT;
                SERIALIZE(s, objID);
                auto pobj = static_cast<DistributedObjectSystemClient&>( m_owner.get() ).CreateObject(s);
                if (pobj)
                {
                    m_owner.get().BindObjectBase( objID, std::move(pobj) );
                    m_spawnedObjects.insert(objID);
                    return true;
                }
                return false;
            }

            bool ProcessDeleteObject(ISerializationType& s) override
            {
                ObjectID objID = MASTER_OBJECT;
                SERIALIZE(s, objID);
                m_spawnedObjects.erase(objID);
                auto pobj = m_owner.get().Translate(objID);
                if (pobj)
                {
                    static_cast<DistributedObjectSystemClient&>( m_owner.get() ).DeleteObject( std::move(pobj) );
                    //return true;
                }
                m_owner.get().UnbindObjectBase(objID);
                return true;
            }
        };

        virtual IDistributedObject::weak_ptr CreateObject(ISerializationType& s) = 0;
        virtual void DeleteObject(IDistributedObject::weak_ptr pobj) = 0;

    private:
        Netran::IClient::ptr m_client;
        std::unique_ptr<Connection> m_connection;
    };
}

#define RMI_COMPOSE_SIGNATURE(DistributedObject, method) \
    #DistributedObject "::" #method, &DistributedObject::method

#define RMI_DEFINE_SUPER(SuperClass) \
    typedef SuperClass Super

#define RMI_DECLARE_INVOKABLE(DistributedObject)                                        \
public:                                                                                 \
    template <typename M>                                                               \
    static bool StaticRegisterMethod(const String& signature, M m)                      \
    {                                                                                   \
        static_method_registry().emplace(signature,                                     \
            std::bind(Distributed::StaticInvoke<M>, m,                                  \
                std::placeholders::_1, std::placeholders::_2));                         \
        return true;                                                                    \
    }                                                                                   \
protected:                                                                              \
    virtual bool Invoke(const String& signature, ISerializationType& s) override        \
    {                                                                                   \
        auto it = static_method_registry().find(signature);                             \
        if ( it != static_method_registry().end() )                                     \
        {                                                                               \
            const Thunk& thunk = it->second;                                            \
            return thunk(this, s);                                                      \
        }                                                                               \
        return Super::Invoke(signature, s);                                             \
    }                                                                                   \
private:                                                                                \
    typedef std::function<bool (DistributedObject*, ISerializationType&)> Thunk;        \
    typedef std::unordered_map<String, Thunk> MethodsRegistry;                          \
    static MethodsRegistry& static_method_registry()                                    \
    {                                                                                   \
        static MethodsRegistry s_registry;                                              \
        return s_registry;                                                              \
    }

#define RMI_DECLARE_METHOD(method, ...) bool method(__VA_ARGS__)
#define RMI_IMPLEMENT_METHOD(DistributedObject, method, ...) bool DistributedObject::method(__VA_ARGS__)

#define RMI_REGISTER_METHOD(DistributedObject, method) \
    static bool registered_##DistributedObject_##method = DistributedObject::StaticRegisterMethod( RMI_COMPOSE_SIGNATURE(DistributedObject, method) )

#endif

