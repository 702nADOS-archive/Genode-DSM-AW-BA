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
    // memory on node 1
    // ---

    /*
     * write test
     */
    PDBG("Writing to local shared memory!");
    strcpy(ptr_node1, "Hello Node1!");
    PDBG("Wrote \"Hello Node1!\" to %p!\n", ptr_node1);

    /*
     * read test
     */
    PDBG("Reading from local shared memory!");
    printf("Read \"%s\" from %p!\n", ptr_node1, ptr_node1);

    // ---
    // memory on node 2
    // ---
    
    /*
     * write test
     */
    PDBG("Writing to remote shared memory!");
    strcpy(ptr_node2, "Hello Node2!");
    printf("Wrote \"Hello Node2!\" to %p!\n", ptr_node2);

    /*
     * wait a bit, to give broker enough time
     * to detach the "cache" dataspace capability
     */
    int i=0;
    while(i < 10000000) {
        __asm__("NOP");
        i++;
    }

    /*
     * read test
     */
    PDBG("Reading from remote shared memory!");
    printf("Read \"%s\" from %p!\n", ptr_node2, ptr_node2);

    return 0;
}
