/*
#ifdef OS_OBJECT_USE_OBJC
#undef OS_OBJECT_USE_OBJC
#endif
#define OS_OBJECT_USE_OBJC 0
*/

#include "AppleNetworkTools.hpp"
#include "IPlugLogger.h"

#include <string>

template <typename HandlerType>
void receive_loop(nw_connection_t connection, HandlerType handlers)
{
    nw_connection_receive(connection, 1, UINT32_MAX,
        ^(dispatch_data_t content, nw_content_context_t context, bool is_complete, nw_error_t receive_error)
        {
            if (receive_error)
            {
                auto err = nw_error_get_error_code(receive_error);
                
                handlers.call_close(connection);
            }
            else    // For now all errors close
            {
                if (is_complete && content)
                {
                    const void *buffer = nullptr;
                    size_t size = 0;
                
                    dispatch_data_t contiguous = dispatch_data_create_map(content, &buffer, &size);
                    handlers.call_receive(connection, buffer, size);
                    
                    //[contiguous release];
                }
                
                receive_loop(connection, handlers);
            }
        });
}

void start_client(nw_connection_t connection, ws_client_handlers handlers)
{
    // Hold a reference until cancelled
    
    nw_connection_set_queue(connection, dispatch_get_main_queue());
    nw_retain(connection);
    
    // Handle state changes
    
    nw_connection_set_state_changed_handler(connection, ^(nw_connection_state_t state, nw_error_t error)
     {
        nw_endpoint_t remote = nw_connection_copy_endpoint(connection);
        errno = error ? nw_error_get_error_code(error) : 0;
     
        if (state == nw_connection_state_ready)
        {
            DBGMSG("Connection to %s port %u succeeded!\n", nw_endpoint_get_hostname(remote), nw_endpoint_get_port(remote));
            
            // Start the receive loop
            
            receive_loop(connection, handlers);
        }
        else if (state == nw_connection_state_cancelled || state == nw_connection_state_failed)
        {
            DBGMSG("Connection to %s port %u cancelled or failed!\n", nw_endpoint_get_hostname(remote), nw_endpoint_get_port(remote));

            // Release the primary reference on the connection that was taken at creation time
           
            nw_release(connection);
        }
        
        nw_release(remote);
    });
     
    nw_connection_start(connection);
}

void start_server(nw_listener_t listener, ws_server_handlers handlers)
{
    // Hold a reference until cancelled
    
    nw_listener_set_queue(listener, dispatch_get_main_queue());
    nw_retain(listener);
    
    // Handle state changes
    
    nw_listener_set_state_changed_handler(listener, ^(nw_listener_state_t state, nw_error_t _Nullable error)
        {
            errno = error ? nw_error_get_error_code(error) : 0;
            
            if (state == nw_listener_state_ready)
            {
                DBGMSG("Server %s port %u is ready!\n", "/ws", nw_listener_get_port(listener));
            }
            else if (state == nw_listener_state_cancelled || state == nw_listener_state_failed)
            {
                DBGMSG("Server %s port %u cancelled or failed!\n", "/ws", nw_listener_get_port(listener));
                
                // Release the primary reference on the connection that was taken at creation time
                
                nw_release(listener);
            }
        });
    
    nw_listener_set_new_connection_handler(listener, ^(nw_connection_t _Nonnull connection)
        {
            handlers.call_connect(connection);
        
            nw_retain(connection);
        
            nw_connection_set_queue(connection, dispatch_get_main_queue());
            nw_connection_set_state_changed_handler(connection, ^(nw_connection_state_t state, nw_error_t error)
                                                {
                nw_endpoint_t remote = nw_connection_copy_endpoint(connection);
                errno = error ? nw_error_get_error_code(error) : 0;
            
                if (state == nw_connection_state_ready)
                {
                    DBGMSG("Connection to %s port %u succeeded!\n", nw_endpoint_get_hostname(remote), nw_endpoint_get_port(remote));
                    
                    handlers.call_ready(connection);
                }
                else if (state == nw_connection_state_cancelled || state == nw_connection_state_failed)
                {
                    DBGMSG("Connection from %s port %u cancelled or failed!\n", nw_endpoint_get_hostname(remote), nw_endpoint_get_port(remote));
                
                    handlers.call_close(connection);
                    
                    // Release the primary reference on the connection that was taken at creation time
                
                    nw_release(connection);
                }
                nw_release(remote);
            });
            
            nw_connection_start(connection);
            receive_loop(connection, handlers);
        });
        
    nw_listener_start(listener);
}

nw_parameters_t create_websocket_parameters()
{
    // Parameters and protocol for websockets

    auto parameters = nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL,
        ^(nw_protocol_options_t options)
        {
            nw_tcp_options_set_enable_keepalive(options, true);
        });
    
    auto protocol_stack = nw_parameters_copy_default_protocol_stack(parameters);
    auto websocket_options = nw_ws_create_options(nw_ws_version_13);
    
    nw_protocol_stack_prepend_application_protocol(protocol_stack, websocket_options);
    //nw_parameters_set_service_class
    
    // Release temporaries
    
    nw_release(protocol_stack);
    nw_release(websocket_options);
    
    return parameters;
}

nw_connection_t create_ws_client(const char *host,
                                 int port,
                                 const char *path,
                                 ws_client_handlers handlers)
{
    std::string port_str = std::to_string(port);
    std::string sock_address_url = "ws://" + std::string(host) + ":" + port_str + path;
        
    auto endpoint = nw_endpoint_create_url(sock_address_url.c_str());
    
    // Create connection with the correct parameters

    auto parameters = create_websocket_parameters();
    auto connection = nw_connection_create(endpoint, parameters);
    
    // Start connection
    
    start_client(connection, handlers);
    
    // Release resources
    
    nw_release(parameters);
    nw_release(endpoint);
    
    return connection;
}

nw_listener_t create_ws_server(const char *port,
                               const char *path,
                               ws_server_handlers handlers)
{
    std::string sock_address_url = "ws://localhost:" + std::string(port) + path;
    auto endpoint = nw_endpoint_create_url(sock_address_url.c_str());
    
    // Parameters and protocol for websockets
    
    auto parameters = create_websocket_parameters();
    nw_parameters_set_local_endpoint(parameters, endpoint);
    
    // Create listener
    
    auto listener = nw_listener_create_with_port(port, parameters);
        
    // Start listener
    
    start_server(listener, handlers);
    
    // Release resources
    
    nw_release(parameters);
    nw_release(endpoint);
    
    return listener;
}

void release_ws_server(nw_listener_t server)
{
    nw_listener_cancel(server);
    nw_release(server);
}

void release_ws_client(nw_connection_t client)
{
    nw_connection_cancel(client);
    nw_release(client);
}

void send_ws_data(nw_connection_t connection, const void *data, size_t size)
{
    dispatch_data_t dispatch_data = dispatch_data_create(data, size, dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
        
    nw_protocol_metadata_t metadata = nw_ws_create_metadata(nw_ws_opcode_binary);
    nw_content_context_t context = nw_content_context_create("send");
    nw_content_context_set_metadata_for_protocol(context, metadata);
    
    nw_connection_send(connection,
                       dispatch_data,
                       context,
                       true,
                       ^(nw_error_t _Nullable error)
                       {
                            if (error != NULL)
                            {
                                errno = nw_error_get_error_code(error);
                                
                                DBGMSG("send error\n");
                                nw_release(context);
                            }
    });
}
