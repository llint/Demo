//
//  GameEngineSystem.cpp
//  Netran
//
//  Created by Lin Luo on 05/01/2015.
//

#include <iostream>
#include "GameEngineSystem.h"

RMI_REGISTER_METHOD(Entity, UpdatePhysics);
RMI_REGISTER_METHOD(Entity, SetAutonomous);
RMI_REGISTER_METHOD(Entity, Test);

RMI_IMPLEMENT_METHOD(Entity, Test)
{
    return true;
}

RMI_IMPLEMENT_METHOD(Entity, SetAutonomous, bool autonomous)
{
    m_auto = autonomous;
    m_engine.SetAutonomousEntity( Entity::weak_ptr(this) );
    return true;
}

RMI_IMPLEMENT_METHOD(Entity, UpdatePhysics, const Vec3& pos, double yaw, U64 timestamp)
{
    // TODO: interpolation based on history and latency
    m_pos = pos;
    m_yaw = yaw;
    const auto& server = m_engine.ServerCast();
    if (server)
    {
        server->InvokeRemoteMethod( {GetInvokeConnection()}, true, GetID(), RMI_COMPOSE_SIGNATURE(Entity, UpdatePhysics), {pos, yaw, 0}, false );
    }
    return true;
}

RMI_REGISTER_METHOD(MasterObject, ClientRequestLogin);
RMI_REGISTER_METHOD(MasterObject, ServerSetupDone);
RMI_REGISTER_METHOD(MasterObject, KeepAlive);

void Engine::Tick()
{
    if (m_server)
    {
        m_server->Tick();
    }

    if (m_client)
    {
        m_client->Tick();
    }

    // Tick the entities
}

