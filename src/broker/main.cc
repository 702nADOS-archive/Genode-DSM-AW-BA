/*
 * \brief  Responsible for resolving page faults of DSM
 * \author Alexander Weidinger
 * \date   2015-12-20
 */

// base includes
#include <base/printf.h>
#include <base/env.h>
#include <rm_session/connection.h>

// config
#include <os/config.h>

// lwip includes
extern "C" {
#include <lwip/sockets.h>
#include <lwip/api.h>
}
#include <lwip/genode.h>
#include <nic/packet_allocator.h>

// Broker service includes
#include <cap_session/connection.h>
#include <root/component.h>
#include <broker_session/broker_session.h>
#include <base/rpc_server.h>

// "config" in broker.h
#include <broker.h>

// quick fix for passing ip addresses
char local_ip[16];
char remote_ip[16];
// ---

using namespace Genode;

void setRemotePage(char *ip, addr_t offset);

/*
 * connects to node 2,
 * sends read request and page offset
 * creates dataspace capability with the requested page as content
 * returns the dataspace capability
 */
Dataspace_capability getRemotePage(char *ip, addr_t offset) {
    // "cache" memory page
    Dataspace_capability dsc = env()->ram_session()->alloc(PAGE_SIZE);
    // include page into own rm session
    char *ptr = env()->rm_session()->attach(dsc);

    // create socket
    int s;
    if((s = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        PERR("No socket available!");
    }

    struct sockaddr_in addr;
    addr.sin_port = htons(2000);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);

    // connect to node 2
    if(lwip_connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        PERR("Could not connect!");
        lwip_close(s);
    }

    // send read request
    char write = 0;
    unsigned long bytes = lwip_send(s, &write, 1, 0);
    if(bytes < 0) {
        PERR("Couldn't send read request!");
        lwip_close(s);
    }

    // send offset
    bytes = lwip_send(s, (char *)&offset, 4, 0);
    if(bytes < 0) {
        PERR("Couldn't send offset!");
        lwip_close(s);
    }

    // receive page
    char buf[PAGE_SIZE];
    ssize_t buflen;
    buflen = lwip_recv(s, buf, PAGE_SIZE, 0);
    if(buflen < 0) {
        PERR("Couldn't receive page!");
        lwip_close(s);
    }

    // copy page into cache
    Genode::memcpy(ptr, buf, PAGE_SIZE);

    // remove page from own rm session
    env()->rm_session()->detach(ptr);
    // return cache page cap
    return dsc;
}

/* dsm stuff */
namespace DSM {
    class Pager : public Thread<8192>
    {
        private:
            Signal_receiver _receiver;

        public:
            Pager(): Thread("Pager") { }
            Signal_receiver *signal_receiver() { return &_receiver; }

            void entry();
    };

    class external_memory : public Signal_context
    {
        private:
            /* managed dataspace representing remote memory */
            Rm_connection _rm;
            size_t _size;

        public:
            /* creates a new RM connection with the given size */
            external_memory(size_t size): _rm(0, size) {
                _size = size;
            }

            virtual ~external_memory() {
            }

            /*
             * handle page faults
             */
            void handle_fault()
            {
                Rm_session::State state = _rm.state();

                PDBG("DSM::external_memory:: rm session state is %s, pf_addr=0x%lx\n",
                        state.type == Rm_session::READ_FAULT ? "READ_FAULT"   :
                        state.type == Rm_session::WRITE_FAULT ? "WRITE_FAULT" :
                        state.type == Rm_session::EXEC_FAULT ? "EXEC_FAULT"   :
                        "READY",
                        state.addr);

                if (state.type == Rm_session::READY)
                    return;

                /* handle read fault */
                if (state.type == Rm_session::READ_FAULT) {
                    _rm.attach_at(getRemotePage(remote_ip, state.addr),
                            state.addr);
                }
                /* handle write fault */
                if (state.type == Rm_session::WRITE_FAULT) {
                    setRemotePage(remote_ip, state.addr);
                }
            }

            Rm_connection *rm() { return &_rm; }
            Dataspace_capability ds() { return _rm.dataspace(); }

            void connect_pager(Pager& _pager) {
                _rm.fault_handler(_pager.signal_receiver()->manage(this));
            }
    };

    void Pager::entry() {
        PDBG("Pager thread started.");

        while(true) {
            try {
                /* wait for page fault signal */
                Signal signal = _receiver.wait_for_signal();

		/* Genode magic? */
                for(int i = 0; i < signal.num(); i++) {
                    static_cast<external_memory *>(signal.context())->handle_fault();
                }
            } catch(Exception &e) {
                PERR("Unknown error occurred!");
            }
        }
    }
};

/*
 * create external memory for node 2
 */
enum { MANAGED_DS_SIZE = SM_SIZE };
DSM::external_memory em(MANAGED_DS_SIZE);

/*
 * create local shared memory for node 1
 */
Dataspace_capability lm = env()->ram_session()->alloc(SM_SIZE);
char *ptr = env()->rm_session()->attach(lm);

/*
 * connects to node 2,
 * sends write request, page offset and page
 */
void setRemotePage(char *ip, addr_t offset) {
    // "cache" memory page
    Dataspace_capability dsc = env()->ram_session()->alloc(PAGE_SIZE);
    em.rm()->attach_at(dsc, offset);

    // TODO determine if write was done in a clean way
    char *ptr = env()->rm_session()->attach(dsc);
    char testString[] = "Hello Node2!";
    while(Genode::strcmp(ptr, testString) != 0) { PDBG(">> Waiting for Node 1 to finish writing!"); }
    PDBG("Dummy is done writing!");
    em.rm()->detach(offset);

    // create socket
    int s;
    if((s = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        PERR("No socket available!");
    }

    struct sockaddr_in addr;
    addr.sin_port = htons(2000);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);

    // connect to node 2
    if(lwip_connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        PERR("Could not connect!");
        lwip_close(s);
    }

    // send write request
    char write = 1;
    unsigned long bytes = lwip_send(s, &write, 1, 0);
    if(bytes < 0) {
        PERR("Couldn't send write request!");
        lwip_close(s);
    }

    // send offset
    bytes = lwip_send(s, (char *)&offset, 4, 0);
    if(bytes < 0) {
        PERR("Couldn't send offset!");
        lwip_close(s);
    }

    // send page
    bytes = lwip_send(s, ptr, PAGE_SIZE, 0);
    if (bytes < 0) {
        PERR("Couldn't send page!");
        lwip_close(s);
    }

    // remove page from own rm session
    env()->rm_session()->detach(ptr);
}

/* broker service
 * according to the hello server/client tutorial
 */
namespace Broker {

    struct Session_component : Genode::Rpc_object<Session>
    {
        Genode::Dataspace_capability getRemoteDataspaceCapability() {
            return em.ds();
        }
        Genode::Dataspace_capability getLocalDataspaceCapability() {
            return lm;
        }
    };

    class Root_component : public Genode::Root_component<Session_component>
    {
        protected:

            Broker::Session_component *_create_session(const char *args)
            {
                PDBG("creating broker session.");
                return new (md_alloc()) Session_component();
            }

        public:

            Root_component(Genode::Rpc_entrypoint *ep,
                    Genode::Allocator *allocator)
                : Genode::Root_component<Session_component>(ep, allocator)
            {
                PDBG("creating root component.");
            }
    };
}

int main(int argc, char *argv[]) {
    /*
     * get config
     * quick fix
     */
    Genode::config()->xml_node().attribute("local-ip").value(local_ip, sizeof(local_ip));
    Genode::config()->xml_node().attribute("remote-ip").value(remote_ip, sizeof(remote_ip));

    /* init lwip
     * and receive ip address
     * */
    enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

    lwip_tcpip_init();

    if(lwip_nic_init(inet_addr(local_ip), inet_addr("255.255.255.0"),
                inet_addr("10.0.0.1"), BUF_SIZE, BUF_SIZE)) {
        PERR("We got no ip address!");
        return -1;
    }
    // ---

    /*
     * create and start pager
     */
    DSM::Pager _pager;
    _pager.start();

    /*
     * connect pager to the external memory
     */
    try {
        em.connect_pager(_pager);
    } catch(Exception &e) {
        PERR("Failed to connect pager!");
    }

    /*
     * Get a session for the parent's capability service, so that we
     * are able to create capabilities.
     */
    Cap_connection cap;

    /*
     * A sliced heap is used for allocating session objects - thereby we
     * can release objects seperately.
     */
    static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

    /*
     * Create objects for use by the framework.
     *
     * An 'Rpc_entrypoint' is created to announce our service's root
     * capability to our parent, manage incoming session creation
     * requests, and dispatch the session interface. The incoming RPC
     * requests are dispatched via a dedicated thread. The 'STACK_SIZE'
     * argument defines the size of the thread's stack. The additional
     * string argument is the name of the entry point, used for
     * debugging purposes only.
     */
    enum { STACK_SIZE = 4096 };
    static Rpc_entrypoint ep(&cap, STACK_SIZE, "broker_ep");

    static Broker::Root_component broker_root(&ep, &sliced_heap);
    env()->parent()->announce(ep.manage(&broker_root));

    /*
     * set up lwip server
     * listening for clients
     */
    int s;
    if((s = lwip_socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        PERR("No socket available!");
        return -1;
    }
    struct sockaddr_in in_addr;
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(2000);
    in_addr.sin_addr.s_addr = INADDR_ANY;
    if(lwip_bind(s, (struct sockaddr *)&in_addr, sizeof(in_addr))) {
        PERR("Failed to bind!");
        return -1;
    }

    if(lwip_listen(s, 5)) {
        PERR("Listen failed!");
        return -1;
    }

    /*
     * loop to wait for incoming memory access requests
     */
    while(true) {
        PDBG("Waiting for Client!");
        struct sockaddr addr;
        socklen_t len = sizeof(addr);
        int client = lwip_accept(s, &addr, &len);
        PDBG("Here is one!");
        // checking read or write?
        char write;
        lwip_recv(client, &write, 1, 0);
        // handle read request
        if(!write) {
            // receive offset
            long unsigned offset;
            lwip_recv(client, (char *)&offset, 4, 0);
            PDBG("Remote note wants to read at %lx", offset);
            // send requested page
            lwip_send(client, ptr+offset, PAGE_SIZE, 0);
        }
        // handle write request
        else {
            // receive offset
            long unsigned offset;
            lwip_recv(client, (char *)&offset, 4, 0); 
            PDBG("Remote note wants to write at %lx", offset);
            // receive page and save into buffer
            char buf[PAGE_SIZE];
            lwip_recv(client, &buf, PAGE_SIZE, 0);
            // write buffered page to memory
            Genode::memcpy(ptr+offset, buf, PAGE_SIZE);
            PDBG("Wrote \"%s\" at %lx", ptr+offset, offset);
        }
        lwip_close(client);
    }

    return 0;
}
