#include <baldr/docker.hpp>

#include <libutl/rlog.hpp>
#include <libnova/error.hpp>
#include <libnova/log.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <fmt/format.h>

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <tuple>

namespace baldr {

namespace {

namespace beast = boost::beast;
namespace http = boost::beast::http;

constexpr auto default_api_version = "v1.43";

/**
 * @brief   Decode the 4-byte big-endian payload size out of one of
 *          Docker's 8-byte frame headers, starting at `offset`.
 */
std::uint32_t frame_payload_size(std::string_view buf, std::size_t offset) {
    return (static_cast<std::uint8_t>(buf[offset + 4]) << 24) |
           (static_cast<std::uint8_t>(buf[offset + 5]) << 16) |
           (static_cast<std::uint8_t>(buf[offset + 6]) << 8) |
            static_cast<std::uint8_t>(buf[offset + 7]);
}

} // namespace

std::string docker_client::api_prefix() {
    if (const char* version = std::getenv("DOCKER_API_VERSION")) {
        std::string ver = version;
        if (not ver.empty() and ver.front() != 'v') {
            ver = "v" + ver;
        }
        return "/" + ver;
    }
    return fmt::format("/{}", default_api_version);
}

docker_client::docker_client(std::string socket_path)
    : m_socket(m_io)
{
    m_socket.connect(boost::asio::local::stream_protocol::endpoint{ socket_path });
}

std::string docker_client::request(const std::string& method, const std::string& target, const std::string& body) {
    http::request<http::string_body> req;
    req.method(http::string_to_verb(method));
    req.target(target);
    req.version(11);
    req.set(http::field::host, "docker");
    if (not body.empty()) {
        req.set(http::field::content_type, "application/json");
        req.body() = body;
        req.prepare_payload();
    }

    beast::flat_buffer buffer;
    http::response<http::string_body> res;

    http::write(m_socket, req);
    http::read(m_socket, buffer, res);

    return std::move(res.body());
}

nlohmann::json docker_client::request_json(const std::string& method, const std::string& target, const nlohmann::json& body) {
    const auto raw = request(method, target, body.empty() ? std::string{} : body.dump());
    try {
        return nlohmann::json::parse(raw);
    } catch (const nlohmann::json::parse_error&) {
        return {};
    }
}

bool docker_client::ping() {
    return request("GET", "/_ping") == "OK";
}

void docker_client::pull_image(std::string_view image) {
    const auto path = fmt::format("{}/images/create?fromImage={}", api_prefix(), image);
    const auto response = request_json("POST", path);
    nova::log::info("Pull response: {}", response.dump());
}

std::string docker_client::create_container(
    std::string_view image,
    const std::vector<std::string>& cmd,
    const container_config& cfg
) {
    std::vector<std::string> binds;
    binds.reserve(cfg.mounts.size());
    for (const auto& mount : cfg.mounts) {
        binds.push_back(fmt::format("{}:{}{}", mount.host, mount.container, mount.read_only ? ":ro" : ""));
    }

    nlohmann::json host_config = { { "AutoRemove", true } };
    if (not binds.empty()) {
        host_config["Binds"] = binds;
    }

    nlohmann::json body = {
        { "Image", image },
        { "Cmd", cmd },
        { "AttachStdout", true },
        { "AttachStderr", true },
        { "HostConfig", host_config },
    };

    if (cfg.user.has_value()) {
        body["User"] = *cfg.user;
    }

    const auto response = request_json("POST", api_prefix() + "/containers/create", body);
    if (not response.contains("Id")) {
        throw nova::exception("Container create failed: {}", response.dump());
    }
    return response["Id"].get<std::string>();
}

void docker_client::start_container(const std::string& id) {
    std::ignore = request("POST", fmt::format("{}/containers/{}/start", api_prefix(), id));
    nova::log::info("Started container: {}", id);
}

int docker_client::wait_container(const std::string& id) {
    const auto response = request_json("POST", fmt::format("{}/containers/{}/wait", api_prefix(), id));
    if (not response.contains("StatusCode")) {
        return EXIT_FAILURE;
    }
    return response["StatusCode"].get<int>();
}

void docker_client::stream_logs(const std::string& id, bool log_raw) {
    const auto path = fmt::format("{}/containers/{}/logs?stdout=1&stderr=1&follow=1", api_prefix(), id);

    http::request<http::empty_body> req{ http::verb::get, path, 11 };
    req.set(http::field::host, "docker");
    http::write(m_socket, req);

    beast::flat_buffer buffer;
    http::response_parser<http::buffer_body> parser;
    parser.body_limit(std::numeric_limits<std::uint64_t>::max());
    http::read_header(m_socket, buffer, parser);

    // Docker multiplexes stdout/stderr into a stream of frames, each
    // prefixed by an 8-byte header: 1 byte stream type, 3 reserved bytes,
    // and a 4-byte big-endian payload size. Frames can be split across
    // reads, so completed lines are emitted as soon as they're fully
    // buffered instead of waiting for the whole response to finish.
    std::string pending;
    std::array<char, 4096> chunk{};

    while (not parser.is_done()) {
        parser.get().body().data = chunk.data();
        parser.get().body().size = chunk.size();

        boost::system::error_code ec;
        http::read_some(m_socket, buffer, parser, ec);
        if (ec == http::error::need_buffer) {
            ec = {};
        } else if (ec) {
            break;
        }

        const auto bytes_read = chunk.size() - parser.get().body().size;
        pending.append(chunk.data(), bytes_read);

        std::size_t offset = 0;
        while (offset + 8 <= pending.size()) {
            const auto size = frame_payload_size(pending, offset);
            if (offset + 8 + size > pending.size()) {
                break;
            }

            auto content = std::string_view{ pending }.substr(offset + 8, size);
            if (content.ends_with('\n')) {
                content.remove_suffix(1);
            }

            if (log_raw) {
                std::cerr << ' ' << content << '\n';
            } else {
                nova::log::info("{}", content);
            }

            offset += 8 + size;
        }
        pending.erase(0, offset);
    }
}

int docker_run(
    std::string_view image,
    const std::vector<std::string>& cmd,
    const container_config& cfg
) {
    docker_client client;

    if (not client.ping()) {
        throw nova::exception("Docker daemon is not reachable.");
    }

    if (cfg.force_pull) {
        client.pull_image(image);
    }

    const auto id = client.create_container(image, cmd, cfg);
    client.start_container(id);

    if (cmd[0].contains("baldr")) {
        client.stream_logs(id, true);
    } else {
        client.stream_logs(id, false);
    }

    const int status = client.wait_container(id);
    if (status == 0) {
        utl::rlog::success("Container exited successfully.");
    } else {
        utl::rlog::failure(fmt::format("Container exited with code {}.", status));
    }
    return status;
}

} // namespace baldr
