#include "Client.hpp"
#include "Server.hpp"
#include "RingBuffer.hpp"
#include <thread>

int main() {
    RingBuffer<std::string> buffer(10);
    Client client(buffer);
    Server server(buffer);

    std::thread client_thread(&Client::run, &client);
    std::thread server_thread(&Server::run, &server);

    client_thread.join();
    server_thread.join();

    return 0;
}
