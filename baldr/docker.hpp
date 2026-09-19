/**
 * Part of Baldr
 *
 * A minimal Docker Engine API client, used to build/run projects inside a
 * container.
 *
 * @author      Claude Sonnet 5 (Junie)
 * @date        2026-07-12
 */

#pragma once

#include <nlohmann/json.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace baldr {

/**
 * @brief   A single host-directory-into-container bind mount.
 */
struct bind_mount {
    std::string host;
    std::string container;
    bool read_only = false;
};

struct container_config {
    std::vector<bind_mount> mounts;
    std::optional<std::string> user;
    bool auto_remove = true;
    bool force_pull = false;
};

/**
 * @brief   Minimal HTTP/1.1 client speaking to the Docker Engine API over
 *          its Unix domain socket.
 */
class docker_client {
public:
    static constexpr auto default_socket = "/var/run/docker.sock";

    /**
     * @brief   Construct a client and connect it to `socket_path`.
     */
    explicit docker_client(std::string socket_path = default_socket);

    /**
     * @brief   Check that the Docker daemon is reachable and responsive.
     */
    [[nodiscard]] bool ping();

    /**
     * @brief   Pull `image` from its configured registry.
     */
    void pull_image(std::string_view image);

    /**
     * @brief   Create a container running `cmd` inside `image`.
     *
     * @param   mounts  Host directories to bind-mount into the container.
     * @param   user    If set, run the container's process as this
     *                  `uid[:gid]` (Docker Engine API `HostConfig.User`)
     *                  instead of the image's default user.
     *
     * @return  The created container's ID.
     *
     * @throws  nova::exception if the daemon didn't return a container ID.
     */
    [[nodiscard]] std::string create_container(
        std::string_view image,
        const std::vector<std::string>& cmd,
        const container_config& cfg
    );

    /**
     * @brief   Start the container identified by `id`.
     */
    void start_container(const std::string& id);

    /**
     * @brief   Block until the container identified by `id` exits.
     *
     * @return  The container's exit code.
     */
    [[nodiscard]] int wait_container(const std::string& id);

    /**
     * @brief   Stream the container's combined stdout/stderr.
     *
     * Forwards each line to `nova::log::info` if not `log_raw`.
     */
    void stream_logs(const std::string& id, bool log_raw);

private:
    /**
     * @brief   Send a raw HTTP request to the daemon and return its body.
     */
    [[nodiscard]] std::string request(const std::string& method, const std::string& target, const std::string& body = {});

    /**
     * @brief   Send a JSON request to the daemon and parse its response
     *          body, returning an empty object if it wasn't valid JSON.
     */
    [[nodiscard]] nlohmann::json request_json(const std::string& method, const std::string& target, const nlohmann::json& body = {});

    /**
     * @brief   API version path prefix, e.g. `/v1.43`; overridable via the
     *          `DOCKER_API_VERSION` environment variable.
     */
    [[nodiscard]] static std::string api_prefix();

    boost::asio::io_context m_io;
    boost::asio::local::stream_protocol::socket m_socket;
};

/**
 * @brief   Pull `image`, run `cmd` inside a fresh, auto-removed container,
 *          stream its output via `nova::log::info`, and return its exit
 *          code.
 *
 * @param   mounts  Host directories to bind-mount into the container.
 * @param   user    If set, run the container's process as this
 *                  `uid[:gid]` instead of the image's default user.
 *
 * @throws  nova::exception if the daemon is unreachable or container
 *          creation fails.
 */
[[nodiscard]] int docker_run(
    std::string_view image,
    const std::vector<std::string>& cmd,
    const container_config& cfg
);

} // namespace baldr
