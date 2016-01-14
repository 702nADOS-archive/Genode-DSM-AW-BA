/*
 * \brief  Connection to Broker service
 * \author Alexander Weidinger
 * \date   2015-12-20
 */

#ifndef _INCLUDE__BROKER_SESSION__CONNECTION_H_
#define _INCLUDE__BROKER_SESSION__CONNECTION_H_

#include <broker_session/client.h>
#include <base/connection.h>

namespace Broker {

    struct Connection : Genode::Connection<Session>, Session_client
    {
        Connection()
        :
            /* create session */
            Genode::Connection<Broker::Session>(session("Broker, ram_quota=4K")),

            /* initialize RPC interface */
            Session_client(cap()) { }
    };
}

#endif /* _INCLUDE__BROKER_SESSION__CONNECTION_H_ */
