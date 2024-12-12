#include <boost/thread/thread.hpp>

#include "Client.hpp"
#include "Server.hpp"
#include "RingBuffer.hpp"
#include "ConfigParser.hpp"
#include "Common.hpp"

int main(int argc, char* argv[]) {
    ConfigParser config;

    if (!config.parse(argc, argv)) {
        return config.isHelpRequested() ? 0 : 1;
    }

    RingBuffer<Message> buffer(config.getBufferSize());
    std::vector<int> memory_sizes;
    const auto& client_configs = config.getClientConfigs();
    for (const auto& client_config : client_configs) {
        memory_sizes.push_back(client_config.memory_size);
    }

    Server server(buffer, memory_sizes);

    std::vector<std::shared_ptr<Client>> clients;
    std::vector<boost::thread> client_threads;
    for (size_t i = 0; i < client_configs.size(); i++) {
        const auto& client_config = client_configs[i];
        auto client = std::make_shared<Client>(
            buffer,
            i,
            config.getMessageCount(),
            client_config.memory_size,
            client_config.pattern
        );
        clients.push_back(client);
        client_threads.emplace_back([client]() { client->run(); });
    }

    boost::thread server_thread(&Server::run, &server);

    for (auto& thread : client_threads) {
        thread.join();
    }
    server_thread.join();

    return 0;
}
