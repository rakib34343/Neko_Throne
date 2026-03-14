#include "include/configs/sub/clash.hpp"

#include <sstream>

namespace clash {

// Helper: safely check if the node is a mapping and contains key
static bool safeContains(const fkyaml::node& node, const std::string& key) {
    return node.is_mapping() && node.contains(key);
}

// Helper: safely retrieve a value of type T from a mapping node; no-op if absent
template<typename T>
static void safeGet(const fkyaml::node& node, const std::string& key, T& field) {
    if (safeContains(node, key)) {
        field = node.at(key).get_value<T>();
    }
}

void from_node(const fkyaml::node& node, MyBool& b) {
    if (node.is_boolean()) {
        b.value = node.get_value<bool>();
    } else if (node.is_integer()) {
        b.value = node.get_value<int64_t>() != 0;
    } else if (node.is_string()) {
        const auto s = node.get_value<std::string>();
        b.value = (s == "true" || s == "1" || s == "yes");
    }
}

void from_node(const fkyaml::node& node, MyInt& i) {
    if (node.is_integer()) {
        i.value = static_cast<int>(node.get_value<int64_t>());
    } else if (node.is_float_number()) {
        i.value = static_cast<int>(node.get_value<double>());
    } else if (node.is_string()) {
        try {
            i.value = std::stoi(node.get_value<std::string>());
        } catch (...) {}
    }
}

void from_node(const fkyaml::node& node, WgReserved& w) {
    if (node.is_sequence()) {
        for (const auto& elem : node) {
            if (elem.is_integer()) {
                w.value.push_back(static_cast<uint8_t>(elem.get_value<int64_t>()));
            }
        }
    } else if (node.is_string()) {
        // comma-separated integers
        std::istringstream ss(node.get_value<std::string>());
        std::string token;
        while (std::getline(ss, token, ',')) {
            try {
                w.value.push_back(static_cast<uint8_t>(std::stoi(token)));
            } catch (...) {}
        }
    }
}

void from_node(const fkyaml::node& node, smuxOpts& opts) {
    if (!node.is_mapping()) return;
    safeGet(node, "enabled", opts.enabled);
    safeGet(node, "max-connections", opts.max_connections);
    safeGet(node, "max-streams", opts.max_streams);
    safeGet(node, "min-streams", opts.min_streams);
    safeGet(node, "padding", opts.padding);
    safeGet(node, "protocol", opts.protocol);
}

void from_node(const fkyaml::node& node, grpcOpts& opts) {
    if (!node.is_mapping()) return;
    if (safeContains(node, "grpc-service-name")) {
        opts.grpc_service_name = node.at("grpc-service-name").get_value<std::string>();
    } else {
        safeGet(node, "serviceName", opts.grpc_service_name);
    }
}

void from_node(const fkyaml::node& node, httpOpts& opts) {
    if (!node.is_mapping()) return;
    safeGet(node, "method", opts.method);
    if (safeContains(node, "path")) {
        const auto& pathNode = node.at("path");
        if (pathNode.is_sequence()) {
            opts.path = pathNode.get_value<std::vector<std::string>>();
        } else if (pathNode.is_string()) {
            opts.path.push_back(pathNode.get_value<std::string>());
        }
    }
    if (safeContains(node, "headers")) {
        const auto& headersNode = node.at("headers");
        if (headersNode.is_mapping()) {
            for (auto itr : headersNode.map_items()) {
                const auto key = itr.key().get_value<std::string>();
                if (itr.value().is_sequence()) {
                    opts.headers[key] = itr.value().get_value<std::vector<std::string>>();
                } else if (itr.value().is_string()) {
                    opts.headers[key] = {itr.value().get_value<std::string>()};
                }
            }
        }
    }
}

void from_node(const fkyaml::node& node, h2Opts& opts) {
    if (!node.is_mapping()) return;
    safeGet(node, "path", opts.path);
    if (safeContains(node, "host")) {
        const auto& hostNode = node.at("host");
        if (hostNode.is_sequence()) {
            opts.host = hostNode.get_value<std::vector<std::string>>();
        } else if (hostNode.is_string()) {
            opts.host.push_back(hostNode.get_value<std::string>());
        }
    }
}

void from_node(const fkyaml::node& node, wsOpts& opts) {
    if (!node.is_mapping()) return;
    safeGet(node, "path", opts.path);
    safeGet(node, "early-data-header-name", opts.early_data_header_name);
    safeGet(node, "max-early-data", opts.max_early_data);
    safeGet(node, "v2ray-http-upgrade", opts.v2ray_http_upgrade);
    if (safeContains(node, "headers")) {
        const auto& headersNode = node.at("headers");
        if (headersNode.is_mapping()) {
            for (auto itr : headersNode.map_items()) {
                opts.headers[itr.key().get_value<std::string>()] =
                    itr.value().get_value<std::string>();
            }
        }
    }
}

void from_node(const fkyaml::node& node, realityOpts& opts) {
    if (!node.is_mapping()) return;
    if (safeContains(node, "public-key")) {
        opts.public_key = node.at("public-key").get_value<std::string>();
    } else {
        safeGet(node, "public_key", opts.public_key);
    }
    if (safeContains(node, "short-id")) {
        opts.short_id = node.at("short-id").get_value<std::string>();
    } else {
        safeGet(node, "short_id", opts.short_id);
    }
}

void from_node(const fkyaml::node& node, obfs& opts) {
    if (!node.is_mapping()) return;
    safeGet(node, "mode", opts.mode);
    safeGet(node, "host", opts.host);
}

void from_node(const fkyaml::node& node, v2rayPlugin& opts) {
    if (!node.is_mapping()) return;
    safeGet(node, "mode", opts.mode);
    safeGet(node, "tls", opts.tls);
    safeGet(node, "host", opts.host);
    safeGet(node, "path", opts.path);
    safeGet(node, "mux", opts.mux);
}

void from_node(const fkyaml::node& node, wgPeer& peer) {
    if (!node.is_mapping()) return;
    safeGet(node, "server", peer.server);
    safeGet(node, "port", peer.port);
    safeGet(node, "ip", peer.ip);
    safeGet(node, "ipv6", peer.ipv6);
    if (safeContains(node, "public-key")) {
        peer.public_key = node.at("public-key").get_value<std::string>();
    } else {
        safeGet(node, "public_key", peer.public_key);
    }
    if (safeContains(node, "pre-shared-key")) {
        peer.pre_shared_key = node.at("pre-shared-key").get_value<std::string>();
    } else {
        safeGet(node, "pre_shared_key", peer.pre_shared_key);
    }
    if (safeContains(node, "reserved")) {
        peer.reserved = node.at("reserved").get_value<WgReserved>();
    }
    if (safeContains(node, "allowed-ips")) {
        const auto& ipsNode = node.at("allowed-ips");
        if (ipsNode.is_sequence()) {
            peer.allowed_ips = ipsNode.get_value<std::vector<std::string>>();
        }
    } else if (safeContains(node, "allowed_ips")) {
        const auto& ipsNode = node.at("allowed_ips");
        if (ipsNode.is_sequence()) {
            peer.allowed_ips = ipsNode.get_value<std::vector<std::string>>();
        }
    }
}

void from_node(const fkyaml::node& node, Proxies& p) {
    if (!node.is_mapping()) return;

    safeGet(node, "name", p.name);
    safeGet(node, "type", p.type);
    safeGet(node, "server", p.server);
    safeGet(node, "port", p.port);
    safeGet(node, "cipher", p.cipher);
    safeGet(node, "uuid", p.uuid);
    safeGet(node, "udp", p.udp);
    safeGet(node, "tls", p.tls);
    safeGet(node, "network", p.network);
    safeGet(node, "username", p.username);
    safeGet(node, "password", p.password);
    safeGet(node, "sni", p.sni);
    safeGet(node, "plugin", p.plugin);
    safeGet(node, "fingerprint", p.fingerprint);
    safeGet(node, "obfs", p.obfs);
    safeGet(node, "protocol", p.protocol);
    safeGet(node, "flow", p.flow);
    safeGet(node, "encryption", p.encryption);
    safeGet(node, "ip", p.ip);
    safeGet(node, "ipv6", p.ipv6);
    safeGet(node, "tfo", p.tfo);
    safeGet(node, "mptcp", p.mptcp);
    safeGet(node, "up", p.up);
    safeGet(node, "down", p.down);
    safeGet(node, "ports", p.ports);
    safeGet(node, "reduce-rtt", p.reduce_rtt);
    safeGet(node, "obfs-password", p.obfs_password);

    if (safeContains(node, "alterId")) {
        p.alterId = node.at("alterId").get_value<MyInt>();
    } else if (safeContains(node, "alter-id")) {
        p.alterId = node.at("alter-id").get_value<MyInt>();
    }

    if (safeContains(node, "skip-cert-verify")) {
        p.skip_cert_verify = node.at("skip-cert-verify").get_value<MyBool>();
    }

    if (safeContains(node, "servername")) {
        p.servername = node.at("servername").get_value<std::string>();
    }

    if (safeContains(node, "client-fingerprint")) {
        p.client_fingerprint = node.at("client-fingerprint").get_value<std::string>();
    } else {
        safeGet(node, "client_fingerprint", p.client_fingerprint);
    }

    if (safeContains(node, "packet-encoding")) {
        p.packet_encoding = node.at("packet-encoding").get_value<std::string>();
    } else {
        safeGet(node, "packet_encoding", p.packet_encoding);
    }

    if (safeContains(node, "dialer-proxy")) {
        p.dialer_proxy = node.at("dialer-proxy").get_value<std::string>();
    } else {
        safeGet(node, "dialer_proxy", p.dialer_proxy);
    }

    if (safeContains(node, "obfs-param")) {
        p.obfs_param = node.at("obfs-param").get_value<std::string>();
    } else {
        safeGet(node, "obfs_param", p.obfs_param);
    }

    if (safeContains(node, "protocol-param")) {
        p.protocol_param = node.at("protocol-param").get_value<std::string>();
    } else {
        safeGet(node, "protocol_param", p.protocol_param);
    }

    if (safeContains(node, "auth-str")) {
        p.auth_str = node.at("auth-str").get_value<std::string>();
    } else {
        safeGet(node, "auth_str", p.auth_str);
    }

    if (safeContains(node, "auth-str1")) {
        p.auth_str1 = node.at("auth-str1").get_value<std::string>();
    } else {
        safeGet(node, "auth_str1", p.auth_str1);
    }

    if (safeContains(node, "disable-mtu-discovery")) {
        p.disable_mtu_discovery = node.at("disable-mtu-discovery").get_value<MyBool>();
    }

    if (safeContains(node, "fast-open")) {
        p.fast_open = node.at("fast-open").get_value<MyBool>();
    } else {
        safeGet(node, "fast_open", p.fast_open);
    }

    safeGet(node, "mtu", p.mtu);

    if (safeContains(node, "disable-sni")) {
        p.disable_sni = node.at("disable-sni").get_value<MyBool>();
    }

    if (safeContains(node, "congestion-controller")) {
        p.congestion_controller = node.at("congestion-controller").get_value<std::string>();
    } else {
        safeGet(node, "congestion_controller", p.congestion_controller);
    }

    if (safeContains(node, "udp-relay-mode")) {
        p.udp_relay_mode = node.at("udp-relay-mode").get_value<std::string>();
    } else {
        safeGet(node, "udp_relay_mode", p.udp_relay_mode);
    }

    if (safeContains(node, "heartbeat-interval")) {
        p.heartbeat_interval = node.at("heartbeat-interval").get_value<MyInt>();
    } else {
        safeGet(node, "heartbeat_interval", p.heartbeat_interval);
    }

    if (safeContains(node, "recv-window")) {
        p.recv_window = node.at("recv-window").get_value<MyInt>();
    } else {
        safeGet(node, "recv_window", p.recv_window);
    }

    if (safeContains(node, "recv-window-conn")) {
        p.recv_window_conn = node.at("recv-window-conn").get_value<MyInt>();
    } else {
        safeGet(node, "recv_window_conn", p.recv_window_conn);
    }

    if (safeContains(node, "recv-window1")) {
        p.recv_window1 = node.at("recv-window1").get_value<MyInt>();
    } else {
        safeGet(node, "recv_window1", p.recv_window1);
    }

    if (safeContains(node, "recv-window-conn1")) {
        p.recv_window_conn1 = node.at("recv-window-conn1").get_value<MyInt>();
    } else {
        safeGet(node, "recv_window_conn1", p.recv_window_conn1);
    }

    if (safeContains(node, "idle-session-check-interval")) {
        p.idle_session_check_interval = node.at("idle-session-check-interval").get_value<MyInt>();
    } else {
        safeGet(node, "idle_session_check_interval", p.idle_session_check_interval);
    }

    if (safeContains(node, "idle-session-timeout")) {
        p.idle_session_timeout = node.at("idle-session-timeout").get_value<MyInt>();
    } else {
        safeGet(node, "idle_session_timeout", p.idle_session_timeout);
    }

    if (safeContains(node, "min-idle-session")) {
        p.min_idle_session = node.at("min-idle-session").get_value<MyInt>();
    } else {
        safeGet(node, "min_idle_session", p.min_idle_session);
    }

    if (safeContains(node, "udp-over-tcp")) {
        p.udp_over_tcp = node.at("udp-over-tcp").get_value<MyBool>();
    } else {
        safeGet(node, "udp_over_tcp", p.udp_over_tcp);
    }

    if (safeContains(node, "udp-over-tcp-version")) {
        p.udp_over_tcp_version = node.at("udp-over-tcp-version").get_value<MyInt>();
    } else {
        safeGet(node, "udp_over_tcp_version", p.udp_over_tcp_version);
    }

    if (safeContains(node, "alpn")) {
        const auto& alpnNode = node.at("alpn");
        if (alpnNode.is_sequence()) {
            p.alpn = alpnNode.get_value<std::vector<std::string>>();
        }
    }

    if (safeContains(node, "host-key")) {
        const auto& hkNode = node.at("host-key");
        if (hkNode.is_sequence()) {
            p.host_key = hkNode.get_value<std::vector<std::string>>();
        }
    } else if (safeContains(node, "host_key")) {
        const auto& hkNode = node.at("host_key");
        if (hkNode.is_sequence()) {
            p.host_key = hkNode.get_value<std::vector<std::string>>();
        }
    }

    if (safeContains(node, "host-key-algorithms")) {
        const auto& hkaNode = node.at("host-key-algorithms");
        if (hkaNode.is_sequence()) {
            p.host_key_algorithms = hkaNode.get_value<std::vector<std::string>>();
        }
    } else if (safeContains(node, "host_key_algorithms")) {
        const auto& hkaNode = node.at("host_key_algorithms");
        if (hkaNode.is_sequence()) {
            p.host_key_algorithms = hkaNode.get_value<std::vector<std::string>>();
        }
    }

    if (safeContains(node, "public-key")) {
        p.public_key = node.at("public-key").get_value<std::string>();
    } else {
        safeGet(node, "public_key", p.public_key);
    }

    if (safeContains(node, "pre-shared-key")) {
        p.pre_shared_key = node.at("pre-shared-key").get_value<std::string>();
    } else {
        safeGet(node, "pre_shared_key", p.pre_shared_key);
    }

    if (safeContains(node, "private-key")) {
        p.private_key = node.at("private-key").get_value<std::string>();
    } else {
        safeGet(node, "private_key", p.private_key);
    }

    if (safeContains(node, "private-key-passphrase")) {
        p.private_key_passphrase = node.at("private-key-passphrase").get_value<std::string>();
    } else {
        safeGet(node, "private_key_passphrase", p.private_key_passphrase);
    }

    if (safeContains(node, "reserved")) {
        p.reserved = node.at("reserved").get_value<WgReserved>();
    }

    if (safeContains(node, "peers")) {
        const auto& peersNode = node.at("peers");
        if (peersNode.is_sequence()) {
            p.peers = peersNode.get_value<std::vector<wgPeer>>();
        }
    }

    if (safeContains(node, "ws-opts")) {
        p.ws_opts = node.at("ws-opts").get_value<wsOpts>();
    } else if (safeContains(node, "ws_opts")) {
        p.ws_opts = node.at("ws_opts").get_value<wsOpts>();
    }

    if (safeContains(node, "ws-headers")) {
        const auto& whNode = node.at("ws-headers");
        if (whNode.is_mapping()) {
            for (auto itr : whNode.map_items()) {
                p.ws_headers[itr.key().get_value<std::string>()] =
                    itr.value().get_value<std::string>();
            }
        }
    }

    if (safeContains(node, "h2-opts")) {
        p.h2_opts = node.at("h2-opts").get_value<h2Opts>();
    } else if (safeContains(node, "h2_opts")) {
        p.h2_opts = node.at("h2_opts").get_value<h2Opts>();
    }

    if (safeContains(node, "http-opts")) {
        p.http_opts = node.at("http-opts").get_value<httpOpts>();
    } else if (safeContains(node, "http_opts")) {
        p.http_opts = node.at("http_opts").get_value<httpOpts>();
    }

    if (safeContains(node, "grpc-opts")) {
        p.grpc_opts = node.at("grpc-opts").get_value<grpcOpts>();
    } else if (safeContains(node, "grpc_opts")) {
        p.grpc_opts = node.at("grpc_opts").get_value<grpcOpts>();
    }

    if (safeContains(node, "reality-opts")) {
        p.reality_opts = node.at("reality-opts").get_value<realityOpts>();
    } else if (safeContains(node, "reality_opts")) {
        p.reality_opts = node.at("reality_opts").get_value<realityOpts>();
    }

    if (safeContains(node, "smux")) {
        p.smux = node.at("smux").get_value<smuxOpts>();
    }

    if (safeContains(node, "plugin-opts")) {
        p.plugin_opts = node.at("plugin-opts");
    } else if (safeContains(node, "plugin_opts")) {
        p.plugin_opts = node.at("plugin_opts");
    }
}

void from_node(const fkyaml::node& node, Clash& c) {
    if (!node.is_mapping()) return;
    if (safeContains(node, "proxies")) {
        const auto& proxiesNode = node.at("proxies");
        if (proxiesNode.is_sequence()) {
            c.proxies = proxiesNode.get_value<std::vector<Proxies>>();
        }
    }
}

} // namespace clash
