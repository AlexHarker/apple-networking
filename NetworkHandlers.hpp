
#ifndef NETWORKHANDLERS_H
#define NETWORKHANDLERS_H

// Function type definitions

template <class T>
struct network_handlers
{
    using connect_handler = void(*)(T, void *);
    using receive_handler = void(*)(T, const void *, size_t, void *);
};

// Client handlers

template <class T>
struct client_handlers
{
    using connect_handler = typename network_handlers<T>::connect_handler;
    using receive_handler = typename network_handlers<T>::receive_handler;
    
    void call_receive(T connection, const void *buffer, size_t size) const
    {
        m_receive(connection, buffer, size, m_owner);
    }
    
    void call_close(T connection) const
    {
        m_close(connection, m_owner);
    }
    
    receive_handler m_receive;
    connect_handler m_close;
    
    void *m_owner;
};

// Server handlers

template <class T>
struct server_handlers
{
    using connect_handler = typename network_handlers<T>::connect_handler;
    using receive_handler = typename network_handlers<T>::receive_handler;
    
    void call_connect(T connection) const
    {
        m_connect(connection, m_owner);
    }
    
    void call_ready(T connection) const
    {
        m_ready(connection, m_owner);
    }

    void call_receive(T connection, const void *buffer, size_t size) const
    {
        m_receive(connection, buffer, size, m_owner);
    }
    
    void call_close(T connection) const
    {
        m_close(connection, m_owner);
    }

    connect_handler m_connect;
    connect_handler m_ready;
    receive_handler m_receive;
    connect_handler m_close;
    
    void *m_owner;
};

#endif /* NETWORKHANDLERS_H */
