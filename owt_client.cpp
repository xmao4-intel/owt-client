
#include <string>
#include <iostream>
#include <unistd.h>
#include <SDL2/SDL.h>

#include "owt/base/stream.h"
#include "owt/base/videorendererinterface.h"
#include "owt/base/audioplayerinterface.h"
#include "owt/p2p/p2pclient.h"

#include "owt_signalingchannel.h"

using namespace owt::p2p;

class VideoRenderer : public owt::base::VideoRendererInterface {
 public:
  VideoRenderer(SDL_Window* w): win_(w) {
    std::cout << __func__ << ":" << std::endl;
    init();
  }
  virtual ~VideoRenderer() {
    std::cout << __func__ << ":" << std::endl;
    if (renderer_) {
      SDL_DestroyTexture(texture_);
      SDL_DestroyRenderer(renderer_);
    }
  }
  void RenderFrame(std::unique_ptr<VideoBuffer> buffer) override {
    if (!renderer_)
      return;

    if (buffer->type == VideoBufferType::kI420) {
      uint32_t w = buffer->resolution.width;
      uint8_t *y = buffer->buffer;
      uint8_t *u = y + buffer->resolution.width * buffer->resolution.height;
      uint8_t *v = u + buffer->resolution.width * buffer->resolution.height / 4;
      SDL_UpdateYUVTexture(texture_, nullptr, y, w, u, w / 2, v, w / 2);
    } else {
      SDL_UpdateTexture(texture_, nullptr, buffer->buffer, buffer->resolution.width * 4);
    }
    SDL_RenderCopy(renderer_, texture_, NULL, NULL);
    SDL_RenderPresent(renderer_);
  };
  VideoRendererType Type() override {
    return VideoRendererType::kI420;
  }

 private:
    void init() {
      std::cout << __func__ << ":" << std::endl;
      renderer_ = SDL_CreateRenderer(win_, -1, 0);
      if (!renderer_) {
          std::cout << "Failed to create renderer" << std::endl;
          return;
      }
      texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, 1280, 720);
    }
 private:
    SDL_Window* win_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
};

class AudioPlayer : public owt::base::AudioPlayerInterface {
 public:
  AudioPlayer() {
      std::cout << __func__ << ":" << std::endl;
      init();
  }
  virtual ~AudioPlayer() {
      std::cout << __func__ << ":" << std::endl;
      if (auddev_ != -1) {
        SDL_CloseAudioDevice(auddev_);
      }
  }
  void OnData(const void *audio_data, int bits_per_sample,
                      int sample_rate, size_t number_of_channels,
                      size_t number_of_frames) override {
    //std::cout << __func__ << ":" << " bits:" << bits_per_sample << ",sample rate:" << sample_rate <<
    //    ",channels:" << number_of_channels << ",frames:" << number_of_frames << std::endl;
    if (auddev_ != -1) {
      SDL_QueueAudio(auddev_, audio_data, number_of_frames * number_of_channels * bits_per_sample / 8);
    }
  }

 private:
  void init() {
    SDL_AudioSpec want, have;
    auddev_ = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (auddev_== -1) {
        std::cout << "Failed to open audio device" << std::endl;
        return;
    }
    SDL_PauseAudioDevice(auddev_, 0);
  }

 private:
  SDL_AudioDeviceID auddev_ = -1;
};

class PcObserver : public owt::p2p::P2PClientObserver {
public:
  PcObserver(SDL_Window* w): mVideoRenderer(w){}
  void OnMessageReceived(const std::string& remote_user_id,
                         const std::string message) {
    std::cout << __func__ << ":from" << remote_user_id << ", msg:" << message << std::endl;
  }
  virtual void OnStreamAdded(
      std::shared_ptr<owt::base::RemoteStream> stream) override {
          stream->AttachVideoRenderer(mVideoRenderer);
          stream->AttachAudioPlayer(mAudioPlayer);
  }
  void OnServerDisconnected() {
    std::cout << __func__ << ":" << std::endl;
  }
private:
    AudioPlayer mAudioPlayer;
    VideoRenderer mVideoRenderer;
};

int main(int argc, char* argv[]) {
  if (argc < 3) {
      std::cout << "usage:" << argv[0] << " <ip> <id>" << std::endl;
      exit(0);
  }

  std::string ip = argv[1];
  std::string id = argv[2];

  SDL_Init(SDL_INIT_VIDEO);
  atexit(SDL_Quit);

  std::string title = ip + ":" + id;
  auto win = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            1280, 720, SDL_WINDOW_RESIZABLE);
  if (!win) {
    std::cout << "Failed to create SDL window!" << std::endl;
    exit(0);
  }

  std::string url = "http://" + ip + ":8095";
  std::string serverId = "s" + id;
  std::string clientId =  "c" + id;

  P2PClientConfiguration configuration;
  IceServer stunServer, turnServer;
  std::string stunUrl = "stun:" + ip + ":3478";
  stunServer.urls.push_back(stunUrl);
  stunServer.username = "username";
  stunServer.password = "password";
  configuration.ice_servers.push_back(stunServer);

  std::string turnUrlTcp = "turn:" + ip + ":3478?transport=tcp";
  std::string turnUrlUdp = "turn:" + ip + ":3478?transport=udp";
  turnServer.urls.push_back(turnUrlTcp);
  turnServer.urls.push_back(turnUrlUdp);
  turnServer.username = "username";
  turnServer.password = "password";
  configuration.ice_servers.push_back(turnServer);

  VideoCodecParameters videoParam;
  videoParam.name = owt::base::VideoCodec::kH264;

  auto sc = std::make_shared<OwtSignalingChannel>();
  auto pc = std::make_shared<P2PClient>(configuration, sc);

  PcObserver ob(win);
  pc->AddObserver(ob);
  pc->AddAllowedRemoteId(serverId);
  pc->Connect(url, clientId, nullptr, nullptr);
  pc->Send(serverId, "start", nullptr, nullptr);

  auto sendCtrl = [&](const char* event, const char* param) {
    char msg[256];

    snprintf(msg, 256, "{\"type\": \"control\", \"data\": { \"event\": \"%s\", \"parameters\": %s }}", event, param);
    //std::cout << "sendCtl: " <<  msg << std::endl;
    pc->Send(serverId, msg, nullptr, nullptr);
  };

  auto onMouseMove = [&](SDL_MouseMotionEvent& e) {
      if (e.state == 1) {
          char param[64];
          int w, h;
          SDL_GetWindowSize(win, &w, &h);

          int x = e.x * 32767 / w;
          int y = e.y * 32767 / h;
          snprintf(param, 64, "{\"x\": %d, \"y\": %d, \"movementX\": %d, \"movementY\": %d }", x, y, e.xrel, e.yrel);
          sendCtrl("mousemove", param);
      }
  };

  auto onMouseButton = [&](SDL_MouseButtonEvent& e) {
      char param[64];
      const char* et = (e.type == SDL_MOUSEBUTTONDOWN) ? "mousedown" : "mouseup";
      int w, h;

      SDL_GetWindowSize(win, &w, &h);

      int x = e.x * 32767 / w;
      int y = e.y * 32767 / h;
      snprintf(param, 64, "{\"which\": %d, \"x\": %d, \"y\": %d }", e.which, x, y);
      sendCtrl(et, param);
  };

  bool fullscreen = false;
  bool running = true;
  while (running) {
      SDL_Event e;
      while(SDL_PollEvent(&e)) {
          switch (e.type) {
              case SDL_QUIT:
              running = false;
              break;
              case SDL_MOUSEBUTTONDOWN:
              case SDL_MOUSEBUTTONUP:
              onMouseButton(e.button);
              break;
              case SDL_MOUSEMOTION:
              onMouseMove(e.motion);
              break;
              case SDL_KEYDOWN: {
                if (e.key.keysym.sym == SDLK_F11) {
                  uint32_t flags = fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
                  SDL_SetWindowFullscreen(win, flags);
                  fullscreen = !fullscreen;
                }
              }
              break;
              case SDL_WINDOWEVENT:
              break;
              default:
              std::cout << "Unhandled SDL event " << e.type << std::endl;
              break;
         }
      }
    }

    pc->Stop(serverId, nullptr, nullptr);
    pc->RemoveObserver(ob);
    pc->Disconnect(nullptr, nullptr);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}