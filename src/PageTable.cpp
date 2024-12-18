#include "PageTable.hpp"

PageMetadata::PageMetadata(void* addr, PageLayer layer, int id)
    : page_address(addr),
    page_layer(layer),
    page_id(id),
    last_access_time(std::chrono::steady_clock::now()),
    access_count(0)
{
}

PageTable::PageTable(size_t size) : table_(size) {}

PageMetadata PageTable::getPage(size_t index) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (index < table_.size()) {
        return table_[index];
    }
    return PageMetadata();
}

size_t PageTable::size() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return table_.size();
}

void PageTable::updatePage(size_t index, const PageMetadata& metadata) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (index < table_.size()) {
        table_[index] = metadata;
    }
}

void PageTable::updateAccess(size_t index) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (index < table_.size()) {
        table_[index].last_access_time = std::chrono::steady_clock::now();
        table_[index].access_count++;
    }
}

void PageTable::updatePageLayer(size_t index, PageLayer new_layer) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (index < table_.size()) {
        table_[index].page_layer = new_layer;
        table_[index].last_access_time = std::chrono::steady_clock::now();
        table_[index].access_count++;
    }
}

PageMetadata PageTable::scanNext() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    if (table_.empty()) {
        return PageMetadata(); // Handle empty table case
    }

    // Return the current page and move to the next index (circularly)
    PageMetadata page = table_[current_index_];
    current_index_ = (current_index_ + 1) % table_.size(); // Wrap around using modulo

    return page;
}

void PageTable::resetAccessCount() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    for (auto& page : table_) {
        page.access_count = 0; // Reset access count
    }
}