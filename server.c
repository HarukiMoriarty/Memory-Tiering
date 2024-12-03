#include "TieringManager.h"
#include <iostream>

int main() {
    TieringManager manager;

    // Allocate a page
    void* page = manager.allocatePage();
    if (!page) {
        std::cerr << "Failed to allocate page." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Page allocated at address: " << page << std::endl;

    // Write something to the page to ensure it's allocated
    *(char*)page = 'A';

    // Query NUMA node
    int currentNode = manager.queryNUMANode(page);
    if (currentNode == -1) {
        std::cerr << "Failed to query NUMA node." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Page is currently on NUMA node: " << currentNode << std::endl;

    // Move the page to another NUMA node (e.g., node 3)
    int targetNode = 0;
    if (!manager.movePage(page, targetNode)) {
        std::cerr << "Failed to move page." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Page moved to NUMA node: " << targetNode << std::endl;

    return EXIT_SUCCESS;
}
