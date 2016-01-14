/*
 * \brief  Dummy application using the Broker service and therefore DSM
 * \author Alexander Weidinger
 * \date   2015-12-20
 */

// base includes
#include <base/printf.h>
#include <base/env.h>
#include <string.h>

// broker service includes
#include <broker_session/client.h>
#include <broker_session/connection.h>
#include <rm_session/connection.h>

using namespace Genode;

int main(int argc, char *argv[]) {
    // establish connection to broker
    Broker::Connection bc;

    /*
     * get local shared memory capability
     * and include into rm session
     */
    Dataspace_capability dsc = bc.getLocalDataspaceCapability();
    char *ptr_node1 = env()->rm_session()->attach(dsc);

    /*
     * get remote shared memory capability
     * and include into rm session
     */
    dsc = bc.getRemoteDataspaceCapability();
    char *ptr_node2 = env()->rm_session()->attach(dsc);

    // ---
    // memory on node 2
    // ---

    while(1) {
        // wait some time...
	int i=0;
	while(i < 100000000) {
            __asm__("NOP");
	    i++;
	}
        // and print content of local shared memory at 0x0
        printf("%20s\n", ptr_node1);
    }

    return 0;
}
