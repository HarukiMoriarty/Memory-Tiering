#include "PageTable.hpp"

PageMetadata::PageMetadata(int addr, int layer)
    : page_address(addr),
    page_layer(layer),
    last_access_time(std::chrono::steady_clock::now())
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

void PageTable::updateAccessTime(size_t index) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (index < table_.size()) {
        table_[index].last_access_time = std::chrono::steady_clock::now();
    }
}

void PageTable::updatePageLayer(size_t index, int new_layer) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (index < table_.size()) {
        table_[index].page_layer = new_layer;
        table_[index].last_access_time = std::chrono::steady_clock::now();
    }
}