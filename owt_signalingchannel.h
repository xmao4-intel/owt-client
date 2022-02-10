#ifndef _OWT_SIGNALING_CHANNEL_H
#define _OWT_SIGNALING_CHANNEL_H

#include <vector>

#include "sio/sio_client.h"
#include "owt/p2p/p2psignalingchannelinterface.h"

using namespace owt::p2p;

class OwtSignalingChannel : public P2PSignalingChannelInterface {
 public:
  explicit OwtSignalingChannel();
  virtual void AddObserver(P2PSignalingChannelObserver& observer) override;
  virtual void RemoveObserver(P2PSignalingChannelObserver& observer) override;
  virtual void Connect(
      const std::string& host,
      const std::string& token,
      std::function<void(const std::string&)> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) override;
  virtual void Disconnect(
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) override;
  virtual void SendMessage(
      const std::string& message,
      const std::string& target_id,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) override;

 private:
  std::vector<P2PSignalingChannelObserver*> observers_;
  std::unique_ptr<sio::client> io_;
  const uint32_t MAX_MESSAGE_SIZE = 1073741824;
};

#endif  // _OWT_SIGNALING_CHANNEL_H
