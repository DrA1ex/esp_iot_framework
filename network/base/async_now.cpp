#ifdef ARDUINO_ARCH_ESP32

#include "async_now.h"

#include <esp_wifi.h>

uint64_t mac_to_key(const uint8_t *mac_addr) {
    uint64_t mac_addr_key = 0;
    memcpy(&mac_addr_key, mac_addr, ESP_NOW_ETH_ALEN);

    return mac_addr_key;
}

AsyncEspNow AsyncEspNow::_instance {};

bool AsyncEspNow::begin() {
    if (_initialized) return false;

    if (!Dispatcher::begin()) {
        D_PRINT("AsyncEspNow: Unable to initialized Dispatcher");
        return false;
    }

    if (WiFiClass::getMode() == WIFI_MODE_NULL) WiFiClass::mode(WIFI_MODE_STA);

    auto ret = esp_now_init();
    if (ret != ESP_OK) {
        D_PRINTF("AsyncEspNow: Unable to initialize ESP NOW: %i\r'n", ret);
        return false;
    }

    bool cb_init_success = esp_now_register_send_cb(_on_sent) == ESP_OK;
    cb_init_success = cb_init_success && esp_now_register_recv_cb(_on_receive) == ESP_OK;

    if (!cb_init_success) {
        esp_now_deinit();

        D_PRINT("AsyncEspNow: Unable to initialize ESP NOW: failed to attach callbacks");
        return false;
    }

    _initialized = true;
    return true;
}

void AsyncEspNow::end() {
    if (!_initialized) return;

    set_on_packet_cb(nullptr);
    _initialized = false;

    esp_now_deinit();
}

Future<void> AsyncEspNow::send(const uint8_t *mac_addr, const uint8_t *data, uint8_t size) {
    if (!_initialized) {
        D_PRINT("AsyncEspNow: Not initialized");
        return Future<void>::errored();
    }

    if (!register_peer(mac_addr)) {
        D_PRINT("AsyncEspNow: Failed to register peer");
        return Future<void>::errored();
    }

    D_PRINT("AsyncEspNow: sending packet");
    D_WRITE("\t- Destination: ");
    D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);
    D_PRINTF("\t- Size: %i\r\n", size);

    auto promise = std::make_shared<Promise<void>>();

    auto send_key = mac_to_key(mac_addr);
    _send_order[send_key].push(promise);

    if (esp_now_send(mac_addr, data, size) != ESP_OK) {
        D_PRINT("AsyncEspNow: Failed to register peer");

        _send_order[send_key].pop();
        promise->set_error();
    }

    return Future {promise};
}

bool AsyncEspNow::is_peer_exists(const uint8_t *mac_addr) const {
    return _initialized && esp_now_is_peer_exist(mac_addr);
}

bool AsyncEspNow::register_peer(const uint8_t *mac_addr, uint8_t channel) {
    if (!_initialized) return false;
    if (esp_now_is_peer_exist(mac_addr)) return true;

    esp_now_peer_info peer {};
    peer.channel = channel;
    peer.encrypt = false;
    memcpy(peer.peer_addr, mac_addr, ESP_NOW_ETH_ALEN);

    auto ret = esp_now_add_peer(&peer);
    if (ret != ESP_OK) {
        D_PRINTF("AsyncEspNow: Unable to register peer: %i\r\n", ret);
        return false;
    }

    D_WRITE("AsyncEspNow: Register new peer ");
    D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);

    uint64_t mac_addr_key = mac_to_key(mac_addr);
    _send_order[mac_addr_key] = {};
    _peers.push_back(peer);

    return true;
}

bool AsyncEspNow::unregister_peer(const uint8_t *mac_addr) {
    if (!_initialized) return false;
    if (!esp_now_is_peer_exist(mac_addr)) return true;

    _peers.erase(std::find_if(_peers.begin(), _peers.end(), [mac_addr](auto &peer) {
        return memcmp(peer.peer_addr, mac_addr, ESP_NOW_ETH_ALEN) == 0;
    }));

    D_WRITE("AsyncEspNow: Unregister peer: ");
    D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);

    return esp_now_del_peer(mac_addr) == ESP_OK;
}

bool AsyncEspNow::change_channel(uint8_t channel) { // NOLINT(*-make-member-function-const)
    if (!_initialized) return false;

    D_PRINTF("AsyncEspNow: Change channel to: %i\r\n", channel + 1);

    bool success = esp_wifi_set_promiscuous(true) == ESP_OK;
    success && esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) == ESP_OK;
    success && esp_wifi_set_promiscuous(false) == ESP_OK;

    return success;
}

void AsyncEspNow::_on_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    auto &self = instance();

    uint64_t mac_addr_key = mac_to_key(mac_addr);
    auto it = self._send_order.find(mac_addr_key);
    if (it == self._send_order.end() || it->second.empty()) {
        D_WRITE("AsyncEspNow: Unexpected sent event. Destination: ");
        D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);
        return;
    }

    VERBOSE(D_PRINT("AsyncEspNow: Received sent event"));

    auto &queue = it->second;
    const auto promise = queue.front();
    queue.pop();

    if (status == ESP_NOW_SEND_SUCCESS) {
        VERBOSE(D_WRITE("AsyncEspNow: Send confirmed "));
        VERBOSE(D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN));

        promise->set_success();
    } else {
        D_PRINTF("AsyncEspNow: Error while sending data: %i. Destination: ", status);
        D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);

        promise->set_error();
    }
}

void AsyncEspNow::_on_receive(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    EspNowPacket packet;

    packet.size = data_len;
    packet.data = std::shared_ptr<uint8_t[]>(new uint8_t[data_len]);
    memcpy(packet.data.get(), data, data_len);
    memcpy(packet.mac_addr, mac_addr, sizeof(packet.mac_addr));

    D_PRINT("AsyncEspNow: Received packet");
    D_WRITE("\t- Sender: ");
    D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);
    D_PRINTF("\t- Size: %i\r\n", data_len);
    VERBOSE(D_WRITE("\t- Data: "));
    VERBOSE(D_PRINT_HEX(data, data_len));

    auto &self = instance();
    if (self._on_packet_cb) {
        self._on_packet_cb(std::move(packet));
    }
}

#endif
