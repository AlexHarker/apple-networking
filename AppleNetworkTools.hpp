
#ifndef APPLENETWORKTOOLS_H
#define APPLENETWORKTOOLS_H

#include "Network/Network.h"

#include "NetworkHandlers.hpp"

using ws_server_handlers = server_handlers<nw_connection_t>;
using ws_client_handlers = client_handlers<nw_connection_t>;

nw_listener_t create_ws_server(const char *port,
                               const char *path,
                               ws_server_handlers handlers);

nw_connection_t create_ws_client(const char *host,
                                 int port,
                                 const char *path,
                                 ws_client_handlers handlers);

void release_ws_server(nw_listener_t server);
void release_ws_client(nw_connection_t client);

void send_ws_data(nw_connection_t connection, const void *data, size_t size);

#endif /* APPLENETWORKTOOLS_H */
