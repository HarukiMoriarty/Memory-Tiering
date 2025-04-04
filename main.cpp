#include "Client.hpp"
#include "Common.hpp"
#include "ConfigParser.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"
#include "RingBuffer.hpp"
#include "Server.hpp"

int main(int argc, char* argv[]) {
  // Initialize components
  Logger::getInstance().init();
  ConfigParser config;

  if (!config.parse(argc, argv)) {
    return config.isHelpRequested() ? 0 : 1;
  }

  // Set up client-server request buffer, get client configurations
  RingBuffer<ClientMessage> clientRequestBuffer(config.getBufferSize());
  const auto& clientConfigs = config.getClientConfigs();

  // Create and initialize server
  ServerMemoryConfig serverConfig = config.getServerMemoryConfig();
  PolicyConfig policyConfig = config.getPolicyConfig();
  Server server(clientRequestBuffer, clientConfigs, &serverConfig,
    &policyConfig, config.getSampleRate());

  // NOTICE: wait for hot page threshold to expire
  boost::this_thread::sleep_for(boost::chrono::seconds(
    config.getPolicyConfig().scan_interval));

  // Create and start clients
  std::vector<std::shared_ptr<Client>> clients;
  std::vector<boost::thread> clientThreads;

  for (size_t i = 0; i < clientConfigs.size(); i++) {
    size_t clientPageSize = 0;
    for (size_t tierSize : clientConfigs[i].tier_sizes) {
      clientPageSize += tierSize;
    }

    auto client = std::make_shared<Client>(
      i, clientRequestBuffer, config.getRunningTime(), clientPageSize,
      clientConfigs[i].pattern, config.getRwRatio());

    clients.push_back(client);
    clientThreads.emplace_back([client]() { client->run(); });
  }

  // Start server thread
  boost::thread serverThread(&Server::start, &server);

  // Wait for completion
  for (auto& thread : clientThreads) {
    thread.join();
  }
  serverThread.join();

  // Output metrics
  auto& metrics = Metrics::getInstance();
  if (config.getServerMemoryConfig().num_tiers == 3) {
    metrics.printMetricsThreeTiers();
  }
  else {
    metrics.printMetricsTwoTiers();
  }

  metrics.outputLatencyCDFToFile(config.getLatencyOutputFile());
  return 0;
}