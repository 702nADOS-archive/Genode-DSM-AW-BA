/*
 * \brief  Interface definition of the Broker service
 * \author Alexander Weidinger
 * \date   2015-12-20
 */

#ifndef _INCLUDE__BROKER_SESSION__BROKER_SESSION_H_
#define _INCLUDE__BROKER_SESSION__BROKER_SESSION_H_

#include <session/session.h>
#include <base/rpc.h>
#include <rm_session/capability.h>

namespace Broker {

    struct Session : Genode::Session
    {
        static const char *service_name() { return "Broker"; };

        virtual Genode::Dataspace_capability getRemoteDataspaceCapability() = 0;
	virtual Genode::Dataspace_capability getLocalDataspaceCapability() = 0;

        /*******************
         ** RPC interface **
         *******************/

        GENODE_RPC(Rpc_getRemoteDataspaceCapability, Genode::Dataspace_capability, getRemoteDataspaceCapability);
	GENODE_RPC(Rpc_getLocalDataspaceCapability, Genode::Dataspace_capability, getLocalDataspaceCapability);

        GENODE_RPC_INTERFACE(Rpc_getRemoteDataspaceCapability, Rpc_getLocalDataspaceCapability);
    };
}

#endif /* _INCLUDE__BROKER_SESSION__BROKER_SESSION_H_ */
