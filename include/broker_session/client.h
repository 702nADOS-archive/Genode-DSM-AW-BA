/*
 * \brief  Client-side interface of the Broker service
 * \author Alexander Weidinger
 * \date   2015-12-20
 */

#ifndef _INCLUDE__BROKER_SESSION__CLIENT_H
#define _INCLUDE__BROKER_SESSION__CLIENT_H

#include <broker_session/broker_session.h>
#include <base/rpc_client.h>
#include <dataspace/capability.h>

namespace Broker {

    struct Session_client : Genode::Rpc_client<Session>
    {
        Session_client(Genode::Capability<Session> cap)
        : Genode::Rpc_client<Session>(cap) { }

        Genode::Dataspace_capability getRemoteDataspaceCapability()
        {
            return call<Rpc_getRemoteDataspaceCapability>();
        }

	Genode::Dataspace_capability getLocalDataspaceCapability()
	{
	    return call<Rpc_getLocalDataspaceCapability>();
	}
    };
}

#endif /* _INCLUDE__BROKER_SESSION__CLIENT_H_ */
