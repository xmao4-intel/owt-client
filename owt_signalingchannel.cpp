#include <algorithm>
#include <iostream>

#include "owt_signalingchannel.h"

OwtSignalingChannel::OwtSignalingChannel() : io_(new sio::client()) {}

void OwtSignalingChannel::AddObserver(P2PSignalingChannelObserver& observer) {
  observers_.push_back(&observer);
}

void OwtSignalingChannel::RemoveObserver(
    P2PSignalingChannelObserver& observer) {
  observers_.erase(remove(observers_.begin(), observers_.end(), &observer),
                   observers_.end());
}

void OwtSignalingChannel::Connect(
    const std::string& host,
    const std::string& token,
    std::function<void(const std::string&)> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  std::map<std::string, std::string> query;
  query.insert(std::pair<std::string, std::string>("clientVersion", "4.2"));
  query.insert(std::pair<std::string, std::string>("clientType", "cpp"));
  query.insert(std::pair<std::string, std::string>(
      "token", token));  // TODO: parse token to get actual token.
  sio::socket::ptr socket = io_->socket();
  std::string name = "owt-message";
  socket->on(
      name,
      sio::socket::event_listener_aux(
          [&](std::string const& name, sio::message::ptr const& data,
              bool has_ack, sio::message::list& ack_resp) {
            if (data->get_flag() == sio::message::flag_object) {
              std::string msg = data->get_map()["data"]->get_string().data();
              std::string from = data->get_map()["from"]->get_string().data();
              uint32_t len = msg.length() + from.length();
              if (len > MAX_MESSAGE_SIZE) {
                  printf("The received message size %u is too large.\n", len);
                  return;
              }

              for (auto it = observers_.begin(); it != observers_.end(); ++it) {
                printf("from %s, received msg: %s\n", from.c_str(), msg.c_str());
                (*it)->OnSignalingMessage(msg, from);
              };
            }
          }));
  io_->connect(host, query);
}

void OwtSignalingChannel::Disconnect(
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {}

void OwtSignalingChannel::SendMessage(
    const std::string& message,
    const std::string& target_id,
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  printf("message: %s\n", message.c_str());
  sio::message::ptr jsonObject = sio::object_message::create();
  jsonObject->get_map()["to"] = sio::string_message::create(target_id);
  jsonObject->get_map()["data"] = sio::string_message::create(message);
  io_->socket()->emit("owt-message", jsonObject,
                      [=](const sio::message::list& msg) {
                        if (on_success) {
                          on_success();
                        }
                      });
}
