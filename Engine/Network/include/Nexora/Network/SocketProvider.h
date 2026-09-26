#pragma once

#include "Nexora/Network/Api.h"
#include "Nexora/Network/Transport.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace nexora::network {

struct SocketEndpoint {
  std::string host;
  std::uint16_t port{};

  bool operator==(const SocketEndpoint &) const = default;
};

struct SocketDatagram {
  SocketEndpoint peer;
  std::vector<std::byte> bytes;
};

class NEXORA_NETWORK_API IDatagramSocket {
public:
  virtual ~IDatagramSocket() = default;
  [[nodiscard]] virtual bool Bind(const SocketEndpoint &local) = 0;
  [[nodiscard]] virtual bool SendTo(const SocketEndpoint &peer,
                                    std::span<const std::byte> bytes) = 0;
  [[nodiscard]] virtual bool Poll(SocketDatagram &datagram) = 0;
  virtual void Close() = 0;
  [[nodiscard]] virtual bool IsOpen() const noexcept = 0;
};

// Providers own platform initialization. Sockets returned from CreateDatagramSocket remain owned by
// the caller and must not outlive their provider. Native UDP and DTLS providers are backend gates.
class NEXORA_NETWORK_API ISocketProvider {
public:
  virtual ~ISocketProvider() = default;
  [[nodiscard]] virtual std::unique_ptr<IDatagramSocket> CreateDatagramSocket() = 0;
};

} // namespace nexora::network
