#include "Client.hpp"
#include "Common.hpp"
#include "ConfigParser.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"
#include "RingBuffer.hpp"
#include "Server.hpp"

int main(int argc, char *argv[]) {
  Logger::getInstance().init();
  ConfigParser config;

  if (!config.parse(argc, argv)) {
    return config.isHelpRequested() ? 0 : 1;
  }

  RingBuffer<ClientMessage> client_req_buffer(config.getBufferSize());
  const auto &client_configs = config.getClientConfigs();

  ServerMemoryConfig server_config = config.getServerMemoryConfig();
  PolicyConfig policy_config = config.getPolicyConfig();
  Server server(client_req_buffer, client_configs, server_config, policy_config,
                config.getSampleRate());

  // Sleep for hot time thershold time, make sure loaded page will not be
  // classified as hot page.
  boost::this_thread::sleep_for(
      boost::chrono::milliseconds(policy_config.hot_access_interval));

  std::vector<std::shared_ptr<Client>> clients;
  std::vector<boost::thread> client_threads;
  for (size_t i = 0; i < client_configs.size(); i++) {
    const auto &client_config = client_configs[i];
    size_t client_page_size = 0;
    for (size_t tier_size : client_config.tier_sizes) {
      client_page_size += tier_size;
    }
    auto client = std::make_shared<Client>(
        i, client_req_buffer, config.getRunningTime(), client_page_size,
        client_config.pattern, config.getRwRatio());
    clients.push_back(client);
    client_threads.emplace_back([client]() { client->run(); });
  }

  boost::thread server_thread(&Server::start, &server);

  for (auto &thread : client_threads) {
    thread.join();
  }
  server_thread.join();

  if (server_config.num_tiers == 3) {
    Metrics::getInstance().printMetricsThreeTiers();
  } else {
    Metrics::getInstance().printMetricsTwoTiers();
  }
  Metrics::getInstance().outputLatencyCDFToFile(config.getLatencyOutputFile());
  return 0;
}
