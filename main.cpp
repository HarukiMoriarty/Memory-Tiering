#include <boost/thread/thread.hpp>

#include "Client.hpp"
#include "Server.hpp"
#include "RingBuffer.hpp"
#include "ConfigParser.hpp"

int main(int argc, char* argv[]) {
    ConfigParser config;

    if (!config.parse(argc, argv)) {
        return config.isHelpRequested() ? 0 : 1;
    }

    RingBuffer<std::string> buffer(config.getBufferSize());
    Client client(buffer, config.getMessageCount());
    Server server(buffer);

    boost::thread client_thread(&Client::run, &client);
    boost::thread server_thread(&Server::run, &server);

    client_thread.join();
    server_thread.join();

    return 0;
}
